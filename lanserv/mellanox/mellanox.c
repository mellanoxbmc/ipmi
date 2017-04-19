/*
 * mellanox.c
 *
 * Mellanox specific module for handling MC functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>

#include <OpenIPMI/ipmi_err.h>
#include <OpenIPMI/ipmi_msgbits.h>
#include <OpenIPMI/ipmi_bits.h>
#include <OpenIPMI/ipmi_mc.h>
#include <OpenIPMI/serv.h>
#include "../bmc.h"

static lmc_data_t *bmc_mc;

/**************************************************************************
 *                  Mellanox custom commands codes                        *
 *************************************************************************/
/* IPMI_SENSOR_EVENT_NETFN (0x04) */
#define IPMI_OEM_MLX_SET_FAN_SPEED_CMD      0x30
#define IPMI_OEM_MLX_GET_LED_STATE_CMD      0x31
#define IPMI_OEM_MLX_SET_LED_STATE_CMD      0x32
#define IPMI_OEM_MLX_SET_LED_BLINKING_CMD   0x33
#define IPMI_OEM_MLX_GET_FAN_PWM_CMD        0x34

/* IPMI_APP_NETFN (0x06) */
#define IPMI_OEM_MLX_SEL_BUFFER_SET_CMD     0x5B
#define IPMI_OEM_MLX_CPU_HARD_RESET_CMD     0x5e
#define IPMI_OEM_MLX_CPU_SOFT_RESET_CMD     0x5f
#define IPMI_OEM_MLX_RESET_PHY_CMD          0x60
#define IPMI_OEM_MLX_SET_UART_TO_BMC_CMD    0x61

#define IPMI_OEM_MLX_SEL_LOG_SIZE_MIN       0x40
#define IPMI_OEM_MLX_SEL_LOG_SIZE_MAX       0x0fff

/* Set sel log size script file path */
#define SEL_SET_SCRIPT_NAME "sel_set_log_size.sh"

/**  FAN FUNCTIONALITY DEFINES  **/
#define MLX_FAN_MAX   8
#define MLX_FAN_TACHO_FILE  "/bsp/fan/tacho"
#define MLX_FAN_PWM_FILE            "/bsp/fan/pwm"
#define MLX_FAN_PWM_ENABLE_FILE     "/bsp/fan/pwm_en"
#define MLX_PWM_MIN                 0
#define MLX_PWM_MAX                 9

static const char* fan_tacho_en[MLX_FAN_MAX] =
{
    MLX_FAN_TACHO_FILE"1_en",
    MLX_FAN_TACHO_FILE"2_en",
    MLX_FAN_TACHO_FILE"3_en",
    MLX_FAN_TACHO_FILE"4_en",
    MLX_FAN_TACHO_FILE"5_en",
    MLX_FAN_TACHO_FILE"6_en",
    MLX_FAN_TACHO_FILE"7_en",
    MLX_FAN_TACHO_FILE"8_en"
};

/**  LED FUNCTIONALITY DEFINES  **/
#define MLX_STATUS_LED_MAX 1
#define MLX_PSU_LED_MAX    1
#define MLX_FAN_LED_MAX    4
#define MLX_MAX_LEDS       MLX_FAN_LED_MAX + MLX_PSU_LED_MAX + MLX_STATUS_LED_MAX

#define LED_FAN_FILE "/bsp/leds/fan/"
#define LED_STATUS_FILE "/bsp/leds/status/"
#define LED_PSU_FILE "/bsp/leds/psu/"
#define LED_BRIGHTNESS "brightness"
#define LED_TRIGGER    "trigger"

#define LED_COLOR_RED    1
#define LED_COLOR_GREEN  2
#define LED_COLOR_AMBER  3

static const char* red_led[MLX_MAX_LEDS] =
{
    LED_STATUS_FILE"red/",
    LED_PSU_FILE"red/",
    LED_FAN_FILE"red/1/",
    LED_FAN_FILE"red/2/",
    LED_FAN_FILE"red/3/",
    LED_FAN_FILE"red/4/",
};

