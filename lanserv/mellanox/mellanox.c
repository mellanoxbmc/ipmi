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
#include <time.h>
#include <OpenIPMI/serserv.h>
#include <OpenIPMI/ipmi_err.h>
#include <OpenIPMI/ipmi_msgbits.h>
#include <OpenIPMI/ipmi_bits.h>
#include <OpenIPMI/ipmi_mc.h>
#include <OpenIPMI/serv.h>
#include <OpenIPMI/ipmi_mlx.h>
#include "../bmc.h"

#if HAVE_SYSLOG
#include <syslog.h>
#endif

static lmc_data_t *bmc_mc;
static unsigned char all_fans_failure = 0;
static unsigned int mlx_fan_speed_front_profile1[] = {21000, 6300, 6300, 6300, 8400, 10500, 12700, 15000, 17000, 19500};
static unsigned int mlx_fan_speed_rear_profile1[] = {18000, 5400, 5400, 5400, 7200, 9000, 10800, 12600, 14500, 16500};

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

#define IPMI_OEM_MLX_TZ_NETFUNC             0x32
#define IPMI_OEM_MLX_TZ_SET_CMD             0xa5

#define IPMI_OEM_MLX_SEL_LOG_SIZE_MIN       0x40
#define IPMI_OEM_MLX_SEL_LOG_SIZE_MAX       0x0fff

/* Set sel log size script file path */
#define SEL_SET_SCRIPT_NAME "sel_set_log_size.sh"

/**  FAN FUNCTIONALITY DEFINES  **/
#define MLX_FAN_TACHO_FILE  "/bsp/fan/tacho"
#define MLX_FAN_PWM_FILE            "/bsp/fan/pwm"
#define MLX_FAN_PWM_ENABLE_FILE     "/bsp/fan/pwm_en"

/**  LED FUNCTIONALITY DEFINES  **/
#define MLX_LED_FAN_FILE "/bsp/leds/fan/"
#define MLX_LED_STATUS_FILE "/bsp/leds/status/"
#define MLX_LED_BRIGHTNESS "brightness"
#define MLX_LED_TRIGGER    "trigger"
#define MLX_LED_DELAY_OFF  "delay_off"
#define MLX_LED_DELAY_ON   "delay_on"
#define MLX_LED_TIMER      "timer"

#define MLX_LED_COLOR_RED    1
#define MLX_LED_COLOR_GREEN  2
#define MLX_LED_COLOR_AMBER  3

#define MLX_LED_BLINK_OFF 0
#define MLX_LED_BLINK_3HZ 3
#define MLX_LED_BLINK_6HZ 6

/* Reset links */
#define MLX_BMC_SOFT_RESET   "/bsp/reset/bmc_reset_soft"
#define MLX_CPU_HARD_RESET   "/bsp/reset/cpu_reset_hard"
#define MLX_CPU_SOFT_RESET   "/bsp/reset/cpu_reset_soft"
#define MLX_SYS_HARD_RESET   "/bsp/reset/system_reset_hard"
#define MLX_PS1_ON           "/bsp/reset/ps1_on"
#define MLX_PS2_ON           "/bsp/reset/ps2_on"
#define MLX_RESET_PHY        "/bsp/reset/reset_phy"

#define MLX_UART_TO_BMC      "/bsp/reset/uart_sel"

#define MLX_PSU_COUNT        2
#define MLX_PSU_PIN_FILE     "/bsp/environment/psu%i_pin"

/* number of thermal zones */
#define MLX_MAX_THERMAL_ZONE 1
#define MLX_THERMAL_ZONE     "/bsp/thermal/thermal_zone%u/mode"

#define MLX_UPTIME_FILE      "/proc/uptime"

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
    MLX_RESET_CAUSE_SW_CMD,
    MLX_RESET_CAUSE_RST_FROM_MB,
    MLX_RESET_CAUSE_AUX_OFF_OR_RELOAD,
    MLX_RESET_CAUSE_CPU_POWER_FAIL,
    MLX_RESET_CAUSE_PLAT_RST_ASSERT,
    MLX_RESET_CAUSE_PWROFF_BY_SOC,
    MLX_RESET_CAUSE_CPU_RST_BY_WD,
    MLX_RESET_CAUSE_POWER_OK_ASSERT,
    MLX_RESET_CAUSE_BUTTON,
    MLX_RESET_CAUSE_MAX = MLX_RESET_CAUSE_BUTTON
};

static const char* reset_cause[MLX_RESET_CAUSE_MAX] =
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
    "/bsp/reset/psu_pwrok_fail",
    "/bsp/reset/sw_cmd",
    "/bsp/reset/rst_from_mb",
    "/bsp/reset/aux_off_or_reload",
    "/bsp/reset/cpu_pwr_fail",
    "/bsp/reset/plat_rst_assert",
    "/bsp/reset/pwroff_by_soc",
    "/bsp/reset/cpu_rst_by_wd",
    "/bsp/reset/power_ok_assert"
};

static unsigned int reset_logged = 0;
static unsigned int chassis_status = 0;
static unsigned int poh_start_point = 0;

/*
 * This timer is called periodically to monitor the system reset cause.
 */
static ipmi_timer_t *mlx_reset_monitor_timer = NULL;
#define MLX_RESET_MONITOR_TIMEOUT         10

/*
 * This timer is called periodically to monitor the FANs
 */
static ipmi_timer_t *mlx_fans_monitor_timer = NULL;
#define MLX_FANS_MONITOR_TIMEOUT          10

/*
 * This timer is called periodically for overheat monitoring
 */
static ipmi_timer_t *mlx_overheat_monitor_timer = NULL;
#define MLX_OVERHEAT_MONITOR_TIMEOUT          5
#define MLX_CPU_TEMPERATURE_FILE              "/bsp/thermal/cpu_temp"
#define MLX_ASIC_TEMPERATURE_FILE             "/bsp/thermal/asic_temp"
#define MLX_CPU_MAX_TEMP                      110000
#define MLX_ASIC_MAX_TEMP                     82000

/*
 * This timer is called periodically for BMC watchdog monitoring
 */
static ipmi_timer_t *mlx_wd_monitor_timer = NULL;
static unsigned char mlx_wd_status = 0;
static unsigned char mlx_wd_control = 0;
#define MLX_WD_MONITOR_TIMEOUT          10
#define MLX_WD_TIMEOUT_CURRENT_STATUS_FILE    "/bsp/environment/bmcwd_current"
#define MLX_WD_TIMEOUT_SAVED_STATUS_FILE      "/bsp/environment/bmcwd_saved"
#define MLX_WD_CONTROL_CURRENT_STATUS_FILE    "/bsp/environment/bmcwd_control_curr"
#define MLX_WD_CONTROL_SAVED_STATUS_FILE      "/bsp/environment/bmcwd_control"

#define MLX_SYTEM_CONSOLE_DEVICE               "/dev/ttyS4"
#define MLX_CPU_STATUS_FILE                    "/bsp/environment/cpu_status"
#define MLX_CPU_STATUS_NONE                    0
#define MLX_CPU_STATUS_IERR                    1
#define MLX_CPU_STATUS_PRESENCE_DETECTED       128
#define MLX_CPU_STATUS_DISABLED                256
#define MLX_CPU_STATUS_MONITOR_TIMEOUT         300
static ipmi_timer_t *mlx_cpu_status_monitor_timer = NULL;

