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
#include <OpenIPMI/ipmi_mlx.h>
#include "../bmc.h"

static lmc_data_t *bmc_mc;
static unsigned char all_fans_failure = 0;
static unsigned int fan_speed_front_profile1[] = {21000, 6300, 6300, 6300, 8400, 10500, 12700, 15000, 17000, 19500};
static unsigned int fan_speed_rear_profile1[] = {18000, 5400, 5400, 5400, 7200, 9000, 10800, 12600, 14500, 16500};

/**************************************************************************
 *                  Mellanox custom commands codes                        *
 *************************************************************************/
/* IPMI_SENSOR_EVENT_NETFN (0x04) */
#define IPMI_OEM_MLX_SET_FAN_SPEED_CMD      0x30
#define IPMI_OEM_MLX_GET_LED_STATE_CMD      0x31
#define IPMI_OEM_MLX_SET_LED_STATE_CMD      0x32
#define IPMI_OEM_MLX_SET_LED_BLINKING_CMD   0x33
#define IPMI_OEM_MLX_GET_FAN_PWM_CMD        0x34
#define IPMI_OEM_MLX_GET_TOTAL_POWER_CMD    0x35

/* IPMI_APP_NETFN (0x06) */
#define IPMI_OEM_MLX_SEL_BUFFER_SET_CMD     0x5B
#define IPMI_OEM_MLX_CPU_READY_EVENT_CMD    0x5c
#define IPMI_OEM_MLX_CPU_HARD_RESET_CMD     0x5e
#define IPMI_OEM_MLX_CPU_SOFT_RESET_CMD     0x5f
#define IPMI_OEM_MLX_RESET_PHY_CMD          0x60
#define IPMI_OEM_MLX_SET_UART_TO_BMC_CMD    0x61
#define IPMI_OEM_MLX_THERMAL_ALGORITHM_CMD  0x62
#define IPMI_OEM_MLX_BMC_UPTIME_GET_CMD     0x63
#define IPMI_OEM_MLX_LOG_TO_SEL_CMD         0x64

#define IPMI_OEM_MLX_SEL_LOG_SIZE_MIN       0x40
#define IPMI_OEM_MLX_SEL_LOG_SIZE_MAX       0x0fff

/* Set sel log size script file path */
#define SEL_SET_SCRIPT_NAME "sel_set_log_size.sh"

/**  FAN FUNCTIONALITY DEFINES  **/
#define MLX_FAN_TACHO_FILE  "/bsp/fan/tacho"
#define MLX_FAN_PWM_FILE            "/bsp/fan/pwm"
#define MLX_FAN_PWM_ENABLE_FILE     "/bsp/fan/pwm_en"

/**  LED FUNCTIONALITY DEFINES  **/
#define LED_FAN_FILE "/bsp/leds/fan/"
#define LED_STATUS_FILE "/bsp/leds/status/"
#define LED_BRIGHTNESS "brightness"
#define LED_TRIGGER    "trigger"
#define LED_DELAY_OFF  "delay_off"
#define LED_DELAY_ON   "delay_on"
#define LED_TIMER      "timer"

#define LED_COLOR_RED    1
#define LED_COLOR_GREEN  2
#define LED_COLOR_AMBER  3

#define MLX_LED_BLINK_OFF 0
#define MLX_LED_BLINK_3HZ 3
#define MLX_LED_BLINK_6HZ 6

/* Reset links */
#define MLX_BMC_SOFT_RESET   "/bsp/reset/bmc_reset_soft"
#define MLX_CPU_HARD_RESET   "/bsp/reset/cpu_reset_hard"
#define MLX_CPU_SOFT_RESET   "/bsp/reset/cpu_reset_soft"
#define MLX_SYS_HARD_RESET   "/bsp/reset/system_reset_hard"
#define MLX_RESET_PHY        "/bsp/reset/reset_phy"

#define MLX_UART_TO_BMC      "/bsp/reset/uart_sel"

#define MLX_PSU_COUNT        2
#define MLX_PSU_PIN_FILE     "/bsp/environment/psu%i_pin"

