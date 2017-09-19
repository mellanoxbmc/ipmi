/*
 * ipmi_mlx.h
 */

#ifndef _IPMI_MLX_H
#define _IPMI_MLX_H

#include <OpenIPMI/mcserv.h>

/* MLX Sensors */
#define MLX_AMBIENT_CARRIER_TEMP_SENSOR_NUM 0x01
#define MLX_AMBIENT_SWITCH_TEMP_SENSOR_NUM  0X02
#define MLX_PSU1 temp_SENSOR_NUM            0x03
#define MLX_PSU2 temp_SENSOR_NUM            0x04
#define MLX_ASIC_TEMP_SENSOR_NUM            0x05
#define MLX_PSU1_VIN_SENSOR_NUM             0x9A
#define MLX_PSU1_VOUT_SENSOR_NUM            0x9B
#define MLX_PSU2_VIN_SENSOR_NUM             0x9C
#define MLX_PSU2_VOUT_SENSOR_NUM            0x9D
#define MLX_A2D_1_8V_SENSOR_NUM             0x9E
#define MLX_A2D_1_2v_SENSOR_NUM             0x9F
#define MLX_A2D_VCORE_SENSOR_NUM            0xA0
#define MLX_SWB_12V_SENSOR_NUM              0xA1
#define MLX_SWB_3_3V_AUX_SENSOR_NUM         0xA2
#define MLX_SWB_3_3V_SEN_SENSOR_NUM         0xA3
#define MLX_ADC_12V_SENSOR_NUM              0xA4
#define MLX_ADC_5V_SENSOR_NUM               0xA5
#define MLX_ADC_5V_USB_SENSOR_NUM           0xA6
#define MLX_ADC_3_3V_AUX_SENSOR_NUM         0xA7
#define MLX_ADC_3_3V_BMC_SENSOR_NUM         0xA8
#define MLX_ADC_2_5V_DDR_SENSOR_NUM         0xA9
#define MLX_ADC_1_2V_DDR_SENSOR_NUM         0xAA
#define MLX_ADC_1_15V_VCORE                 0XAB
#define MLX_UCD_3_3V_SENSOR_NUM             0xAC
#define MLX_UCD_1_2V_SENSOR_NUM             0xAD
#define MLX_UCD_VCORE_SENSOR_NUM            0xAE
#define MLX_PSU1_PIN_SENSOR_NUM             0x14
#define MLX_PSU1_POUT_SENSOR_NUM            0X15
#define MLX_PSU2_PIN_SENSOR_NUM             0x16
#define MLX_PSU2_POUT_SENSOR_NUM            0X17
#define MLX_PSU1_IIN_SENSOR_NUM             0X1E
#define MLX_PSU1_IOUT_SENSOR_NUM            0X1F
#define MLX_PSU2_IIN_SENSOR_NUM             0X20
#define MLX_PSU2_IOUT_SENSOR_NUM            0X21
#define MLX_VCORE_UCD_CURR_SENSOR_NUM       0x22
#define MLX_UCD_3_3V_SEN_CURR_SENSOR_NUM    0x23
#define MLX_UCD_1_2V_CURR_SENSOR_NUM        0x24
#define MLX_FAN1_1_SENSOR_NUM               0x70
#define MLX_FAN1_2_SENSOR_NUM               0x71
#define MLX_FAN2_1_SENSOR_NUM               0x72
#define MLX_FAN2_2_SENSOR_NUM               0x73
#define MLX_FAN3_1_SENSOR_NUM               0x74
#define MLX_FAN3_2_SENSOR_NUM               0x75
#define MLX_FAN4_1_SENSOR_NUM               0x76
#define MLX_FAN4_2_SENSOR_NUM               0x77
#define MLX_PSU1_FAN_SENSOR_NUM             0x78
#define MLX_PSU2_FAN_SENSOR_NUM             0x79
#define MLX_CPU_STATUS_SENSOR_NUM           0x28