static void
mlx_cpu_status_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    FILE *fd;
    char cmd[MLX_SYS_CMD_BUF_SIZE];

    /* set LED to red */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd,"status_led.py 0x%02x %d 0x%02x\n", MLX_CPU_FAULT_LOG_NUM, MLX_EVENT_ASSERTED, 0))
        system(cmd);
    else
        sys->log(sys, OS_ERROR, NULL,"Unable to set status LED to red");

    /*Uart to BMC*/
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "echo 0 > %s", MLX_UART_TO_BMC);
    system(cmd);

    /* Printing the warning message */
    fd = fopen(MLX_SYTEM_CONSOLE_DEVICE, "w");
    fprintf(fd, "WARNING:\n The system console ownership has been taken by BMC \n "
            "due to timeout expiration.\n In order to return the system console ownership to CPU, \n"
            "Please enter the following command: cons2cpu \n");
    fclose(fd);

}

static void 
set_poh_start_point()
{
    char uptime[MLX_READ_BUF_SIZE];
    unsigned int val = 0;
    FILE *fuptime;

    memset(uptime, 0, sizeof(uptime));
    fuptime = fopen(MLX_UPTIME_FILE, "r");

    if (!fuptime) {
        poh_start_point = 0;
        return;
    }

    fscanf(fuptime, "%s", uptime);
    fclose(fuptime);
    val = strtoul(uptime, NULL, 0);

    poh_start_point = val;
}

static void
mlx_add_event_to_sel(lmc_data_t    *mc,
           unsigned char sensor_type,
           unsigned char sensor_num,
           unsigned char direction,
           unsigned char event_type,
           unsigned char offset)
{
    lmc_data_t    *dest_mc;
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];
    int           rv;

    rv = ipmi_emu_get_mc_by_addr(mc->emu, mc->event_receiver, &dest_mc);
    if (rv)
        return;

    memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

    data[4] = mc->ipmb;
    data[6] = 0x04; /* Event message revision for IPMI 1.5. */
    data[7] = sensor_type;
    data[8] = sensor_num;
    data[9] = (direction << MLX_EVENT_DIRECTION_SHIFT) | event_type;
    data[10] = offset;

    mc_new_event(dest_mc, 0x02, data);
}

static void mlx_sel_time_update(lmc_data_t    *mc)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    mc->sel.time_offset = ts.tv_sec;
}