/* number of thermal zones */
#define MLX_MAX_THERMAL_ZONE 1
#define MLX_THERMAL_ZONE     "/bsp/thermal/thermal_zone%u/mode"

#define MLX_UPTIME_FILE      "/proc/uptime"

static const char* reset_cause[8] =
{
    "/bsp/reset/ac_power_cycle",
    "/bsp/reset/dc_power_cycle",
    "/bsp/reset/platform_rst",
    "/bsp/reset/thermal_or_swb_fail",
    "/bsp/reset/cpu_power_down",
    "/bsp/reset/cpu_reboot",
    "/bsp/reset/cpu_sleep_or_fail",
    "/bsp/reset/rst_from_cpu",
    "/bsp/reset/bmc_soft_rst",
    "/bsp/reset/sys_pwr_cycle",
    "/bsp/reset/cpu_rst",
    "/bsp/reset/psu_pwrok_fail"
};

enum reset_cause_e {
    MLX_RESET_CAUSE_AC_POWER_CYCLE = 0,
    MLX_RESET_CAUSE_DC_POWER_CYCLE,
    MLX_RESET_CAUSE_PLATFORM_RST,
    MLX_RESET_CAUSE_THERMAL_OR_SWB_FAIL,
    MLX_RESET_CAUSE_CPU_POWER_DOWN,
    MLX_RESET_CAUSE_CPU_REBOOT,
    MLX_RESET_CAUSE_CPU_SLEEP_OR_FAIL,
    MLX_RESET_CAUSE_RESET_FROM_CPU,
    MLX_RESET_CAUSE_BMC_SOFT_RST,
    MLX_RESET_CAUSE_SYS_PWR_CYCLE,
    MLX_RESET_CAUSE_CPU_RST,
    MLX_RESET_CAUSE_PSU_PWROK_FAIL,
    MLX_RESET_CAUSE_BUTTON
};

/*
 * This timer is called periodically to monitor the system reset cause.
 */
static ipmi_timer_t *reset_monitor_timer = NULL;
#define MLX_RESET_MONITOR_TIMEOUT         10

/*
 * This timer is called periodically to monitor the FANs
 */