static const char* green_led[MLX_MAX_LEDS] =
{
    LED_STATUS_FILE"green/",
    LED_PSU_FILE"green/",
    LED_FAN_FILE"green/1/",
    LED_FAN_FILE"green/2/",
    LED_FAN_FILE"green/3/",
    LED_FAN_FILE"green/4/",
};

static const char* amber_led[MLX_STATUS_LED_MAX] =
{
    LED_STATUS_FILE"amber/"
};

/* Reset links */
#define MLX_BMC_SOFT_RESET   "/bsp/reset/bmc_reset_soft"
#define MLX_CPU_HARD_RESET   "/bsp/reset/cpu_reset_hard"
#define MLX_CPU_SOFT_RESET   "/bsp/reset/cpu_reset_soft"
#define MLX_SYS_HARD_RESET   "/bsp/reset/system_reset_hard"
#define MLX_RESET_PHY        "/bsp/reset/reset_phy"

#define MLX_UART_TO_BMC      "/bsp/reset/uart_sel"


static unsigned char set_fan_enable(const char* fname)
{
    FILE *f_en;

    f_en = fopen(fname, "w");

    if (!f_en) {
            printf("\nUnable to open %s file", fname);
            return IPMI_DESTINATION_UNAVAILABLE_CC;
    } else
        fprintf(f_en, "%u", 1);

    fclose(f_en);

    return 0;
}