static unsigned char mlx_set_fan_enable(const char* fname)
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
        if (IPMI_DESTINATION_UNAVAILABLE_CC == mlx_set_fan_enable(MLX_FAN_PWM_ENABLE_FILE)) {
            rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
            *rdata_len = 1;
            fclose(f_pwm);
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
    char line_pwm[MLX_READ_BUF_SIZE];
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

static unsigned char 
mlx_set_led_command(unsigned char led,
                            unsigned char color,
                            unsigned char cmd)
{
    FILE *fbrightness;
    FILE *ftrigger;
    char fname[MLX_FILE_NAME_SIZE];
    char cmd_trigger[MLX_SYS_CMD_BUF_SIZE];

    memset(fname, 0, sizeof(fname));
    memset(cmd_trigger, 0, sizeof(cmd_trigger));

    switch (color) {
    case MLX_LED_COLOR_RED:
        if (led <= sys_devices.status_led_number) {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sred/%s\n",MLX_LED_STATUS_FILE, MLX_LED_TRIGGER);

            sprintf(fname, "%sred/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);
        }
        else {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sred/%u/%s\n",MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_TRIGGER);

            sprintf(fname, "%sred/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_BRIGHTNESS);
        }
        break;
    case MLX_LED_COLOR_GREEN:
        if (led <= sys_devices.status_led_number) {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sgreen/%s\n",MLX_LED_STATUS_FILE, MLX_LED_TRIGGER);

            sprintf(fname, "%sgreen/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);
        }
        else {
            if (cmd)
                sprintf(cmd_trigger,"echo timer > %sgreen/%u/%s\n",MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_TRIGGER);

            sprintf(fname, "%sgreen/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_BRIGHTNESS);
        }
        break;
    case MLX_LED_COLOR_AMBER:
        if (cmd)
            sprintf(cmd_trigger,"echo timer > %samber/%s\n",MLX_LED_STATUS_FILE, MLX_LED_TRIGGER);

        sprintf(fname, "%samber/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);
        break;
    default:
        break;
    }

    fbrightness = fopen(fname, "w");

    if (!fbrightness) {
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
    if (color <  MLX_LED_COLOR_RED || color > MLX_LED_COLOR_AMBER) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    switch (color) {
    case MLX_LED_COLOR_AMBER:
        if (led > sys_devices.status_led_number) {
            rv = IPMI_INVALID_DATA_FIELD_CC;
            goto out;
        }
        rv = mlx_set_led_command(led, MLX_LED_COLOR_RED, 0);
        if (rv)
            goto out;
        rv = mlx_set_led_command(led, MLX_LED_COLOR_GREEN, 0);
        if (rv)
            goto out;
        rv = mlx_set_led_command(led, MLX_LED_COLOR_AMBER, 1);
        if (rv)
            goto out;
        break;
    case MLX_LED_COLOR_RED:
        if (led <= sys_devices.status_led_number){
            rv = mlx_set_led_command(led, MLX_LED_COLOR_AMBER, 0);
            if (rv)
                goto out;
        }
        rv = mlx_set_led_command(led, MLX_LED_COLOR_GREEN, 0);
        if (rv)
            goto out;
        rv = mlx_set_led_command(led, MLX_LED_COLOR_RED, 1);
        if (rv)
            goto out;
        break;
    case MLX_LED_COLOR_GREEN:
        if (led <= sys_devices.status_led_number) {
            rv = mlx_set_led_command(led, MLX_LED_COLOR_AMBER, 0);
            if (rv)
                goto out;
        }
        rv = mlx_set_led_command(led, MLX_LED_COLOR_RED, 0);
        if (rv)
            goto out;
        rv = mlx_set_led_command(led, MLX_LED_COLOR_GREEN, 1);
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
    char fname[MLX_FILE_NAME_SIZE];

    if (check_msg_length(msg, 3, rdata, rdata_len))
        return;

    led = msg->data[0];
    color = msg->data[1];
    time = msg->data[2];
    if (led == 0 ||
        led > sys_devices.status_led_number + sys_devices.fan_led_number || 
        color > MLX_LED_COLOR_AMBER ||
        color < MLX_LED_COLOR_RED ||
        (time != MLX_LED_BLINK_OFF && 
         time != MLX_LED_BLINK_3HZ && 
         time != MLX_LED_BLINK_6HZ)) {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    if (led <= sys_devices.status_led_number) {
        switch (color) {
        case MLX_LED_COLOR_RED:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", MLX_LED_STATUS_FILE, MLX_LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", MLX_LED_STATUS_FILE,MLX_LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case MLX_LED_COLOR_GREEN:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%s", MLX_LED_STATUS_FILE, MLX_LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%s", MLX_LED_STATUS_FILE,MLX_LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case MLX_LED_COLOR_AMBER:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%samber/%s", MLX_LED_STATUS_FILE, MLX_LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%samber/%s", MLX_LED_STATUS_FILE,MLX_LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        default:
            break;
        }
    }
    else if (led > sys_devices.status_led_number && 
             led < sys_devices.status_led_number + sys_devices.fan_led_number) {
        switch (color) {
        case MLX_LED_COLOR_RED:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sred/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number,MLX_LED_DELAY_ON);
            f_delayon = fopen(fname, "w");
            break;
        case MLX_LED_COLOR_GREEN:
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_DELAY_OFF);
            f_delayoff = fopen(fname, "w");
            memset(fname, 0, sizeof(fname));
            sprintf(fname, "%sgreen/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number,MLX_LED_DELAY_ON);
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


static unsigned char mlx_get_led_color(unsigned int led, unsigned char *color)
{
    FILE *f_green;
    FILE *f_red;
    FILE *f_amber;
    char line_green[MLX_READ_BUF_SIZE];
    char line_red[MLX_READ_BUF_SIZE];
    char line_amber[MLX_READ_BUF_SIZE];
    int red, green, amber;
    char fname[MLX_FILE_NAME_SIZE];

    if (led <= sys_devices.status_led_number) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%samber/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);

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
        sprintf(fname, "%sgreen/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);
        f_green = fopen(fname, "r");

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sred/%s", MLX_LED_STATUS_FILE, MLX_LED_BRIGHTNESS);
        f_red = fopen(fname, "r");
    }
    else {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sgreen/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_BRIGHTNESS);
        f_green = fopen(fname, "r");

        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%sred/%u/%s", MLX_LED_FAN_FILE, led - sys_devices.status_led_number, MLX_LED_BRIGHTNESS);
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
            *color = MLX_LED_COLOR_AMBER;
        else if (!amber && green && !red)
            *color = MLX_LED_COLOR_GREEN;
        else
            *color = MLX_LED_COLOR_RED;

        fclose(f_amber);
    }
    else {
        if (!red && green)
            *color = MLX_LED_COLOR_GREEN;
        else
            *color = MLX_LED_COLOR_RED;
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
    unsigned char color = MLX_LED_COLOR_RED;
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

    rv = mlx_get_led_color(led, &color);

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
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, mc->ipmb, 
                             MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_OS_WARM_RESET_EVENT);

        if (mlx_reset_monitor_timer)
            sys->free_timer(mlx_reset_monitor_timer);

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
    sys_data_t *sys = cb_data;
    unsigned char status_led_run_str[MLX_SYS_CMD_BUF_SIZE];
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

    memset(status_led_run_str, 0, sizeof(status_led_run_str));
    if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",MLX_CPU_REBOOT_LOG_NUM, MLX_EVENT_DEASSERTED, 0))
        system(status_led_run_str);

    memset(status_led_run_str, 0, sizeof(status_led_run_str));
    if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",MLX_CPU_READY_LOG_NUM, ready, IPMI_SENSOR_TYPE_PROCESSOR))
        system(status_led_run_str);

    /* CPU started - deasert the CPU fault */
    memset(status_led_run_str, 0, sizeof(status_led_run_str));
    if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n", MLX_CPU_FAULT_LOG_NUM, MLX_EVENT_DEASSERTED, 0))
        system(status_led_run_str);

    /* Stop monitorring the cpu status*/
    sys->stop_timer(mlx_cpu_status_monitor_timer);

    if (ready) {
        /* set "Presence detected" if CPU started successfully */
        memset(status_led_run_str, 0, sizeof(status_led_run_str));
        if (sprintf(status_led_run_str,"echo %u > %s", MLX_CPU_STATUS_PRESENCE_DETECTED, MLX_CPU_STATUS_FILE))
            system(status_led_run_str);

        /* Set the poh_start_point */
        set_poh_start_point();
    }
    else {
        /* set "IERR" in case something goes wrong on CPU sturtup */
        memset(status_led_run_str, 0, sizeof(status_led_run_str));
        if (sprintf(status_led_run_str,"echo %u > %s", MLX_CPU_STATUS_IERR, MLX_CPU_STATUS_FILE))
            system(status_led_run_str);
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
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_OS_CRITICAL_STOP , MLX_CPU_STATUS_SENSOR_NUM, 
                                 MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_OS_SHUTDOWN_EVENT);
        else
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_OS_CRITICAL_STOP , 0, 
                                 MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_OS_SHUTDOWN_EVENT);

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
    char trigger[MLX_SYS_CMD_BUF_SIZE];
    FILE *ftrigger;
    sys_data_t *sys = cb_data;
    struct timeval tv;
    int rv;

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
        mlx_set_led_command(1, MLX_LED_COLOR_GREEN, 1);

       /* set 0 state at CPU reboot */
        memset(trigger, 0, sizeof(trigger));
        if (sprintf(trigger,"echo %u > %s", MLX_CPU_STATUS_NONE, MLX_CPU_STATUS_FILE))
            system(trigger);

        /* set LED blinking on CPU restart */
        memset(trigger, 0, sizeof(trigger));
        if (sprintf(trigger,"status_led.py 0x%02x %d 0x%02x\n", MLX_CPU_REBOOT_LOG_NUM, MLX_EVENT_ASSERTED, 0))
            system(trigger);
        else
            sys->log(sys, OS_ERROR, NULL,"Unable to set status LED blinking");
    }

    fclose(freset);

    if (cpu_reboot_cmd) {
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, MLX_CPU_STATUS_SENSOR_NUM, 
                             MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_SYS_RESTART_EVENT); 
    }
    else
        mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_BOOT_INITIATED, 0, 
                             MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_SYS_RESTART_EVENT); 

    tv.tv_sec = MLX_CPU_STATUS_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(mlx_cpu_status_monitor_timer, &tv);

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
    char sel_set_cmd_buf[MLX_SYS_CMD_BUF_SIZE];

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
    char fname[MLX_FILE_NAME_SIZE];

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

static unsigned char mlx_chassis_power_on_off(unsigned char val)
{
    FILE *file;
    char cmd[MLX_SYS_CMD_BUF_SIZE];

    file = fopen(MLX_PS1_ON, "w");

    if (!file)
        goto ps2_on;

    fprintf(file, "%u", !val);
    fclose(file);

 ps2_on:
    file = fopen(MLX_PS2_ON, "w");

    if (!file)
        goto out;

    fprintf(file, "%u", !val);
    fclose(file);

    /* set "Disabled" state on power-off */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd, "echo %u > %s", MLX_CPU_STATUS_DISABLED, MLX_CPU_STATUS_FILE))
        system(cmd);

 out:
    if (val) 
        chassis_status |= val;
    else {
        /* clear the power status bit */
        chassis_status &= MLX_CHASSIS_POWER_ON_CLEAR;
    }

    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd, "echo %u > /bsp/environment/chassis_status", chassis_status))
        system(cmd);

    return 0;
}

/*
 * Chassis control for the chassis
 */
static int
mlx_set_chassis_control(lmc_data_t *mc, int op, unsigned char *val,
                        void *cb_data)
{
    FILE *freset;

    switch (op) {
    case CHASSIS_CONTROL_POWER:
        mlx_chassis_power_on_off(*val);
        break;
    case CHASSIS_CONTROL_BOOT_INFO_ACK:
    case CHASSIS_CONTROL_BOOT:
    case CHASSIS_CONTROL_GRACEFUL_SHUTDOWN:
        break;
    case CHASSIS_CONTROL_RESET:
        freset = fopen(MLX_SYS_HARD_RESET, "w");

        if (!freset) {
                return ETXTBSY;
        } else {
            mlx_add_event_to_sel(mc, IPMI_SENSOR_TYPE_SYSTEM_EVENT, 0 , 
                                 MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_SYS_BOOT_EVENT);
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
    char line_pin[MLX_READ_BUF_SIZE];
    unsigned char total = 0;
    FILE *fpin;

    for (int i = 0; i < MLX_PSU_COUNT; ++i) {
        char filename[MLX_FILE_NAME_SIZE];
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
    char uptime[MLX_READ_BUF_SIZE];
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

/**
 *
 * ipmitool chassis poh
 *
**/
static void
handle_get_poh_counter_cmd(lmc_data_t    *mc,
                           msg_t         *msg,
                           unsigned char *rdata,
                           unsigned int  *rdata_len,
                           void          *cb_data)
{
    char uptime[MLX_READ_BUF_SIZE];
    char line_status[MLX_READ_BUF_SIZE];
    unsigned long long int val = 0;
    unsigned long int status = 0;
    FILE *fuptime;
    FILE *fstatus;
    sys_data_t *sys = cb_data;

    fstatus = fopen(MLX_CPU_STATUS_FILE, "r");

    if (!fstatus)
        return;

    if (0 >= fread(line_status, 1, sizeof(line_status),fstatus)) {
        fclose(fstatus);
        return;
    }

    status = strtoul(line_status, NULL, 0);

    if (status != MLX_CPU_STATUS_PRESENCE_DETECTED) {
        memset(rdata, 0, 6);
        *rdata_len = 6;
        fclose(fstatus);
        return;
    }

    memset(uptime, 0, sizeof(uptime));
    fuptime = fopen(MLX_UPTIME_FILE, "r");

    if (!fuptime) {
        sys->log(sys, OS_ERROR, NULL,"Unable to open  uptime file");
        rdata[0] = IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
        *rdata_len = 1;
        return;
    }

    fscanf(fuptime, "%s", uptime);
    fclose(fuptime);
    val = (strtouq(uptime, NULL, 0) - poh_start_point)/60;

    rdata[0] = 0;
    rdata[1] = 1;
    rdata[2] = MLX_GET_BYTE(val, 0);
    rdata[3] = MLX_GET_BYTE(val, 1);
    rdata[4] = MLX_GET_BYTE(val, 2);
    rdata[5] = MLX_GET_BYTE(val, 3);

    *rdata_len = 6;
    return;
}

/**
 *
 * ipmitool raw 0x32 0xa5 <timezone>
 *
**/
static void
handle_set_timezone(lmc_data_t    *mc,
                  msg_t         *msg,
                  unsigned char *rdata,
                  unsigned int  *rdata_len,
                  void          *cb_data)
{
    char tz[MLX_TIMEZONE_BUF_SIZE];
    char cmd[MLX_SYS_CMD_BUF_SIZE];

    system("timedatectl set-ntp 0");

    memset(tz, 0, sizeof(tz));
    strncpy(tz, msg->data, msg->len);

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "timedatectl set-timezone %s", tz);
    if (system(cmd))
    {
        rdata[0] = IPMI_INVALID_DATA_FIELD_CC;
        *rdata_len = 1;
        return;
    }

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "ln -sf /usr/share/zoneinfo/%s /etc/localtime", tz);
    system(cmd);

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "echo %s > /etc/timezone");
    system(cmd);

    system("timedatectl set-local-rtc 1");
    system("hwclock -s");

    mlx_sel_time_update(mc);

    rdata[0] = 0;
    *rdata_len = 1;
    return;
}

static void
mlx_reset_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    int i;
    struct timeval tv;
    int fd;
    int rv;
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];

    for (i = 0; i < MLX_RESET_CAUSE_MAX; ++i) {
        unsigned char c = 0;
        int active = 0;

        fd = open(reset_cause[i], O_RDONLY);

        rv = read(fd, &c, 1);
        if (rv != 1) {
            sys->log(sys, OS_ERROR, NULL, "Warning: filed to read '%s' file", reset_cause[i]);
            continue;
        }
        active = atoi(&c);
        switch (i) {
        case MLX_RESET_CAUSE_AC_POWER_CYCLE:
            if (active && !(reset_logged & (1 << MLX_RESET_CAUSE_AC_POWER_CYCLE))) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
                data[1] = MLX_AC_PWR_CYCLE_EVENT;
                mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
                reset_logged |= 1 << MLX_RESET_CAUSE_AC_POWER_CYCLE;
            }
            else if (!active && (reset_logged & (1 << MLX_RESET_CAUSE_AC_POWER_CYCLE)))
                reset_logged ^= 1 << MLX_RESET_CAUSE_AC_POWER_CYCLE;
            break;

        case MLX_RESET_CAUSE_DC_POWER_CYCLE:
            if (active && !(reset_logged & (1 << MLX_RESET_CAUSE_DC_POWER_CYCLE))) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
                data[1] = MLX_DC_PWR_CYCLE_EVENT;
                mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
                reset_logged |= 1 << MLX_RESET_CAUSE_DC_POWER_CYCLE;
            }
            else if (!active && (reset_logged & (1 << MLX_RESET_CAUSE_DC_POWER_CYCLE)))
                reset_logged ^= 1 << MLX_RESET_CAUSE_DC_POWER_CYCLE;
            break;


        case MLX_RESET_CAUSE_CPU_POWER_DOWN:
            if (active && !(reset_logged & (1 << MLX_RESET_CAUSE_CPU_POWER_DOWN))) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
                data[1] = MLX_CPU_PWR_DOWN_EVENT;
                mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
                reset_logged |= 1 << MLX_RESET_CAUSE_CPU_POWER_DOWN;
           }
            else if (!active && (reset_logged & (1 << MLX_RESET_CAUSE_CPU_POWER_DOWN)))
                reset_logged ^= 1 << MLX_RESET_CAUSE_CPU_POWER_DOWN;

            break;
        }
        close(fd);
    }

    tv.tv_sec = MLX_RESET_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(mlx_reset_monitor_timer, &tv);
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
mlx_fans_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    struct timeval tv;
    int failed = 0;
    int i = 0;
    char fname[MLX_FILE_NAME_SIZE];
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];
    char chassis_status_str[MLX_SYS_CMD_BUF_SIZE];

    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "/bsp/fan/tacho%i_rpm", i);
        if (access(fname, F_OK) != 0)
            ++failed;
    }

    /* All FANs failure monitoring */
    if (failed == sys_devices.fan_number * sys_devices.fan_tacho_per_drw) {
        if (all_fans_failure == 0) {
            mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_FAN, 0 , 
                                 MLX_EVENT_ASSERTED, IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
            all_fans_failure = 1;
            chassis_status |= all_fans_failure << MLX_FANS_FAILURE_SHIFT;
        }
    }
    else if (all_fans_failure == 1) {
        mlx_add_event_to_sel(sys->mc, IPMI_SENSOR_TYPE_FAN, 0 , 
                             MLX_EVENT_DEASSERTED, IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
        all_fans_failure = 0;
        chassis_status &= MLX_FANS_FAILURE_CLEAR_MASK;
    }

    memset(chassis_status_str, 0, sizeof(chassis_status_str));
    if (sprintf(chassis_status_str,"echo %u > /bsp/environment/chassis_status", chassis_status))
        system(chassis_status_str);

    /* Abnormal FAN speed monitoring */
    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        FILE *file;
        unsigned long int rpm, pwm;
        unsigned int expected_speed;
        char line[MLX_READ_BUF_SIZE];

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

            data[1] = MLX_FAN_STOPPED_EVENT;
            data[2] = MLX_FAN1_1_SENSOR_NUM + i - 1; /*FAN# */

            mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
        }
        else {
            expected_speed = sys_devices.get_fan_speed(i, pwm);

            if (rpm < expected_speed * (1 - sys_devices.fan_speed_deviation)) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

                data[1] = MLX_FAN_SPEED_TOO_LOW_EVENT;
                data[2] = MLX_FAN1_1_SENSOR_NUM + i -1;/* FAN# */

                mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
            }
            if (rpm > expected_speed * (1 + sys_devices.fan_speed_deviation)) {
                memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);

                data[1] = MLX_FAN_SPEED_TOO_HIGH_EVENT;
                data[2] = MLX_FAN1_1_SENSOR_NUM + i -1; /* FAN# */

                mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
            }
        }
    }

    tv.tv_sec = MLX_FANS_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(mlx_fans_monitor_timer, &tv);
}