static ipmi_timer_t *fans_monitor_timer = NULL;
#define MLX_FANS_MONITOR_TIMEOUT          10



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
    if (pwm > sys_devices.fan_pwm_max) {
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

static unsigned char set_led_command(unsigned char led,
                            unsigned char color,
                            unsigned char cmd)
{
    FILE *fbrightness;
    FILE *ftrigger;
    char fname[100];
    char cmd_trigger[100];

    memset(fname, 0, sizeof(fname));
    memset(cmd_trigger, 0, sizeof(cmd_trigger));

    switch (color) {
    case LED_COLOR_RED:
        if (led <= sys_devices.status_led_number) {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sred/%s\n",LED_STATUS_FILE, LED_TRIGGER);

            sprintf(fname, "%sred/%s", LED_STATUS_FILE, LED_BRIGHTNESS);
        }
        else {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sred/%u/%s\n",LED_FAN_FILE, led - sys_devices.status_led_number, LED_TRIGGER);

            sprintf(fname, "%sred/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_BRIGHTNESS);
        }
        break;
    case LED_COLOR_GREEN:
        if (led <= sys_devices.status_led_number) {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sgreen/%s\n",LED_STATUS_FILE, LED_TRIGGER);

            sprintf(fname, "%sgreen/%s", LED_STATUS_FILE, LED_BRIGHTNESS);
        }
        else {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sgreen/%u/%s\n",LED_FAN_FILE, led - sys_devices.status_led_number, LED_TRIGGER);

            sprintf(fname, "%sgreen/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_BRIGHTNESS);
        }
        break;
    case LED_COLOR_AMBER:
        if (cmd)
            sprintf(cmd_trigger,"echo timer > %samber/%s\n",LED_STATUS_FILE, LED_TRIGGER);

        sprintf(fname, "%samber/%s", LED_STATUS_FILE, LED_BRIGHTNESS);
        break;
    default:
        break;
    }

    fbrightness = fopen(fname, "w");

    if (!fbrightness) {
        fclose(fbrightness);
        return IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
    }

    fprintf(fbrightness, "%u", cmd);
    system(cmd_trigger);

    fclose(fbrightness);

    return 0;
}

/**
 *
 * ipmitool raw 0x04  0x32  LedNum  Color
 *
 * LedNum:
 * 0x1 - status LED
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
    unsigned char rv = 0;
    unsigned char led, color;

    if (check_msg_length(msg, 2, rdata, rdata_len))
        return;

    led = msg->data[0];
    if (led > sys_devices.status_led_number + sys_devices.fan_led_number ||
        led == 0) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    color = msg->data[1];
    if (color <  LED_COLOR_RED || color > LED_COLOR_AMBER) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    switch (color) {
    case LED_COLOR_AMBER:
        if (led > sys_devices.status_led_number) {
            rv = IPMI_INVALID_DATA_FIELD_CC;
            goto out;
        }
        rv = set_led_command(led, LED_COLOR_RED, 0);
        if (rv)
            goto out;
        rv = set_led_command(led, LED_COLOR_GREEN, 0);
        if (rv)
            goto out;
        rv = set_led_command(led, LED_COLOR_AMBER, 1);
        if (rv)
            goto out;
        break;
    case LED_COLOR_RED:
        if (led <= sys_devices.status_led_number){
            rv = set_led_command(led, LED_COLOR_AMBER, 0);
            if (rv)
                goto out;
        }
        rv = set_led_command(led, LED_COLOR_GREEN, 0);
        if (rv)
            goto out;
        rv = set_led_command(led, LED_COLOR_RED, 1);
        if (rv)
            goto out;
        break;
    case LED_COLOR_GREEN:
        if (led <= sys_devices.status_led_number) {
            rv = set_led_command(led, LED_COLOR_AMBER, 0);
            if (rv)
                goto out;
        }
        rv = set_led_command(led, LED_COLOR_RED, 0);
        if (rv)
            goto out;
        rv = set_led_command(led, LED_COLOR_GREEN, 1);
        if (rv)
            goto out;
        break;
    default:
        break;
    }

 out:
    rdata[0] = rv;
    *rdata_len = 1;
}

/**
 *
 * ipmitool raw 0x04  0x33  LedNum  Color Time
 *
 * LedNum:
 * 0x1 - status LED
 * 0x2 - FAN1 LED
 * 0x3 - FAN2 LED
 * 0x4 - FAN2 LED
 * 0x5 - FAN2 LED
 *
 * Color:
 * 0x1 - red
 * 0x2 - green
 * 0x3 - amber 
 *
 * Time: 
 * 0x0 - off blinking
 * 0x3 - blinking 3HZ
 * 0x6 - blinking 6HZ 
 *
 *  */ 
static void
handle_set_led_blinking (lmc_data_t    *mc,
                msg_t         *msg,
                unsigned char *rdata,
                unsigned int  *rdata_len,
                void          *cb_data)
{
    unsigned char led;
    unsigned char time;
    unsigned char color;
    FILE *f_delayon;
    FILE *f_delayoff;
    char fname[100];

    if (check_msg_length(msg, 3, rdata, rdata_len))
        return;

    led = msg->data[0];
    color = msg->data[1];
    time = msg->data[2];
    if (led == 0 ||
        led > sys_devices.status_led_number + sys_devices.fan_led_number || 
        color > LED_COLOR_AMBER ||
        color < LED_COLOR_RED ||
        (time != MLX_LED_BLINK_OFF && 
         time != MLX_LED_BLINK_3HZ && 
         time != MLX_LED_BLINK_6HZ)) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    if (led <= sys_devices.status_led_number) {
        switch (color) {
        case LED_COLOR_RED:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", LED_STATUS_FILE, LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", LED_STATUS_FILE,LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case LED_COLOR_GREEN:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%s", LED_STATUS_FILE, LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%s", LED_STATUS_FILE,LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case LED_COLOR_AMBER:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%samber/%s", LED_STATUS_FILE, LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%samber/%s", LED_STATUS_FILE,LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        default:
            break;
        }
    }
    else if (led > sys_devices.status_led_number && 
             led < sys_devices.status_led_number + sys_devices.fan_led_number) {
        switch (color) {
        case LED_COLOR_RED:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", LED_FAN_FILE, led - sys_devices.status_led_number,LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case LED_COLOR_GREEN:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number,LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        default:
            break;
        }
    }

    if (!f_delayon || !f_delayoff) {
        if (f_delayoff)
            fclose(f_delayoff);
        if (f_delayon)
            fclose(f_delayon);

        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
        return;
    }

    switch (time) {
    case MLX_LED_BLINK_OFF:
        fprintf(f_delayoff, "%u", 0);
        fprintf(f_delayon, "%u", 0);
        break;
    case MLX_LED_BLINK_3HZ:
        fprintf(f_delayoff, "%u", 83);
        fprintf(f_delayon, "%u", 83);
        break;
    case MLX_LED_BLINK_6HZ:
        fprintf(f_delayoff, "%u", 167);
        fprintf(f_delayon, "%u", 167);
        break;
    default:
        break;
    }

    fclose(f_delayon);
    fclose(f_delayoff);

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

    if (led <= sys_devices.status_led_number) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%samber/%s", LED_STATUS_FILE, LED_BRIGHTNESS);

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

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sgreen/%s", LED_STATUS_FILE, LED_BRIGHTNESS);
        f_green = fopen(fname, "r");

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sred/%s", LED_STATUS_FILE, LED_BRIGHTNESS);
        f_red = fopen(fname, "r");
    }
    else {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sgreen/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_BRIGHTNESS);
        f_green = fopen(fname, "r");

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sred/%u/%s", LED_FAN_FILE, led - sys_devices.status_led_number, LED_BRIGHTNESS);
        f_red = fopen(fname, "r");
    }

    if (!f_green || !f_red) {
        printf("\nUnable to open LED status file");
        if (led <= sys_devices.status_led_number)
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
        if (led <= sys_devices.status_led_number)
            fclose(f_amber);
        fclose(f_green);
        fclose(f_red);
        return IPMI_INVALID_DATA_FIELD_CC;
    }

    red = strtoul(line_red, NULL, 0);
    green = strtoul(line_green, NULL, 0);

    if (led <= sys_devices.status_led_number) {
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
 * ipmitool raw 0x04  0x31  LedNum
 *
 * LedNum:
 * 0x0 - status LED
 * 0x1 - FAN1 LED
 * 0x2 - FAN2 LED
 * 0x3 - FAN2 LED
 * 0x4 - FAN2 LED
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
    if (led > sys_devices.status_led_number + sys_devices.fan_led_number) {
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
    sys_data_t *sys = cb_data;
    FILE *freset;

    freset = fopen(MLX_BMC_SOFT_RESET, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, mc->ipmb, 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x6);

        if (reset_monitor_timer)
            sys->free_timer(reset_monitor_timer);

        fprintf(freset, "%u", 0);
    }

    fclose(freset);
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06 0x5c [0x0]
 *
 **/
static void
handle_cpu_ready_event(lmc_data_t    *mc,
			  msg_t         *msg,
			  unsigned char *rdata,
			  unsigned int  *rdata_len,
			  void          *cb_data)
{
    unsigned char status_led_run_str[32];
    unsigned int ready;

    if (check_msg_length(msg, 1, rdata, rdata_len)) {
        ready = 1;
    }
    else {
        ready = msg->data[0];
        if (ready != 0 ) {
            rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
            *rdata_len = 1;
            return;
        }
    }

   if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",0xbc, 1, 0))
       system(status_led_run_str);

   if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",0xbb, ready, IPMI_SENSOR_TYPE_PROCESSOR))
       system(status_led_run_str);

   if (ready) {
        /* set "Presence detected" if CPU started successfully */
        system("echo 128 > /bsp/environment/cpu_status");
   }
   else {
        /* set "IERR" in case something goes wrong on CPU sturtup */
        system("echo 1 > /bsp/environment/cpu_status");
   }
    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x5e [off/on] [0x1]
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
    unsigned char cpu_reboot_cmd = 0;

    if (check_msg_length(msg, 1, rdata, rdata_len)) {
        reset = 0;
    }
    else {
        if (check_msg_length(msg, 2, rdata, rdata_len)) {
            reset = msg->data[0];
        }
        else {
            reset = msg->data[0];
            cpu_reboot_cmd = msg->data[1];
        }

        if (reset > 1 || cpu_reboot_cmd > 1) {
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

    if (!reset) {
        if (cpu_reboot_cmd)
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_OS_CRITICAL_STOP , 40, 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x3);
        else
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_OS_CRITICAL_STOP , 0, 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x3);

    }

    rdata[0] = 0;
    *rdata_len = 1;
}

/**
 *
 *  ipmitool raw 0x06  0x5f [0x1]
 *
 **/
static void
handle_cpu_soft_reset(lmc_data_t    *mc,
                      msg_t         *msg,
                      unsigned char *rdata,
                      unsigned int  *rdata_len,
                      void          *cb_data)
{
    FILE *freset;
    unsigned char cpu_reboot_cmd = 0;
    char trigger[100];
    FILE *ftrigger;
    sys_data_t *sys = cb_data;

    if (!check_msg_length(msg, 1, rdata, rdata_len)) {
        cpu_reboot_cmd = msg->data[0];
        if (cpu_reboot_cmd != 1 ) {
            rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
            *rdata_len = 1;
            return;
        }
    }

    freset = fopen(MLX_CPU_SOFT_RESET, "w");

    if (!freset) {
            printf("\nUnable to open reset file");
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            return;
    } else {
        fprintf(freset, "%u", 0);

        /* sel status LED to green to enable delay_off/delay_on */
        set_led_command(1, LED_COLOR_GREEN, 1);

       /* set 0 state at CPU reboot */
        system("echo 0 > /bsp/environment/cpu_status");

        /* set LED blinking on CPU restart */
        memset(trigger, 0, sizeof(trigger));
        if (sprintf(trigger,"status_led.py 0x%02x %d 0x%02x\n", 0xbc, 0, 0))
            system(trigger);
        else
            sys->log(sys, OS_ERROR, NULL,"Unable to set status LED blinking");
    }

    fclose(freset);

    if (cpu_reboot_cmd) {
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, 40, 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x7); 
    }
    else
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, 0, 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x7); 

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
 *  ipmitool raw 0x06  0x62 [zone] [enable/disable]
 *
 **/
static void
handle_thermal_algorithm_set(lmc_data_t    *mc,
                     msg_t         *msg,
                     unsigned char *rdata,
                     unsigned int  *rdata_len,
                     void          *cb_data)
{
    unsigned int zone, state;
    FILE *file;
    char fname[100];

    if (check_msg_length(msg, 2, rdata, rdata_len))
        return;

    zone = msg->data[0];
    if (zone >= MLX_MAX_THERMAL_ZONE) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    state = msg->data[1];
    if (state > 1) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    memset(fname, 0, sizeof(fname));
    sprintf(fname, MLX_THERMAL_ZONE, zone);
    file = fopen(fname, "w");

    if (!file) {
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
     return;
    } else {
        if (state) 
            fprintf(file, "enabled");
        else
            fprintf(file, "disabled");

        fclose(file);
    }

    rdata[0] = 0;
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
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_EVENT, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x1);
            fprintf(freset, "%u", 0);
        }

        fclose(freset);
        break;
    default:
        return EINVAL;
    }

    return 0;
}