#define MLX_IPMIWD_LOG_NUM                  0xb4
#define MLX_PSU1_DRW_LOG_NUM                0xb5
#define MLX_PSU2_DRW_LOG_NUM                0xb6
#define MLX_FAN1_DRW_LOG_NUM                0xb7
#define MLX_FAN2_DRW_LOG_NUM                0xb8
#define MLX_FAN3_DRW_LOG_NUM                0xb9
#define MLX_FAN4_DRW_LOG_NUM                0xba
#define MLX_CPU_READY_LOG_NUM               0xbb
#define MLX_CPU_REBOOT_LOG_NUM              0xbc

#define MLX_EVENT_TO_SEL_BUF_SIZE           13
#define MLX_EVENT_DIRECTION_SHIFT           7

#define MLX_READ_BUF_SIZE                   10
#define MLX_FILE_NAME_SIZE                  50
#define MLX_SYS_CMD_BUF_SIZE                100

/* Mlx OEM SEL event description */
#define MLX_OEM_SEL_RECORD_TYPE      0xE0
#define MLX_FAN_STOPPED_EVENT        0x0
#define MLX_FAN_SPEED_TOO_LOW_EVENT  0x1
#define MLX_FAN_SPEED_TOO_HIGH_EVENT 0x2
#define MLX_ASIC_OVERHEAT_EVENT      0x3
#define MLX_CPU_OVERHEAT_EVENT       0x4
#define MLX_AC_PWR_CYCLE_EVENT       0x5
#define MLX_DC_PWR_CYCLE_EVENT       0x6
#define MLX_CPU_PWR_DOWN_EVENT       0x7

/* Event directions */
#define MLX_EVENT_ASSERTED           0x0
#define MLX_EVENT_DEASSERTED         0x1

/* Event description offset */
#define MLX_DEVICE_DISABLED_EVENT     0x0
#define MLX_DEVICE_ENABLED_EVENT      0x1
#define MLX_DEVICE_ABSENT_EVENT       0x0
#define MLX_IPMI_WD_OS_RESET_EVENT    0x1
#define MLX_IPMI_WD_PWR_DOWN_EVENT    0x3
#define MLX_IPMI_WD_PWR_CYCLE_EVENT   0x4
#define MLX_IPMI_WD_EXPIRED_EVENT     0x6 
#define MLX_PWR_AC_LOST_EVENT         0x3
#define MLX_PWR_AC_OUT_OF_RANGE_EVENT 0x4
#define MLX_OS_WARM_RESET_EVENT       0x6
#define MLX_SYS_RESTART_EVENT         0x7
#define MLX_OS_SHUTDOWN_EVENT         0x3
#define MLX_SYS_BOOT_EVENT            0x1
#define MLX_PSU_PRESENT_EVENT         0x0
#define MLX_WD_STARTED_EVENT          0x8
#define MLX_WD_STOPPED_EVENT          0x9
#define MLX_WD_EXPIRED_EVENT          0xA

#define MLX_CHASSIS_POWER_ON_BIT      1
#define MLX_CHASSIS_POWER_ON_CLEAR    0xFFFFFFFE
#define MLX_FANS_FAILURE_SHIFT        19
#define MLX_FANS_FAILURE_CLEAR_MASK   0x7FFFF

#define MLX_WD_TIMEOUT_STATUS_REG  "0x1e785050"
#define MLX_WD_CONTROL_REG         "0x1e78504c"

#define MLX_GET_BYTE(val, num)      ((val >> 8*num) & 0xFF)

typedef unsigned int (*mlx_get_expected_fan_speed)(unsigned char tacho_num, unsigned char fan_pwm);

struct mlx_devices_data {
    unsigned char fan_number;
    unsigned char fan_tacho_per_drw;
    unsigned char fan_pwm_max;
    unsigned char fan_eeprom_number;
    unsigned char psu_number;
    unsigned char status_led_number;
    unsigned char fan_led_number;
    float fan_speed_deviation;
    mlx_get_expected_fan_speed get_fan_speed;
    unsigned int *fan_speed_front;
    unsigned int *fan_speed_rear;
};

struct mlx_devices_data sys_devices;

#endif /* _IPMI_MLX_H */