static void
mlx_overheat_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    struct timeval tv;
    FILE *file;
    long int cpu_temp;
    unsigned long int asic_temp;
    char line[MLX_READ_BUF_SIZE];
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];

    file = fopen(MLX_CPU_TEMPERATURE_FILE, "r");

    if (!file)
        goto asic_monitor;

    memset(line, 0, sizeof(line));
    if (0 >= fread(line, 1, sizeof(line),file)) {
        fclose(file);
        goto asic_monitor;
    }

    errno =0;
    cpu_temp = strtol(line, NULL, 0);
    if (errno == ERANGE)
    {
        syslog(LOG_ERR, "MLX_CPU_TEMPERATURE_FILE read out of range.");
        cpu_temp = 0;
    }
    fclose(file);
    if(cpu_temp > 0) {
        if (cpu_temp > MLX_CPU_MAX_TEMP) {
            file = fopen(MLX_CPU_HARD_RESET, "w");

            if (!file) {
                sys->log(sys, OS_ERROR, NULL, "CPU temperature is too high! Unable to power-off the CPU!");
            } else {
                fprintf(file, "0");
                fclose(file);
            }

            memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
            data[1] = MLX_CPU_OVERHEAT_EVENT;
            data[2] = cpu_temp/1000;
            mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);

        }
    } else {
        syslog(LOG_ERR, "MLX_CPU_TEMPERATURE_FILE read neagtive value: %d", cpu_temp);
    }

 asic_monitor:
    file = fopen(MLX_ASIC_TEMPERATURE_FILE, "r");

    if (!file)
        goto out;

    memset(line, 0, sizeof(line));
    if (0 >= fread(line, 1, sizeof(line),file)) {
        fclose(file);
        goto out;
    }

    asic_temp = strtoul(line, NULL, 0);
    fclose(file);

    if (asic_temp > MLX_ASIC_MAX_TEMP) {
        mlx_chassis_power_on_off(0);
        memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
        data[1] = MLX_ASIC_OVERHEAT_EVENT;
        data[2] = asic_temp/1000;
        mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);
    }

 out:
    tv.tv_sec = MLX_OVERHEAT_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(mlx_overheat_monitor_timer, &tv);
}