/**
 *
 * ipmitool raw 0x04  0x35
 *
**/
static void
handle_get_total_power_cmd(lmc_data_t    *mc,
                           msg_t         *msg,
                           unsigned char *rdata,
                           unsigned int  *rdata_len,
                           void          *cb_data)
{
    unsigned char rv = 0;
    char line_pin[10];
    unsigned char total = 0;
    FILE *fpin;

    for (int i = 0; i < MLX_PSU_COUNT; ++i) {
        char filename[50];
        unsigned int tmp = 0;
        memset(filename, 0, sizeof(filename));
        sprintf(filename, MLX_PSU_PIN_FILE, i+1);

        fpin = fopen(filename, "r");
        if (fpin) {
            if (0 >= fread(line_pin, 1, sizeof(line_pin),fpin))
            {
                fclose(fpin);
                rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
                *rdata_len = 1;
                return;
            }
            tmp = strtoul(line_pin, NULL, 0);
            total += tmp/1000000;
            fclose(fpin);
        }
    }

    rdata[0] = 0;
    rdata[1] = total;
    *rdata_len = 2;
    return;
}

/**
 *
 * ipmitool raw 0x06  0x63
 *
**/
static void
handle_bmc_uptime_get(lmc_data_t    *mc,
                       msg_t         *msg,
                       unsigned char *rdata,
                       unsigned int  *rdata_len,
                       void          *cb_data)
{
    unsigned char rv = 0;
    char uptime[10];
    unsigned int val = 0;
    unsigned char seconds = 0;
    unsigned char minutes = 0;
    unsigned char hours = 0;
    unsigned char days = 0;
    FILE *fuptime;
    sys_data_t *sys = cb_data;

    memset(uptime, 0, sizeof(uptime));
    fuptime = fopen(MLX_UPTIME_FILE, "r");

    if (!fuptime) {
        sys->log(sys, OS_ERROR, NULL,"Unable to open  uptime file");
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
        return;
    }

    fscanf(fuptime, "%s", uptime);
    val = strtoul(uptime, NULL, 0);

    fclose(fuptime);

    seconds = val%60;
    days = (val - seconds)/86400; /* 60*60*24 */
    hours = (val - seconds)/3600 - days*24;
    minutes = (val - seconds)/60 - (hours*60 + days*24*60);

    rdata[0] = 0;
    rdata[1] = days;
    rdata[2] = hours;
    rdata[3] = minutes;
    rdata[4] = seconds;
    *rdata_len = 5;
    return;
}