/**
 *
 * ipmitool raw 0x04  0x030  Speed (0x00-0xFF)
 *
**/
static void
handle_set_fan_speed_cmd (lmc_data_t    *mc,
                msg_t         *msg,
                unsigned char *rdata,
                unsigned int  *rdata_len,
                void          *cb_data)
{
    FILE *f_pwm;
    unsigned char pwm = 0;

    pwm = msg->data[0];
    if (pwm > MLX_PWM_MAX) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    f_pwm = fopen(MLX_FAN_PWM_FILE, "w");

    if (!f_pwm) {
            printf("\nUnable to open pwm file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        if (IPMI_DESTINATION_UNAVAILABLE_CC == set_fan_enable(MLX_FAN_PWM_ENABLE_FILE)) {
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else
        fprintf(f_pwm, "%u", pwm);
    }

    fclose(f_pwm);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 * ipmitool raw 0x04  0x34
 *
**/
static void
handle_get_fan_pwm_cmd(lmc_data_t    *mc,
                       msg_t         *msg,
                       unsigned char *rdata,
                       unsigned int  *rdata_len,
                       void          *cb_data)
{
    unsigned char rv = 0;
    char line_pwm[10];
    int pwm;
    FILE *fpwm;

    fpwm = fopen(MLX_FAN_PWM_FILE, "r");

    if (!fpwm) {
        printf("\nUnable to open  PWM file");
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
	return;
    }

    if (0 >= fread(line_pwm, 1, sizeof(line_pwm),fpwm))
    {
        fclose(fpwm);
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
	return;
    }

    pwm = strtoul(line_pwm, NULL, 0);

    fclose(fpwm);

    rdata[0] = 0;
    rdata[1] = pwm;
    *rdata_len = 2;
    return;
}

/**
 *
 * ipmitool raw 0x04  0x032  LedNum  Color
 *
 * LedNum:
 * 0x0 - status LED
 * 0x1 - PSU LED
 * 0x2 - FAN1 LED
 * 0x3 - FAN2 LED
 * 0x4 - FAN2 LED
 * 0x5 - FAN2 LED
 *
 * Color:
 * 0x1 - red 0x2 - green 0x3 - amber
**/
static void
handle_set_led_state(lmc_data_t    *mc,
				   msg_t         *msg,
				   unsigned char *rdata,
				   unsigned int  *rdata_len,
				   void          *cb_data)
{
    unsigned int led;
    FILE *f_green;
    FILE *f_red;
    FILE *f_amber;
    char fname[100];

    if (check_msg_length(msg, 2, rdata, rdata_len))
	return;

    led = msg->data[0];
    if (led >= MLX_MAX_LEDS) {
	rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
	*rdata_len = 1;
	return;
    }

    if (led < MLX_STATUS_LED_MAX ) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%s", amber_led[led],LED_BRIGHTNESS);
        f_amber = fopen(fname, "w");
        if (!f_amber) {
            printf("\nUnable to open LED status file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
        }
    }

    memset(fname, 0, sizeof(fname));
    sprintf(fname, "%s%s", green_led[led],LED_BRIGHTNESS);
    f_green = fopen(fname, "w");

    memset(fname, 0, sizeof(fname));
    sprintf(fname, "%s%s", red_led[led],LED_BRIGHTNESS);
    f_red = fopen(fname, "w");

    if (!f_red || !f_green) {
        printf("\nUnable to open LED status file");
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
        if (led < MLX_STATUS_LED_MAX)
            fclose(f_amber);
        if (f_red)
            fclose(f_red);
        if (f_green)
            fclose(f_green);
        return;
    } else {
        if (msg->data[1] == LED_COLOR_AMBER &&
            led < MLX_STATUS_LED_MAX) { // only status led support AMBER
            fprintf(f_red, "%u", 0);
            fprintf(f_green, "%u", 0);
            fprintf(f_amber, "%u", 1);
        }
        else if (msg->data[1] == LED_COLOR_RED) { //RED
            if (led < MLX_STATUS_LED_MAX)
                fprintf(f_amber, "%u", 0);
            fprintf(f_green, "%u", 0);
            fprintf(f_red, "%u", 1);
        }
        else if (msg->data[1] == LED_COLOR_GREEN) { //GREEN
            if (led < MLX_STATUS_LED_MAX)
                fprintf(f_amber, "%u", 0);
            fprintf(f_red, "%u", 0);
            fprintf(f_green, "%u", 1);
        }
        else {
            rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
            *rdata_len = 1;
            if (led < MLX_STATUS_LED_MAX)
                fclose(f_amber);
            fclose(f_green);
            fclose(f_red);
            return;
        }

        fclose(f_green);
        fclose(f_red);
        if (led < MLX_STATUS_LED_MAX)
            fclose(f_amber);
    }

    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 * ipmitool raw 0x04  0x033  LedNum  Color Time
 *
 * LedNum:
 * 0x0 - status LED
 * 0x1 - PSU LED
 * 0x2 - FAN1 LED
 * 0x3 - FAN2 LED
 * 0x4 - FAN2 LED
 * 0x5 - FAN2 LED
 *
 * Color
 * 0x1 - red
 * 0x2 - green
 * 0x3 - amber
**/
static void
handle_set_led_blinking (lmc_data_t    *mc,
                msg_t         *msg,
                unsigned char *rdata,
                unsigned int  *rdata_len,
                void          *cb_data)
{
    unsigned int led;
    unsigned int time;
    FILE *f_trigger;
    char fname[100];

    if (check_msg_length(msg, 3, rdata, rdata_len))
	return;

    led = msg->data[0];
    if (led >= MLX_MAX_LEDS) {
	rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
	*rdata_len = 1;
	return;
    }

    if (msg->data[1] == LED_COLOR_AMBER && led == 0) { // only status led support AMBER
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%s", amber_led[led],LED_TRIGGER);
        f_trigger = fopen(fname, "w");
    }
    else if (msg->data[1] == LED_COLOR_RED) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%s", red_led[led],LED_TRIGGER);
        f_trigger = fopen(fname, "w");
    } else if (msg->data[1] == LED_COLOR_GREEN) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%s", green_led[led],LED_TRIGGER);
        f_trigger = fopen(fname, "w");
    }
    else {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    if (!f_trigger) {
        printf("\nUnable to open LED trigger file");
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
        return;
    }

    time = msg->data[2];
    fprintf(f_trigger, "%u", time);

    fclose(f_trigger);

    rdata[0] = 0;
    *rdata_len = 1;
}


static unsigned char get_led_color(unsigned int led, unsigned char *color)
{
    FILE *f_green;
    FILE *f_red;
    FILE *f_amber;
    char line_green[10];
    char line_red[10];
    char line_amber[10];
    int red, green, amber;
    char fname[100];

    if (led < MLX_STATUS_LED_MAX) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%s", amber_led[led],LED_BRIGHTNESS);

        f_amber = fopen(fname, "r");
        if (!f_amber) {
            printf("\nUnable to open LED status file");
            return IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        }

        if (0 >= fread(line_amber, 1, sizeof(line_amber),f_amber)) {
            fclose(f_amber);
            return IPMI_INVALID_DATA_FIELD_CC;
        }

        amber = strtoul(line_amber, NULL, 0);
    }

    memset(fname, 0, sizeof(fname));
    sprintf(fname, "%s%s", green_led[led],LED_BRIGHTNESS);
    f_green = fopen(fname, "r");

    memset(fname, 0, sizeof(fname));
    sprintf(fname, "%s%s", red_led[led],LED_BRIGHTNESS);
    f_red = fopen(fname, "r");

    if (!f_green || !f_red) {
        printf("\nUnable to open LED status file");
        if (led < MLX_STATUS_LED_MAX)
            fclose(f_amber);
        if (f_red)
            fclose(f_red);
        if (f_green)
            fclose(f_green);

        return IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
    }

    if ((0 >= fread(line_green, 1, sizeof(line_green),f_green))||
         (0 >= fread(line_red, 1, sizeof(line_red),f_red)))
    {
        if (led < MLX_STATUS_LED_MAX)
            fclose(f_amber);
        fclose(f_green);
        fclose(f_red);
        return IPMI_INVALID_DATA_FIELD_CC;
    }

    red = strtoul(line_red, NULL, 0);
    green = strtoul(line_green, NULL, 0);

    if (led < MLX_STATUS_LED_MAX) {
        if (amber && !green && !red)
            *color = LED_COLOR_AMBER;
        else if (!amber && green && !red)
            *color = LED_COLOR_GREEN;
        else
            *color = LED_COLOR_RED;

        fclose(f_amber);
    }
    else {
        if (!red && green)
            *color = LED_COLOR_GREEN;
        else
            *color = LED_COLOR_RED;
    }

    fclose(f_green);
    fclose(f_red);

    return 0;
}

/**
 *
 * ipmitool raw 0x04  0x031  LedNum
 *
 * LedNum:
 * 0x0 - status LED
 * 0x1 - PSU LED
 * 0x2 - FAN1 LED
 * 0x3 - FAN2 LED
 * 0x4 - FAN2 LED
 * 0x5 - FAN2 LED
 *
**/
static void
handle_get_led_state(lmc_data_t    *mc,
				   msg_t         *msg,
				   unsigned char *rdata,
				   unsigned int  *rdata_len,
				   void          *cb_data)
{
    unsigned int led;
    unsigned char color = LED_COLOR_RED;
    unsigned char rv = 0;

    if (check_msg_length(msg, 1, rdata, rdata_len))
	return;

    led = msg->data[0];
    if (led >= MLX_MAX_LEDS) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    rdata[0] = 0;
    rdata[1] = 0;

    rv = get_led_color(led, &color);

    if (rv) {
        rdata[0] = rv;
        *rdata_len = 1;
        return;
    }

    rdata[1] = color;
    *rdata_len = 2;
    return;
}

/**
 *
 *  ipmitool raw 0x06  0x02
 *
 **/
static void
handle_bmc_cold_reset(lmc_data_t    *mc,
			  msg_t         *msg,
			  unsigned char *rdata,
			  unsigned int  *rdata_len,
			  void          *cb_data)
{
    printf("\n %d: %s, %s(); %x %X", __LINE__, __FILE__, __FUNCTION__, msg->data[0], msg->data[1]);

    FILE *freset;

    freset = fopen(MLX_BMC_SOFT_RESET, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(freset, "%u", 0);
    }

    fclose(freset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x5e
 *
 **/
static void
handle_cpu_hard_reset(lmc_data_t    *mc,
			  msg_t         *msg,
			  unsigned char *rdata,
			  unsigned int  *rdata_len,
			  void          *cb_data)
{
    printf("\n %d: %s, %s()", __LINE__, __FILE__, __FUNCTION__);

    FILE *freset;
    unsigned int reset;

    if (check_msg_length(msg, 1, rdata, rdata_len)) {
        reset = 0;
    }
    else {
        reset = msg->data[0];
        if (reset != 1 ) {
            rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
            *rdata_len = 1;
            return;
        }
    }

    freset = fopen(MLX_CPU_HARD_RESET, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(freset, "%u", reset);
    }

    fclose(freset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x5f
 *
 **/
static void
handle_cpu_soft_reset(lmc_data_t    *mc,
			  msg_t         *msg,
			  unsigned char *rdata,
			  unsigned int  *rdata_len,
			  void          *cb_data)
{
    printf("\n %d: %s, %s()", __LINE__, __FILE__, __FUNCTION__);

    FILE *freset;

    freset = fopen(MLX_CPU_SOFT_RESET, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(freset, "%u", 0);
    }

    fclose(freset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x60
 *
 **/
static void
handle_reset_phy(lmc_data_t    *mc,
                     msg_t         *msg,
                     unsigned char *rdata,
                     unsigned int  *rdata_len,
                     void          *cb_data)
{
    printf("\n %d: %s, %s()", __LINE__, __FILE__, __FUNCTION__);

    FILE *freset;

    freset = fopen(MLX_RESET_PHY, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(freset, "%u", 0);
    }

    fclose(freset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x61 (BMC - no value)
 *  ipmitool raw 0x06  0x61 1 (CPU)
 *
 **/
static void
handle_set_uart_to_bmc(lmc_data_t    *mc,
                     msg_t         *msg,
                     unsigned char *rdata,
                     unsigned int  *rdata_len,
                     void          *cb_data)
{
    printf("\n %d: %s, %s()", __LINE__, __FILE__, __FUNCTION__);

    FILE *fset;
    unsigned int uart;

    if (check_msg_length(msg, 1, rdata, rdata_len)) {
        uart = 0;
    }
    else {
        uart = msg->data[0];
        if (uart != 1 ) {
            rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
            *rdata_len = 1;
            return;
        }
    }

    fset = fopen(MLX_UART_TO_BMC, "w");

    if (!fset) {
            printf("\nUnable to open file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(fset, "%u", uart);
    }

    fclose(fset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x5B size_LSB size_MSB
 *
 **/
static void
handle_sel_buffer_set(lmc_data_t    *mc,
              msg_t         *msg,
              unsigned char *rdata,
              unsigned int  *rdata_len,
              void          *cb_data)
{
    uint16_t max_entries = 0;
    char sel_set_cmd_buf[100];

    if (check_msg_length(msg, 2, rdata, rdata_len)){
        return;
    }

    max_entries = msg->data[0] | (((uint16_t) msg->data[1]) << 8);

    if ((max_entries < IPMI_OEM_MLX_SEL_LOG_SIZE_MIN) ||
        (max_entries > IPMI_OEM_MLX_SEL_LOG_SIZE_MAX)){
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    snprintf(sel_set_cmd_buf, sizeof(sel_set_cmd_buf), "%s %d", SEL_SET_SCRIPT_NAME, max_entries);
    rdata[0] = system(sel_set_cmd_buf);

    ipmi_mc_enable_sel(mc, max_entries, mc->sel.flags);
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x04  0x15
 *
 **/
static void handle_get_last_processed_event(lmc_data_t    *mc,
              msg_t         *msg,
              unsigned char *rdata,
              unsigned int  *rdata_len,
              void          *cb_data)
{
    sel_entry_t *entry;
    sel_entry_t *p_entry = NULL;
    int         offset;
    int         count;

    if (mc->sel.count == 0) {
        rdata[0] = IPMI_NOT_PRESENT_CC;
        *rdata_len = 1;
        return;
    }

    entry = mc->sel.entries;
    if (entry) {
        while (entry->next) {
            p_entry = entry;
            entry = entry->next;
            }
    }

    rdata[0] = 0;
    rdata[1] = 0xff;
    rdata[2] = 0xff;

    ipmi_set_uint16(rdata+4, entry->record_id);
    *rdata_len = 3;

    offset = 0x00;
    count = 0xff;

    if ((offset+count) > 16)
        count = 16 - offset;
    memcpy(rdata+5, entry->data+offset, count);
    *rdata_len = count + 5;
}

/*
 * Chassis control for the chassis
 */
static int
bmc_set_chassis_control(lmc_data_t *mc, int op, unsigned char *val,
			void *cb_data)
{
    FILE *freset;

    switch (op) {
    case CHASSIS_CONTROL_POWER:
    case CHASSIS_CONTROL_BOOT_INFO_ACK:
    case CHASSIS_CONTROL_BOOT:
    case CHASSIS_CONTROL_GRACEFUL_SHUTDOWN:
        break;
    case CHASSIS_CONTROL_RESET:
        freset = fopen(MLX_SYS_HARD_RESET, "w");

        if (!freset) {
                return ETXTBSY;
        } else {
            fprintf(freset, "%u", 0);
        }

        fclose(freset);
        break;
    default:
	return EINVAL;
    }

    return 0;
}

/*
 * Chassis control get for the chassis.
 */
static int
bmc_get_chassis_control(lmc_data_t *mc, int op, unsigned char *val,
			void *cb_data)
{
    *val = 1;
    return 0;
}

int
ipmi_sim_module_print_version(sys_data_t *sys, char *initstr)
{
    printf("IPMI Simulator Mellanox module version 0.1\n");
    return 0;
}

/**************************************************************************
 * Module initialization
 *************************************************************************/
/**
 *
 * This is used to initialize the module.  It is called after the
 * configuration has been read from the LAN configuration file, but
 * before the *.hw commands are run.
 *
 **/
int
ipmi_sim_module_init(sys_data_t *sys, const char *initstr_i)
{
    int rv;
    unsigned int i;

    printf("IPMI Mellanox module");

    rv = ipmi_mc_alloc_unconfigured(sys, 0x20, &bmc_mc);
    if (rv) {
	sys->log(sys, OS_ERROR, NULL,
		 "Unable to allocate an mc: %s", strerror(rv));
	return rv;
    }

    rv = set_fan_enable(MLX_FAN_PWM_ENABLE_FILE);

    if (rv) {
        sys->log(sys, OS_ERROR, NULL,
                 "Unable to enable pwm_en: %s", strerror(rv));
    }

    for (i = 0; i < MLX_FAN_MAX; ++i) {
        rv = set_fan_enable(fan_tacho_en[i]);

        if (rv) {
            sys->log(sys, OS_ERROR, NULL,
                     "Unable to enable tacho for FAN%i: %s", i, strerror(rv));
        }
    }

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_SET_FAN_SPEED_CMD,
                                       handle_set_fan_speed_cmd, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_GET_FAN_PWM_CMD,
                                       handle_get_fan_pwm_cmd, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_GET_LED_STATE_CMD,
                                       handle_get_led_state, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_SET_LED_STATE_CMD,
                                       handle_set_led_state, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_SET_LED_BLINKING_CMD,
                                       handle_set_led_blinking, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_COLD_RESET_CMD,
                                       handle_bmc_cold_reset, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_CPU_HARD_RESET_CMD,
                                       handle_cpu_hard_reset, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_CPU_SOFT_RESET_CMD,
                                       handle_cpu_soft_reset, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_RESET_PHY_CMD,
                                       handle_reset_phy, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_SET_UART_TO_BMC_CMD,
                                       handle_set_uart_to_bmc, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_SEL_BUFFER_SET_CMD,
                                       handle_sel_buffer_set, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_GET_LAST_PROCESSED_EVENT_ID_CMD,
                                       handle_get_last_processed_event, sys);

    ipmi_mc_set_chassis_control_func(bmc_mc, bmc_set_chassis_control,
                                     bmc_get_chassis_control, sys);

    if (rv) {
	sys->log(sys, OS_ERROR, NULL,
		 "Unable to register NEW handler: %s", strerror(rv));
    }

    return 0;
}

/**
 * This is called after the emulator command file is run. This
 * can be used to finish up configuration of things, add
 * functions to sensors, do any sensor initialization, or
 * anything else that needs to be done after the emulator
 * commands are run.
**/
int
ipmi_sim_module_post_init(sys_data_t *sys)
{
    int rv;
    unsigned char lver[4];
    unsigned char omajor, ominor, orel;
    unsigned int i;
    int val;



    return rv;
}