static void
mlx_wd_monitor_timeout(void *cb_data)
{
    sys_data_t *sys = cb_data;
    struct timeval tv;
    FILE *file;
    long int timeout_status;
    char cmd[MLX_SYS_CMD_BUF_SIZE];
    char status_buf[MLX_READ_BUF_SIZE];
    unsigned char data[MLX_EVENT_TO_SEL_BUF_SIZE];
    unsigned char status;
    unsigned int tmp;

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd,"devmem %s > %s", MLX_WD_TIMEOUT_STATUS_REG, MLX_WD_TIMEOUT_CURRENT_STATUS_FILE);
    system(cmd);

    file = fopen(MLX_WD_TIMEOUT_CURRENT_STATUS_FILE, "r");
    if (!file)
        goto control;

    memset(status_buf, 0, sizeof(status_buf));
    if (0 >= fread(status_buf, 1, sizeof(status_buf),file)) {
        fclose(file);
        goto out;
    }

    fclose(file);
    tmp = strtoul(status_buf, NULL, 0);
    status = MLX_GET_BYTE(tmp,1) & 0xF;

    if (status != mlx_wd_status) {
        memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
        data[1] = MLX_WD_EXPIRED_EVENT;
        mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);

        mlx_wd_status = status;
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo %u > %s", status, MLX_WD_TIMEOUT_SAVED_STATUS_FILE);
        system(cmd);
    }

 control:
    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd,"devmem %s > %s", MLX_WD_CONTROL_REG, MLX_WD_CONTROL_CURRENT_STATUS_FILE);
    system(cmd);

    file = fopen(MLX_WD_CONTROL_CURRENT_STATUS_FILE, "r");
    if (!file)
        goto out;

    memset(status_buf, 0, sizeof(status_buf));
    if (0 >= fread(status_buf, 1, sizeof(status_buf),file)) {
        fclose(file);
        goto out;
    }

    fclose(file);
    tmp = strtoul(status_buf, NULL, 0);
    status = tmp & 1;

    if (status && status != mlx_wd_control) {
        memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
        data[1] = MLX_WD_STARTED_EVENT;
        mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);

        mlx_wd_control = status;
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo %u > %s", status, MLX_WD_CONTROL_SAVED_STATUS_FILE);
        system(cmd);
    }
    else if (!status && status != mlx_wd_control) {
        memset(data, 0, MLX_EVENT_TO_SEL_BUF_SIZE);
        data[1] = MLX_WD_STOPPED_EVENT;
        mc_new_event(bmc_mc, MLX_OEM_SEL_RECORD_TYPE, data);

        mlx_wd_control = status;
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo %u > %s", status, MLX_WD_CONTROL_SAVED_STATUS_FILE);
        system(cmd);
    }

 out:
    tv.tv_sec = MLX_WD_MONITOR_TIMEOUT;
    tv.tv_usec = 0;
    sys->start_timer(mlx_wd_monitor_timer, &tv);
}

/*
 * Chassis control get for the chassis.
 */
static int
mlx_get_chassis_control(lmc_data_t *mc, int op, unsigned char *val,
			void *cb_data)
{
    *val = 0;
    return IPMI_COULD_NOT_PROVIDE_RESPONSE_CC;
}

void mlx_set_sys_time(unsigned char* data)
{
    struct tm *nowtm;
    char tmbuf[MLX_SYS_CMD_BUF_SIZE];
    uint32_t timeval = ipmi_get_uint32(data);

    time_t nowtime = *((time_t*)&timeval);
    nowtm = localtime(&nowtime);
    system("timedatectl set-ntp false");

    memset(tmbuf, 0, sizeof(tmbuf));
    strftime(tmbuf, sizeof(tmbuf), "timedatectl set-time %Y-%m-%d", nowtm);
    system(tmbuf);

    memset(tmbuf, 0, sizeof(tmbuf));
    strftime(tmbuf, sizeof(tmbuf), "timedatectl set-time %H:%M:%S", nowtm);
    system(tmbuf);

    system("timedatectl set-local-rtc 1");
}