/**
 *
 * ipmitool raw 0x06  0x64 [sdr_type] [direction] [event_type]
 *
**/
static void
handle_log_to_sel(lmc_data_t    *mc,
                  msg_t         *msg,
                  unsigned char *rdata,
                  unsigned int  *rdata_len,
                  void          *cb_data)
{
    unsigned char sdr_type;
    unsigned char direction;
    unsigned char event_type;

    if (check_msg_length(msg, 3, rdata, rdata_len)) {
        return;
    }

    sdr_type = msg->data[0];
    direction = msg->data[1];
    event_type = msg->data[2];

    mlx_add_event_to_sel(mc, sdr_type , 0, direction, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, event_type);

    rdata[0] = 0;
    *rdata_len = 1;
    return;
}

static void
reset_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    int i;
    struct timeval tv;
    int fd;
    int rv;

    for (i = 0; i < 8; ++i) {
        unsigned char c = 0;
        int active = 0;

        fd = open(reset_cause[i], O_RDONLY);

        rv = read(fd, &c, 1);
        if (rv != 1) {
            sys->log(sys, OS_ERROR, NULL, "Warning: filed to read '%s' file", reset_cause[i]);
            continue;
        }
        active = atoi(&c);
        if (active) {
            switch (i) {
            case MLX_RESET_CAUSE_AC_POWER_CYCLE:
                /* "Power Unit", "Power cycle" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_POWER_UNIT, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x1);
                break;
            case MLX_RESET_CAUSE_DC_POWER_CYCLE:
                /* "System Event", "OEM System boot event" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_SYSTEM_EVENT, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x1);
                break;
            case MLX_RESET_CAUSE_PLATFORM_RST:
                /* "Version Change", "Firmware or software change success" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_VERSION_CHANGE, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x7);
                break;
            case MLX_RESET_CAUSE_THERMAL_OR_SWB_FAIL:
                /* "System Firmware Error", "Unknown Error" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_SYSTEM_FIRMWARE_PROGRESS, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x0);
                break;
            case MLX_RESET_CAUSE_CPU_POWER_DOWN:
                /* "Power Unit", "Power off/down" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_POWER_UNIT, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x0);
                break;
            case MLX_RESET_CAUSE_CPU_REBOOT:
                /* "System Boot Initiated", "System Restart" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x7);
                break;
            case MLX_RESET_CAUSE_CPU_SLEEP_OR_FAIL:
                /* "OS Stop/Shutdown", "OS graceful shutdown" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_OS_CRITICAL_STOP, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x3);
                break;
            case MLX_RESET_CAUSE_RESET_FROM_CPU:
                /* "Watchdog 2", "Power cycle" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_WATCHDOG_2, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x3);
                break;
            case MLX_RESET_CAUSE_BUTTON:
                /* "Button", "Reset Button pressed" */
                mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_BUTTON, 0 , 0, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, 0x2);
                break;
            }
        }
        close(fd);
    }

    tv.tv_sec = MLX_RESET_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(reset_monitor_timer, &tv);
}

