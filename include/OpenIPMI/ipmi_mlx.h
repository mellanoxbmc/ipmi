/*
 * ipmi_mlx.h
 */

#ifndef _IPMI_MLX_H
#define _IPMI_MLX_H

#include <OpenIPMI/mcserv.h>

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

#define MLX_EVENT_TO_SEL_BUF_SIZE           13
#define MLX_EVENT_DIRECTION_SHIFT           7

void mlx_add_event_to_sel(lmc_data_t    *mc,
                          unsigned char sensor_type,
                          unsigned char sensor_num,
                          unsigned char direction,
                          unsigned char event_type,
                          unsigned char offset);

void mlx_mc_pef_apply(lmc_data_t    *mc,
                       unsigned char record_type,
                       unsigned char event[13]);

struct mlx_devices_data {
    unsigned char fan_number;
    unsigned char fan_tacho_number;
    unsigned char fan_eeprom_number;
    unsigned char psu_number;
};

struct mlx_devices_data sys_devices;

#endif /* _IPMI_MLX_H */