void mlx_get_chassis_status(unsigned char* data, unsigned int  *data_len)
{
    data[0] = 0;
    data[1] = MLX_GET_BYTE(chassis_status, 0);
    data[2] = MLX_GET_BYTE(chassis_status, 1);
    data[3] = MLX_GET_BYTE(chassis_status, 2);
    data[4] = MLX_GET_BYTE(chassis_status, 3);

    if (data[4])
        *data_len = 5;
    else
        *data_len = 4;
}

void 
mlx_status_led_control(unsigned char num,
                       unsigned char direction,
                       unsigned char type)
{
    unsigned char status_led_run_str[MLX_SYS_CMD_BUF_SIZE];
    FILE *fstatus;
    unsigned long int status;
    char line_status[MLX_READ_BUF_SIZE];

    memset(status_led_run_str, 0, sizeof(status_led_run_str));

    switch (type) {
    case IPMI_SENSOR_TYPE_TEMPERATURE:
        switch (num) {
        case MLX_AMBIENT_CARRIER_TEMP_SENSOR_NUM:
        case MLX_AMBIENT_SWITCH_TEMP_SENSOR_NUM:
            if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",num, direction, type))
                 system(status_led_run_str);
            break;
        case MLX_ASIC_TEMP_SENSOR_NUM:

            fstatus = fopen(MLX_CPU_STATUS_FILE, "r");

            if (!fstatus)
                return;

            if (0 >= fread(line_status, 1, sizeof(line_status),fstatus)) {
                fclose(fstatus);
                return;
            }

            status = strtoul(line_status, NULL, 0);
            if (status) {
                if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",num, direction, type))
                     system(status_led_run_str);
            }
            fclose(fstatus);
            break;
        default:
            break;
        }
        break;
    case IPMI_SENSOR_TYPE_VOLTAGE:
        switch (num) {
        case MLX_UCD_3_3V_SENSOR_NUM:
        case MLX_UCD_1_2V_SENSOR_NUM:
        case MLX_UCD_VCORE_SENSOR_NUM:
        case MLX_PSU1_VIN_SENSOR_NUM:
        case MLX_PSU2_VIN_SENSOR_NUM:
        case MLX_A2D_1_8V_SENSOR_NUM:
            if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",num, direction, type))
                 system(status_led_run_str);
            break;
        default:
            break;
        }
        break;
    case IPMI_SENSOR_TYPE_CURRENT:
        switch (num) {
        case MLX_VCORE_UCD_CURR_SENSOR_NUM:
        case MLX_UCD_3_3V_SEN_CURR_SENSOR_NUM:
        case MLX_UCD_1_2V_CURR_SENSOR_NUM:
            if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",num, direction, type))
                 system(status_led_run_str);
            break;
        default:
            break;
        }
        break;
    case IPMI_SENSOR_TYPE_FAN:
        switch (num) {
        case MLX_FAN1_1_SENSOR_NUM:
        case MLX_FAN1_2_SENSOR_NUM:
        case MLX_FAN2_1_SENSOR_NUM:
        case MLX_FAN2_2_SENSOR_NUM:
        case MLX_FAN3_1_SENSOR_NUM:
        case MLX_FAN3_2_SENSOR_NUM:
        case MLX_FAN4_1_SENSOR_NUM:
        case MLX_FAN4_2_SENSOR_NUM:
            if (sprintf(status_led_run_str,"status_led.py 0x%02x %d 0x%02x\n",num, direction, type))
                 system(status_led_run_str);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void
mlx_log_device_status(int poll_status , sensor_t *sensor)
{
    if (poll_status) {
        if (sensor->enabled) {
            mlx_status_led_control(sensor->num, 0, sensor->sensor_type);
            sensor->enabled = 0;
            /* In case PSU1 or PSU2 is power-off/plugged-out add msg to the SEL */
            switch (sensor->num) {
            case MLX_PSU1_PIN_SENSOR_NUM:
            if (access("/bsp/fru/psu1_eeprom", F_OK) == 0) /* AC lost or out-of-range */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_PWR_AC_OUT_OF_RANGE_EVENT);
            else /* Power Supply AC lost */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_PWR_AC_LOST_EVENT);
            break;
            case MLX_PSU2_PIN_SENSOR_NUM:
            if (access("/bsp/fru/psu2_eeprom", F_OK) == 0)  /* AC lost or out-of-range */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_PWR_AC_OUT_OF_RANGE_EVENT);
            else  /* Power Supply AC lost */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_PWR_AC_LOST_EVENT);
            break;
                /* In case any FAN is plugged-out sensor can't be read add msg to the SEL */
            case MLX_FAN1_1_SENSOR_NUM:
            case MLX_FAN1_2_SENSOR_NUM:
            if (access("/bsp/fru/fan1_eeprom", F_OK) == 0) /* "Availability State",  "Device Disabled" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
            else /* "Availability State",  "Device Absent" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_PRESENCE, MLX_DEVICE_ABSENT_EVENT);
            break;
            case MLX_FAN2_1_SENSOR_NUM:
            case MLX_FAN2_2_SENSOR_NUM:
            if (access("/bsp/fru/fan2_eeprom", F_OK) == 0)  /* "Availability State",  "Device Disabled" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
            else  /* "Availability State",  "Device Absent" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_PRESENCE, MLX_DEVICE_ABSENT_EVENT);
            break;
            case MLX_FAN3_1_SENSOR_NUM:
            case MLX_FAN3_2_SENSOR_NUM:
            if (access("/bsp/fru/fan3_eeprom", F_OK) == 0) /* "Availability State",  "Device Disabled" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
            else /* "Availability State",  "Device Absent" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_PRESENCE, MLX_DEVICE_ABSENT_EVENT);
            break;
            case MLX_FAN4_1_SENSOR_NUM:
            case MLX_FAN4_2_SENSOR_NUM:
            if (access("/bsp/fru/fan4_eeprom", F_OK) == 0) /* "Availability State",  "Device Disabled" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_DISABLED_EVENT);
            else /* "Availability State",  "Device Absent" */
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_PRESENCE, MLX_DEVICE_ABSENT_EVENT);
            break;
            default:
                break;
            }
        }
    } else {
        if (!sensor->enabled) {
            mlx_status_led_control(sensor->num, 1, sensor->sensor_type);
            sensor->enabled = 1;
            switch (sensor->num) {
            /* In case PSU1 or PSU2 is power-on add msg to the SEL */
            case MLX_PSU1_PIN_SENSOR_NUM:
            case MLX_PSU2_PIN_SENSOR_NUM:
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_PSU_PRESENT_EVENT);
            break;
            /* In case FAN is plugged-in add msg to the SEL */
            case MLX_FAN1_1_SENSOR_NUM:
            case MLX_FAN1_2_SENSOR_NUM:
            case MLX_FAN2_1_SENSOR_NUM:
            case MLX_FAN2_2_SENSOR_NUM:
            case MLX_FAN3_1_SENSOR_NUM:
            case MLX_FAN3_2_SENSOR_NUM:
            case MLX_FAN4_1_SENSOR_NUM:
            case MLX_FAN4_2_SENSOR_NUM:
                mlx_add_event_to_sel(sensor->mc, sensor->sensor_type, sensor->num, MLX_EVENT_ASSERTED, 
                 IPMI_EVENT_READING_TYPE_DISCRETE_DEVICE_ENABLE, MLX_DEVICE_ENABLED_EVENT);
            break;
            default:
                break;
            }
        }
    }
}