static unsigned int 
get_expected_fan_speed(unsigned char tacho_num,
                         unsigned char fan_pwm)
{
    switch (sys_devices.fan_tacho_per_drw) {
    case 2:
        if (tacho_num%2 == 1)
            return sys_devices.fan_speed_front[fan_pwm];
        else
            return sys_devices.fan_speed_rear[fan_pwm];
        break;
    default:
        break;
    }
    return 0;
}

static void
fans_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    struct timeval tv;
    int failed = 0;
    int i = 0;
    unsigned char fname[30];
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];

    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "/bsp/fan/tacho%i_rpm", i);
        if (access(fname, F_OK) != 0)
            ++failed;
    }

    /* All FANs failure monitoring */
    if (failed == sys_devices.fan_number * sys_devices.fan_tacho_per_drw) {
        if (all_fans_failure == 0) {
            mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_FAN, 0 , 0, IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, 0x0);
            all_fans_failure = 1;
        }
    }
    else if (all_fans_failure == 1) {
        mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_FAN, 0 , 1, IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, 0x0);
        all_fans_failure = 0;
    }

    /* Abnormal FAN speed monitoring */
    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        FILE *file;
        unsigned long int rpm, pwm;
        unsigned int expected_speed;
        char line[10];

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "/bsp/fan/tacho%i_rpm", i);

        file = fopen(fname, "r");

        if (!file)
            continue;

        if (0 >= fread(line, 1, sizeof(line),file)) {
            fclose(file);
            continue;
        }
        rpm = strtoul(line, NULL, 0);
        fclose(file);

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "/bsp/fan/pwm", i);
        file = fopen(fname, "r");

        if (!file)
            continue;

        if (0 >= fread(line, 1, sizeof(line),file)) {
            fclose(file);
            continue;
        }

        pwm = strtoul(line, NULL, 0);
        fclose(file);

        if (rpm == 0) {
            memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

            data[1] = 0x0; /* event descr id */
            data[2] = 0x70 + i - 1; /*FAN# */

            mc_new_event(bmc_mc, 0xE0, data);
        }
        else {
            expected_speed = sys_devices.get_fan_speed(i, pwm);

            if (rpm < expected_speed * (1 - sys_devices.fan_speed_deviation)) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

                data[1] = 0x1; /* event descr id */
                data[2] = 0x70 + i -1;/* FAN# */

                mc_new_event(bmc_mc, 0xE0, data);
            }
            if (rpm > expected_speed * (1 + sys_devices.fan_speed_deviation)) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

                data[1] = 0x2; /* event descr id */
                data[2] = 0x70 + i -1; /* FAN# */

                mc_new_event(bmc_mc, 0xE0, data);
            }
        }
    }

    tv.tv_sec = MLX_FANS_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(fans_monitor_timer, &tv);
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