void mlx_pef_action_apply(lmc_data_t    *mc,
                        unsigned char record_type,
                        unsigned char event[13])
{
    unsigned int i;

    for (i=1; i<MAX_EVENT_FILTERS; i++) {
        if (!(mc->pef.event_filter_table[i][1] & 0x80))
            continue;

        if ((mc->pef.event_filter_table[i][5] != 0xff) 
            && (mc->pef.event_filter_table[i][5] != event[4]))
            continue;

        if ((mc->pef.event_filter_table[i][6] != 0xff) 
            && (mc->pef.event_filter_table[i][6] != event[5]))
            continue;

        if ((mc->pef.event_filter_table[i][7] != 0xff) 
            && (mc->pef.event_filter_table[i][7] != event[7]))
            continue;

        if ((mc->pef.event_filter_table[i][8] != 0xff) 
            && (mc->pef.event_filter_table[i][8] != event[8]))
            continue;

        if ((mc->pef.event_filter_table[i][9] != 0xff) 
            && (mc->pef.event_filter_table[i][9] != event[12]))
            continue;

        if ((mc->pef.event_filter_table[i][2] & 0x10)
            && (mc->pef.pef_action_global_control & 0x10)) /* OEM */
            system("echo 3 > /bsp/leds/status/amber/trigger");

        if ((mc->pef.event_filter_table[i][2] & 0x2)
            && (mc->pef.pef_action_global_control & 0x2))  /* Power-off */
            system("echo 0 > /bsp/reset/cpu_reset_hard");

        if ((mc->pef.event_filter_table[i][2] & 0x4)
            && (mc->pef.pef_action_global_control & 0x4))  /* Reset */
            system("echo 0 > /bsp/reset/cpu_reset_soft");

        if ((mc->pef.event_filter_table[i][2] & 0x8)
            && (mc->pef.pef_action_global_control & 0x8)) { /* Power cycle */
            system("echo 0 > /bsp/reset/cpu_reset_hard");
            sleep(3);
            system("echo 1 > /bsp/reset/cpu_reset_hard");
        }

        if ((mc->pef.event_filter_table[i][2] & 0x20)
            && (mc->pef.pef_action_global_control & 0x20)) { /* Diagnostic interupt */

            serserv_data_t *si = mc->channels[15]->chan_info;
            unsigned int len = 2;
            unsigned char c[2];
            int rv = 0;

            c[0] = 0x07;
            c[1] = 0xA1;

            si->send_out(si, c, len);
        }

        /* SNMP is not supported, thus skip Alert action */
        if ((mc->pef.event_filter_table[i][2] & 0x1)
            && (mc->pef.pef_action_global_control & 0x1))
            continue;
    }
}

void
mlx_switch_console(unsigned int instance, unsigned char state)
{
    char cmd[MLX_SYS_CMD_BUF_SIZE];
    if (instance == 1) {
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo %u > %s", state, MLX_UART_TO_BMC);
        system(cmd);
    }
}

void
mlx_ipmi_wd_timeout(unsigned char action)
{
    char cmd[MLX_SYS_CMD_BUF_SIZE];

    char status_led_run_str[MLX_SYS_CMD_BUF_SIZE];
    mlx_set_led_command(1, MLX_LED_COLOR_GREEN, 0);
    mlx_set_led_command(1, MLX_LED_COLOR_AMBER, 0);
    mlx_set_led_command(1, MLX_LED_COLOR_RED, 1);

    if (sprintf(cmd, "status_led.py 0x%02x %d 0x%02x\n", MLX_IPMIWD_LOG_NUM, MLX_EVENT_ASSERTED, 0))
        system(cmd);

    /* set "IERR" if IPMI watchdog timeout expired */
    memset(cmd, 0, sizeof(status_led_run_str));
    if (sprintf(cmd,"echo %u > %s", MLX_CPU_STATUS_IERR, MLX_CPU_STATUS_FILE))
        system(cmd);

    switch(action) {
    case IPMI_MC_WATCHDOG_ACTION_NONE:
        /*Uart to BMC*/
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo 0 > %s", MLX_UART_TO_BMC);
        system(cmd);
        mlx_add_event_to_sel(bmc_mc, IPMI_SENSOR_TYPE_WATCHDOG_1, 0 , MLX_EVENT_ASSERTED, 
                             IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_IPMI_WD_EXPIRED_EVENT);
        break;
    case IPMI_MC_WATCHDOG_ACTION_RESET:
        /*CPU Reset*/
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo 0 > %s", MLX_CPU_SOFT_RESET);
        system(cmd);
        mlx_add_event_to_sel(bmc_mc, IPMI_SENSOR_TYPE_WATCHDOG_1, 0 , MLX_EVENT_ASSERTED,
                             IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_IPMI_WD_OS_RESET_EVENT);
        break;
    case IPMI_MC_WATCHDOG_ACTION_POWER_DOWN:
        /*CPU Power-off*/
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo 0 > %s", MLX_CPU_HARD_RESET);
        system(cmd);
        mlx_add_event_to_sel(bmc_mc, IPMI_SENSOR_TYPE_WATCHDOG_1, 0 , MLX_EVENT_ASSERTED, 
                             IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_IPMI_WD_PWR_DOWN_EVENT);
        break;
    case IPMI_MC_WATCHDOG_ACTION_POWER_CYCLE:
        /*CPU power cycle*/
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo 0 > %s", MLX_CPU_HARD_RESET);
        system(cmd);
        sleep(3);
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "echo 1 > %s", MLX_CPU_HARD_RESET);
        system(cmd);
        mlx_add_event_to_sel(bmc_mc, IPMI_SENSOR_TYPE_WATCHDOG_1, 0 , MLX_EVENT_ASSERTED, 
                             IPMI_EVENT_READING_TYPE_SENSOR_SPECIFIC, MLX_IPMI_WD_PWR_CYCLE_EVENT);
        break;
    default:
        break;
    }
}

void
mlx_ipmi_wd_reset(lmc_data_t *mc, struct timeval tv)
{
    unsigned char cmd[MLX_SYS_CMD_BUF_SIZE]; 

    if (mc->watchdog_running != 1) {
        memset(cmd, 0, sizeof(cmd));
        if (sprintf(cmd,"status_led.py 0x%02x %d 0x%02x\n", MLX_IPMIWD_LOG_NUM, MLX_EVENT_DEASSERTED, 0))
            system(cmd);

        /* Stop monitorring the cpu status */
        mc->sysinfo->stop_timer(mlx_cpu_status_monitor_timer);
    }
    mc->sysinfo->stop_timer(mc->watchdog_timer);

    mc->watchdog_running = 1;

    if (mc->sysinfo->start_timer(mc->watchdog_timer, &tv))
        mc->sysinfo->log(mc->sysinfo, OS_ERROR, NULL,
                         "Failed to reset watchdog timer");

    /* set "Presence detected" on IPMI watchdog start */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd,"echo %u > %s\n", MLX_CPU_STATUS_PRESENCE_DETECTED, MLX_CPU_STATUS_FILE))
        system(cmd);
}

void
mlx_panic_event_handler(lmc_data_t *mc)
{
    char cmd[MLX_SYS_CMD_BUF_SIZE];

    /* set LED to red */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd,"status_led.py 0x%02x %d 0x%02x\n", MLX_CPU_FAULT_LOG_NUM, MLX_EVENT_ASSERTED, 0))
        system(cmd);
    else
        mc->sysinfo->log(mc->sysinfo, OS_ERROR, NULL,"Unable to set status LED to red");

    /* set "IERR" CPU state */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd, "echo %u > %s", MLX_CPU_STATUS_IERR, MLX_CPU_STATUS_FILE))
        system(cmd);
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
    char fname[MLX_FILE_NAME_SIZE];
    char status[MLX_READ_BUF_SIZE];
    char cmd[MLX_SYS_CMD_BUF_SIZE];
    FILE *f_status;

    printf("IPMI Mellanox module");

    rv = ipmi_mc_alloc_unconfigured(sys, 0x20, &bmc_mc);
    if (rv) {
	sys->log(sys, OS_ERROR, NULL,
		 "Unable to allocate an mc: %s", strerror(rv));
	return rv;
    }

    rv = mlx_set_fan_enable(MLX_FAN_PWM_ENABLE_FILE);

    if (rv) {
        sys->log(sys, OS_ERROR, NULL,
                 "Unable to enable pwm_en: %s", strerror(rv));
    }

    for (i = 1; i <= sys_devices.fan_number * sys_devices.fan_tacho_per_drw; ++i) {
        memset(fname, 0, sizeof(fname));
        sprintf(fname, "%s%i_en", MLX_FAN_TACHO_FILE, i);
        rv = mlx_set_fan_enable(fname);

        if (rv) {
            sys->log(sys, OS_ERROR, NULL,
                     "Unable to enable tacho for FAN%i: %s", i, strerror(rv));
        }
    }

    f_status = fopen("/bsp/environment/chassis_status", "r");
    if (f_status) {
        memset(status, 0, sizeof(status));
        if (0 >= fread(status, 1, sizeof(status),f_status)) {
            fclose(f_status);
        }
        chassis_status = strtoul(status, NULL, 0);
        fclose(f_status);
    }
    else {
        /* assume that power is on */
        chassis_status = MLX_CHASSIS_POWER_ON_BIT;
        system("echo 1 > /bsp/environment/chassis_status");
    }

    f_status = fopen(MLX_WD_TIMEOUT_SAVED_STATUS_FILE, "r");
    if (f_status) {
        memset(status, 0, sizeof(status));
        if (0 >= fread(status, 1, sizeof(status),f_status)) {
            fclose(f_status);
        }
        mlx_wd_status = strtoul(status, NULL, 0);
        fclose(f_status);
    }
    else {
        /* assume that timeout counter is 0 */
        mlx_wd_status = 0;
        system("echo 0 > /bsp/environment/bmcwd_saved");
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

    rv = ipmi_emu_register_cmd_handler(IPMI_CHASSIS_NETFN, IPMI_GET_POH_COUNTER_CMD,
                                       handle_get_poh_counter_cmd, sys);

    rv = ipmi_emu_register_cmd_handler(IPMI_OEM_MLX_TZ_NETFUNC , IPMI_OEM_MLX_TZ_SET_CMD,
                                       handle_set_timezone, sys);

    ipmi_mc_set_chassis_control_func(bmc_mc, mlx_set_chassis_control,
                                     mlx_get_chassis_control, sys);

    if (rv) {
	sys->log(sys, OS_ERROR, NULL,
		 "Unable to register NEW handler: %s", strerror(rv));
    }

    rv = sys->alloc_timer(sys, mlx_reset_monitor_timeout, sys, &mlx_reset_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create reset monitoring timer");
        return errval;
    } else {
        tv.tv_sec = MLX_RESET_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(mlx_reset_monitor_timer, &tv);
    }

    rv = sys->alloc_timer(sys, mlx_wd_monitor_timeout, sys, &mlx_wd_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create BMC watchdog monitoring timer");
        return errval;
    } else {
        tv.tv_sec = MLX_WD_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(mlx_wd_monitor_timer, &tv);
    }

    rv = sys->alloc_timer(sys, mlx_cpu_status_monitor_timeout, sys, &mlx_cpu_status_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create CPU status monitoring timer");
        return errval;
    } else {
        tv.tv_sec = MLX_CPU_STATUS_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(mlx_cpu_status_monitor_timer, &tv);
    }

    /* set "Disabled" state at startup */
    memset(cmd, 0, sizeof(cmd));
    if (sprintf(cmd,"echo %u > %s", MLX_CPU_STATUS_DISABLED, MLX_CPU_STATUS_FILE))
        system(cmd);

    if (!system("mlx_deftimezone.sh"))
        system("rm /usr/bin/mlx_deftimezone.sh");

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
    }else {
        if (0 >= fread(id_line, 1, sizeof(id_line),fid))
        {
            fclose(fid);
            sys->log(sys, OS_ERROR, NULL, "Unable to read  FW ID file");
        }
        else {
            fclose(fid);

            memcpy(id_maj, id_line+13, sizeof(id_maj));
            memcpy(id_min, id_line+15, sizeof(id_min));

            fw_maj = strtoul(id_maj, NULL, 0);
            fw_min = strtoul(id_min, NULL, 0);

            ipmi_mc_set_fw_revision(bmc_mc, fw_maj, fw_min);
        }
    }

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
        sys_devices.fan_speed_front = mlx_fan_speed_front_profile1;
        sys_devices.fan_speed_rear = mlx_fan_speed_rear_profile1;
        break;
    default:
        break;
    }

    rv = sys->alloc_timer(sys, mlx_fans_monitor_timeout, sys, &mlx_fans_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create FANs monitoring timer");
        return errval;
    }
    else {
        tv.tv_sec = MLX_FANS_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(mlx_fans_monitor_timer, &tv);
    }

    rv = sys->alloc_timer(sys, mlx_overheat_monitor_timeout, sys, &mlx_overheat_monitor_timer);
    if (rv) {
        int errval = errno;
        sys->log(sys, SETUP_ERROR, NULL, "Unable to create overheat monitoring timer");
        return errval;
    } else {
        tv.tv_sec = MLX_OVERHEAT_MONITOR_TIMEOUT;
        tv.tv_usec = 0;
        sys->start_timer(mlx_overheat_monitor_timer, &tv);
    }

    sys->mc->sys_time_set_func = mlx_set_sys_time;
    sys->mc->chassis_status_custom = mlx_get_chassis_status;
    sys->mc->status_led_control = mlx_status_led_control;
    sys->mc->log_device_status = mlx_log_device_status;
    sys->mc->pef_action_apply = mlx_pef_action_apply;
    sys->mc->switch_console = mlx_switch_console;
    sys->mc->ipmi_wd_timeout_custom = mlx_ipmi_wd_timeout;
    sys->mc->ipmi_wd_reset_custom = mlx_ipmi_wd_reset;
    sys->mc->panic_event_handler_custom = mlx_panic_event_handler;

    system("hwclock -s");

    mlx_sel_time_update(sys->mc);

    poh_start_point = 0;

    return 0;
}