void sys_time_set(unsigned char* data)
{
    struct tm *nowtm;
    char tmbuf[64];
    uint32_t timeval = ipmi_get_uint32(data);

    system("systemctl stop systemd-timesyncd");
    system("systemctl disable systemd-timesyncd");

    time_t nowtime = *((time_t*)&timeval);
    nowtm = localtime(&nowtime);
    system("timedatectl set-ntp false");

    memset(tmbuf, 0, sizeof(tmbuf));
    strftime(tmbuf, sizeof(tmbuf), "timedatectl set-time %Y-%m-%d", nowtm);
    system(tmbuf);

    memset(tmbuf, 0, sizeof(tmbuf));
    strftime(tmbuf, sizeof(tmbuf), "timedatectl set-time %H:%M:%S", nowtm);
    system(tmbuf);
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
    struct timeval tv;
    char fname[50];

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

    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%i_en", MLX_FAN_TACHO_FILE, i);
        rv = set_fan_enable(fname);

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

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_CPU_READY_EVENT_CMD,
                                       handle_cpu_ready_event, sys);

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

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_THERMAL_ALGORITHM_CMD,
                                       handle_thermal_algorithm_set, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_GET_LAST_PROCESSED_EVENT_ID_CMD,
                                       handle_get_last_processed_event, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_SENSOR_EVENT_NETFN, IPMI_OEM_MLX_GET_TOTAL_POWER_CMD,
                                       handle_get_total_power_cmd, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_BMC_UPTIME_GET_CMD,
                                       handle_bmc_uptime_get, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_APP_NETFN, IPMI_OEM_MLX_LOG_TO_SEL_CMD,
                                       handle_log_to_sel, sys);

    ipmi_mc_set_chassis_control_func(bmc_mc, bmc_set_chassis_control,
                                     bmc_get_chassis_control, sys);

    if (rv) {
	sys->log(sys, OS_ERROR, NULL,
		 "Unable to register NEW handler: %s", strerror(rv));
    }

    rv = sys->alloc_timer(sys, reset_monitor_timeout, sys, &reset_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create reset monitoring timer");
        return errval;
    } else {
        tv.tv_sec = MLX_RESET_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(reset_monitor_timer, &tv);
    }

    /* set "Disabled" state at startup */
    system("echo 256 > /bsp/environment/cpu_status");

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
    unsigned int fw_maj = 0;
    unsigned int fw_min = 0;
    unsigned char id_line[50];
    unsigned char id_maj[2];
    unsigned char id_min[2];
    unsigned int  productId;
    struct timeval tv;
    int rv;
    FILE *fid;

    memset(id_line, 0, sizeof(id_line));
    memset(id_min, 0, sizeof(id_min));
    memset(id_maj, 0, sizeof(id_maj));

    system("grep ID_LIKE /etc/os-release > /tmp/release");
    fid = fopen("/tmp/release", "r");

    if (!fid) {
        sys->log(sys, OS_ERROR, NULL, "Unable to open  FW ID file");
        return 0;
    }

    if (0 >= fread(id_line, 1, sizeof(id_line),fid))
    {
        fclose(fid);
        sys->log(sys, OS_ERROR, NULL, "Unable to read  FW ID file");
        return 0;
    }

    fclose(fid);

    memcpy(id_maj, id_line+13, sizeof(id_maj));
    memcpy(id_min, id_line+15, sizeof(id_min));

    fw_maj = strtoul(id_maj, NULL, 0);
    fw_min = strtoul(id_min, NULL, 0);

    ipmi_mc_set_fw_revision(bmc_mc, fw_maj, fw_min);

    productId = (sys->mc->product_id[1] << 8) | sys->mc->product_id[0];
    switch (productId) {
    case 1: /* Baidu BMC */
        sys_devices.fan_number = 4;
        sys_devices.fan_tacho_per_drw = 2;
        sys_devices.fan_pwm_max = 9;
        sys_devices.fan_eeprom_number = 4;
        sys_devices.psu_number = 2;
        sys_devices.status_led_number = 1;
        sys_devices.fan_led_number = 4;
        sys_devices.fan_speed_deviation = 0.15;
        sys_devices.get_fan_speed = get_expected_fan_speed;
        sys_devices.fan_speed_front = fan_speed_front_profile1;
        sys_devices.fan_speed_rear = fan_speed_rear_profile1;
        break;
    default:
        break;
    }

    rv = sys->alloc_timer(sys, fans_monitor_timeout, sys, &fans_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create FANs monitoring timer");
        return errval;
    }
    else {
        tv.tv_sec = MLX_FANS_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(fans_monitor_timer, &tv);
    }

    sys->mc->sys_time_set_func = sys_time_set;

    return 0;
}
