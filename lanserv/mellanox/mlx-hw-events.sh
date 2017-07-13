#!/bin/bash

########################################################################
# Copyright (c) 2017 Mellanox Technologies.
# Copyright (c) 2017 Nataliya Yakuts <nataliyay@mellanox.com>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

if [ "$1" == "add" ]; then
  if [ "$2" == "amb_carrier" ] || [ "$2" == "amb_switch" ]; then
    mkdir -p /bsp/thermal/
    ln -sf $3$4/temp1_input /bsp/thermal/$2_temp
    ln -sf $3$4/temp1_max /bsp/thermal/$2_temp_max
    ln -sf $3$4/temp1_max_hyst /bsp/thermal/$2_temp_hyst
  fi
  if [ "$2" == "psu1" ] || [ "$2" == "psu2" ]; then
    mkdir -p /bsp/thermal/
    mkdir -p /bsp/environment/
    mkdir -p /bsp/fan/
    ln -sf $3$4/temp1_input /bsp/thermal/$2_temp
    ln -sf $3$4/temp1_max /bsp/thermal/$2_temp_max
    ln -sf $3$4/temp1_max_alarm /bsp/thermal/$2_temp_alarm
    ln -sf $3$4/in1_input /bsp/environment/$2_vin
    ln -sf $3$4/in2_input /bsp/environment/$2_vout
    ln -sf $3$4/power1_input /bsp/environment/$2_pin
    ln -sf $3$4/power2_input /bsp/environment/$2_pout
    ln -sf $3$4/curr1_input /bsp/environment/$2_iin
    ln -sf $3$4/curr2_input /bsp/environment/$2_iout
    ln -sf $3$4/fan1_input /bsp/fan/$2_fan_input

    #FAN speed set
    if [ "$2" == "psu1" ]; then
      i2cset -f -y 3 0x58 0x3b 0x3c wp
    else
      i2cset -f -y 3 0x59 0x3b 0x3c wp
    fi
  fi
  if [ "$2" == "A2D" ]; then
    mkdir -p /bsp/environment/
    ln -sf $3$4/in_voltage-voltage_scale /bsp/environment/$2_voltage_scale
    ln -sf $3$4/in_voltage6_raw /bsp/environment/$2_1_8v
    ln -sf $3$4/in_voltage5_raw /bsp/environment/$2_1_2v
    ln -sf $3$4/in_voltage4_raw /bsp/environment/$2_vcore
    ln -sf $3$4/in_voltage3_raw /bsp/environment/$2_swb_12v
    ln -sf $3$4/in_voltage2_raw /bsp/environment/$2_swb_3_3v_aux
    ln -sf $3$4/in_voltage1_raw /bsp/environment/$2_swb_3_3v_sen
  fi
  if [ "$2" == "ADC" ]; then
    mkdir -p /bsp/environment/
    ln -sf $3$4/adc0_value /bsp/environment/$2_12v
    ln -sf $3$4/adc2_value /bsp/environment/$2_5v
    ln -sf $3$4/adc3_value /bsp/environment/$2_5v_usb
    ln -sf $3$4/adc4_value /bsp/environment/$2_3_3v_aux
    ln -sf $3$4/adc10_value /bsp/environment/$2_3_3v_bmc
    ln -sf $3$4/adc11_value /bsp/environment/$2_2_5v_ddr
    ln -sf $3$4/adc12_value /bsp/environment/$2_1_2v_ddr
    ln -sf $3$4/adc13_value /bsp/environment/$2_1_15v_Vcore
  fi
  if [ "$2" == "UCD" ]; then
    mkdir -p /bsp/environment/
    ln -sf $3$4/in2_input /bsp/environment/$2_3_3v_sen
    ln -sf $3$4/in3_input /bsp/environment/$2_1_2v
    ln -sf $3$4/curr2_input /bsp/environment/$2_3_3v_sen_curr
    ln -sf $3$4/curr3_input /bsp/environment/$2_1_2v_curr
  fi
  if [ "$2" == "VcoreUCD" ]; then
    mkdir -p /bsp/environment/
    ln -sf $3$4/in2_input /bsp/environment/$2
    ln -sf $3$4/curr2_input /bsp/environment/$2_curr
  fi
  if [ "$2" == "asic" ]; then
    mkdir -p /bsp/thermal/
    ln -sf $3$4/temp1_input /bsp/thermal/$2_temp
    ln -sf $3$4/temp1_highest /bsp/thermal/$2_temp_highest
  fi
  if [ "$2" == "reset" ]; then
    mkdir -p /bsp/reset/
    ln -sf $3$4/bmc_reset_soft /bsp/reset/bmc_reset_soft
    ln -sf $3$4/cpu_reset_hard /bsp/reset/cpu_reset_hard
    ln -sf $3$4/cpu_reset_soft /bsp/reset/cpu_reset_soft
    ln -sf $3$4/system_reset_hard /bsp/reset/system_reset_hard
    ln -sf $3$4/bmc_uart_en /bsp/reset/bmc_uart_en
    ln -sf $3$4/uart_sel /bsp/reset/uart_sel
    ln -sf $3$4/bmc_upgrade /bsp/reset/bmc_upgrade

    ln -sf $3$4/ac_power_cycle /bsp/reset/ac_power_cycle
    ln -sf $3$4/dc_power_cycle /bsp/reset/dc_power_cycle
    ln -sf $3$4/bmc_upgrade /bsp/reset/bmc_upgrade
    ln -sf $3$4/cpu_kernel_panic /bsp/reset/cpu_kernel_panic
    ln -sf $3$4/cpu_power_down /bsp/reset/cpu_power_down
    ln -sf $3$4/cpu_reboot /bsp/reset/cpu_reboot
    ln -sf $3$4/cpu_shutdown /bsp/reset/cpu_shutdown
    ln -sf $3$4/cpu_watchdog /bsp/reset/cpu_watchdog
  fi
  if [ "$2" == "phy_reset" ]; then
    mkdir -p /bsp/reset/
    ln -sf $3$4/phy_reset /bsp/reset/reset_phy
  fi
  if [ "$2" == "fan" ]; then
    mkdir -p /bsp/fan/
    ln -sf $3$4/tacho0_en /bsp/fan/tacho1_en
    ln -sf $3$4/tacho1_en /bsp/fan/tacho2_en
    ln -sf $3$4/tacho2_en /bsp/fan/tacho3_en
    ln -sf $3$4/tacho3_en /bsp/fan/tacho4_en
    ln -sf $3$4/tacho4_en /bsp/fan/tacho5_en
    ln -sf $3$4/tacho5_en /bsp/fan/tacho6_en
    ln -sf $3$4/tacho6_en /bsp/fan/tacho7_en
    ln -sf $3$4/tacho7_en /bsp/fan/tacho8_en

    ln -sf $3$4/tacho0_rpm /bsp/fan/tacho1_rpm
    ln -sf $3$4/tacho1_rpm /bsp/fan/tacho2_rpm
    ln -sf $3$4/tacho2_rpm /bsp/fan/tacho3_rpm
    ln -sf $3$4/tacho3_rpm /bsp/fan/tacho4_rpm
    ln -sf $3$4/tacho4_rpm /bsp/fan/tacho5_rpm
    ln -sf $3$4/tacho5_rpm /bsp/fan/tacho6_rpm
    ln -sf $3$4/tacho6_rpm /bsp/fan/tacho7_rpm
    ln -sf $3$4/tacho7_rpm /bsp/fan/tacho8_rpm

    ln -sf $3$4/pwm0_en /bsp/fan/pwm_en
    ln -sf $3$4/pwm0_falling /bsp/fan/pwm

    for i in /bsp/fan/tacho[1-8]_en; do echo 1 > $i; done
    echo 1 > /bsp/fan/pwm_en
    echo 6 > /bsp/fan/pwm
  fi
  if [ "$2" == "fan_4_10" ]; then
    mkdir -p /bsp/fan/

    ln -sf $3$4/fan1_input /bsp/fan/tacho1_rpm
    ln -sf $3$4/fan2_input /bsp/fan/tacho2_rpm
    ln -sf $3$4/fan3_input /bsp/fan/tacho3_rpm
    ln -sf $3$4/fan4_input /bsp/fan/tacho4_rpm
    ln -sf $3$4/fan5_input /bsp/fan/tacho5_rpm
    ln -sf $3$4/fan6_input /bsp/fan/tacho6_rpm
    ln -sf $3$4/fan7_input /bsp/fan/tacho7_rpm
    ln -sf $3$4/fan8_input /bsp/fan/tacho8_rpm

    ln -sf $3$4/pwm1 /bsp/fan/pwm

    echo 153 > /bsp/fan/pwm
  fi
  if [ "$2" == "eeprom_psu1" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/psu1_eeprom
    status_led.py 0xb5 1 0x2d
  fi
  if [ "$2" == "eeprom_psu2" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/psu2_eeprom
    status_led.py 0xb6 1 0x2d
  fi
  if [ "$2" == "eeprom_fan1" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/fan1_eeprom
    echo 0 > /bsp/leds/fan/red/1/brightness
    echo 1 > /bsp/leds/fan/green/1/brightness
    status_led.py 0xb7 1 0x2e
  fi
  if [ "$2" == "eeprom_fan2" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/fan2_eeprom
    echo 0 > /bsp/leds/fan/red/2/brightness
    echo 1 > /bsp/leds/fan/green/2/brightness
    status_led.py 0xb8 1 0x2e
  fi
  if [ "$2" == "eeprom_fan3" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/fan3_eeprom
    echo 0 > /bsp/leds/fan/red/3/brightness
    echo 1 > /bsp/leds/fan/green/3/brightness
    status_led.py 0xb9 1 0x2e
  fi
  if [ "$2" == "eeprom_fan4" ]; then
    mkdir -p /bsp/fru/
    ln -sf $3$4/eeprom /bsp/fru/fan4_eeprom
    echo 0 > /bsp/leds/fan/red/4/brightness
    echo 1 > /bsp/leds/fan/green/4/brightness
    status_led.py 0xba 1 0x2e
  fi
  if [ "$2" == "fan1_green" ]; then
    mkdir -p /bsp/leds/fan/green/1/
    ln -sf $3$4/brightness /bsp/leds/fan/green/1/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/green/1/trigger
    echo timer > /bsp/leds/fan/green/1/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/green/1/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/green/1/delay_off
  fi
  if [ "$2" == "fan1_red" ]; then
    mkdir -p /bsp/leds/fan/red/1/
    ln -sf $3$4/brightness /bsp/leds/fan/red/1/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/red/1/trigger
    echo 1 > /bsp/leds/fan/red/1/brightness
    echo timer > /bsp/leds/fan/red/1/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/red/1/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/red/1/delay_off
  fi
  if [ "$2" == "fan2_green" ]; then
    mkdir -p /bsp/leds/fan/green/2/
    ln -sf $3$4/brightness /bsp/leds/fan/green/2/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/green/2/trigger
    echo timer > /bsp/leds/fan/green/2/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/green/2/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/green/2/delay_off
  fi
  if [ "$2" == "fan2_red" ]; then
    mkdir -p /bsp/leds/fan/red/2/
    ln -sf $3$4/brightness /bsp/leds/fan/red/2/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/red/2/trigger
    echo 1 > /bsp/leds/fan/red/2/brightness
    echo timer > /bsp/leds/fan/red/2/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/red/2/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/red/2/delay_off
  fi
  if [ "$2" == "fan3_green" ]; then
    mkdir -p /bsp/leds/fan/green/3/
    ln -sf $3$4/brightness /bsp/leds/fan/green/3/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/green/3/trigger
    echo timer > /bsp/leds/fan/green/3/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/green/3/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/green/3/delay_off
  fi
  if [ "$2" == "fan3_red" ]; then
    mkdir -p /bsp/leds/fan/red/3/
    ln -sf $3$4/brightness /bsp/leds/fan/red/3/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/red/3/trigger
    echo 1 > /bsp/leds/fan/red/3/brightness
    echo timer > /bsp/leds/fan/red/3/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/red/3/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/red/3/delay_off
  fi
  if [ "$2" == "fan4_green" ]; then
    mkdir -p /bsp/leds/fan/green/4/
    ln -sf $3$4/brightness /bsp/leds/fan/green/4/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/green/4/trigger
    echo timer > /bsp/leds/fan/green/4/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/green/4/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/green/4/delay_off
  fi
  if [ "$2" == "fan4_red" ]; then
    mkdir -p /bsp/leds/fan/red/4/
    ln -sf $3$4/brightness /bsp/leds/fan/red/4/brightness
    ln -sf $3$4/trigger /bsp/leds/fan/red/4/trigger
    echo 1 > /bsp/leds/fan/red/4/brightness
    echo timer > /bsp/leds/fan/red/4/trigger
    ln -sf $3$4/delay_on  /bsp/leds/fan/red/4/delay_on
    ln -sf $3$4/delay_off /bsp/leds/fan/red/4/delay_off
  fi
  if [ "$2" == "status_green" ]; then
    mkdir -p /bsp/leds/status/green/
    ln -sf $3$4/brightness /bsp/leds/status/green/brightness
    ln -sf $3$4/trigger /bsp/leds/status/green/trigger
    echo timer > /bsp/leds/status/green/trigger
    ln -sf $3$4/delay_on  /bsp/leds/status/green/delay_on
    ln -sf $3$4/delay_off /bsp/leds/status/green/delay_off
  fi
  if [ "$2" == "status_red" ]; then
    mkdir -p /bsp/leds/status/red/
    ln -sf $3$4/brightness /bsp/leds/status/red/brightness
    ln -sf $3$4/trigger /bsp/leds/status/red/trigger
    echo timer > /bsp/leds/status/red/trigger
    ln -sf $3$4/delay_on  /bsp/leds/status/red/delay_on
    ln -sf $3$4/delay_off /bsp/leds/status/red/delay_off
  fi
  if [ "$2" == "status_amber" ]; then
    mkdir -p /bsp/leds/status/amber/
    ln -sf $3$4/brightness /bsp/leds/status/amber/brightness
    ln -sf $3$4/trigger /bsp/leds/status/amber/trigger
    echo timer > /bsp/leds/status/amber/trigger
    ln -sf $3$4/delay_on  /bsp/leds/status/amber/delay_on
    ln -sf $3$4/delay_off /bsp/leds/status/amber/delay_off
  fi
  if [ "$2" == "thermal_zone0" ]; then
    mkdir -p /bsp/thermal/
    mkdir -p /bsp/thermal/$2/
    ln -sf $3$4/mode /bsp/thermal/$2/mode
  fi
elif [ "$1" == "change" ]; then
  if [ "$2" == "reset" ]; then
    unlink /bsp/reset/bmc_reset_soft
    unlink /bsp/reset/cpu_reset_hard
    unlink /bsp/reset/cpu_reset_soft
    unlink /bsp/reset/system_reset_hard
    unlink /bsp/reset/bmc_uart_en
    unlink /bsp/reset/uart_sel
    unlink /bsp/reset/bmc_upgrade
    unlink /bsp/reset/ac_power_cycle
    unlink /bsp/reset/dc_power_cycle
    unlink /bsp/reset/bmc_upgrade
    unlink /bsp/reset/cpu_kernel_panic
    unlink /bsp/reset/cpu_power_down
    unlink /bsp/reset/cpu_reboot
    unlink /bsp/reset/cpu_shutdown
    unlink /bsp/reset/cpu_watchdog

    ln -sf $3$4/bmc_reset_soft /bsp/reset/bmc_reset_soft
    ln -sf $3$4/cpu_reset_hard /bsp/reset/cpu_reset_hard
    ln -sf $3$4/cpu_reset_soft /bsp/reset/cpu_reset_soft
    ln -sf $3$4/system_reset_hard /bsp/reset/system_reset_hard
    ln -sf $3$4/bmc_uart_en /bsp/reset/bmc_uart_en
    ln -sf $3$4/uart_sel /bsp/reset/uart_sel
    ln -sf $3$4/bmc_upgrade /bsp/reset/bmc_upgrade
    ln -sf $3$4/ac_power_cycle /bsp/reset/ac_power_cycle
    ln -sf $3$4/dc_power_cycle /bsp/reset/dc_power_cycle
    ln -sf $3$4/bmc_upgrade /bsp/reset/bmc_upgrade
    ln -sf $3$4/cpu_kernel_panic /bsp/reset/cpu_kernel_panic
    ln -sf $3$4/cpu_power_down /bsp/reset/cpu_power_down
    ln -sf $3$4/cpu_reboot /bsp/reset/cpu_reboot
    ln -sf $3$4/cpu_shutdown /bsp/reset/cpu_shutdown
    ln -sf $3$4/cpu_watchdog /bsp/reset/cpu_watchdog
  fi
  if [ "$2" == "phy_reset" ]; then
    unlink /bsp/reset/reset_phy
    ln -sf $3$4/phy_reset /bsp/reset/reset_phy
  fi
else
  if [ "$2" == "amb_current" ] || [ "$2" == "amb_switch" ]; then
    unlink /bsp/thermal/$2_temp
    unlink /bsp/thermal/$2_temp_max
    unlink /bsp/thermal/$2_temp_hyst
  fi
  if [ "$2" == "psu1" ] || [ "$2" == "psu2" ]; then
    unlink /bsp/thermal/$2_temp
    unlink /bsp/thermal/$2_temp_max
    unlink /bsp/thermal/$2_temp_alarm
    unlink /bsp/environment/$2_vin
    unlink /bsp/environment/$2_vout
    unlink /bsp/environment/$2_pin
    unlink /bsp/environment/$2_pout
    unlink /bsp/environment/$2_iin
    unlink /bsp/environment/$2_iout
    unlink /bsp/fan/$2_fan_input
  fi
  if [ "$2" == "A2D" ]; then
    unlink /bsp/environment/$2_voltage_scale
    unlink /bsp/environment/$2_1_8v
    unlink /bsp/environment/$2_1_2v
    unlink /bsp/environment/$2_vcore
    unlink /bsp/environment/$2_swb_12v
    unlink /bsp/environment/$2_swb_3_3v_aux
    unlink /bsp/environment/$2_swb_3_3v_sen
  fi
  if [ "$2" == "ADC" ]; then
    unlink /bsp/environment/$2_12v
    unlink /bsp/environment/$2_5v
    unlink /bsp/environment/$2_5v_usb
    unlink /bsp/environment/$2_3_3v_aux
    unlink /bsp/environment/$2_3_3v_bmc
    unlink /bsp/environment/$2_2_5v_ddr
    unlink /bsp/environment/$2_1_2v_ddr
    unlink /bsp/environment/$2_1_15v_Vcore
  fi
  if [ "$2" == "UCD" ]; then
    unlink /bsp/environment/$2_3_3v_sen
    unlink /bsp/environment/$2_1_2v
    unlink /bsp/environment/$2_3_3v_sen_curr
    unlink /bsp/environment/$2_1_2v_curr
  fi
  if [ "$2" == "VcoreUCD" ]; then
    unlink /bsp/environment/$2
    unlink /bsp/environment/$2_curr
  fi
  if [ "$2" == "asic" ]; then
    unlink /bsp/thermal/$2_temp
    unlink /bsp/thermal/$2_temp_highest
  fi
  if [ "$2" == "reset" ]; then
    unlink /bsp/reset/bmc_reset_soft
    unlink /bsp/reset/cpu_reset_hard
    unlink /bsp/reset/cpu_reset_soft
    unlink /bsp/reset/system_reset_hard
    unlink /bsp/reset/reset_phy
    unlink /bsp/reset/bmc_uart_en
    unlink /bsp/reset/uart_sel
    unlink /bsp/reset/ac_power_cycle
    unlink /bsp/reset/dc_power_cycle
    unlink /bsp/reset/bmc_upgrade
    unlink /bsp/reset/cpu_kernel_panic
    unlink /bsp/reset/cpu_power_down
    unlink /bsp/reset/cpu_reboot
    unlink /bsp/reset/cpu_shutdown
    unlink /bsp/reset/cpu_watchdog
  fi
  if [ "$2" == "phy_reset" ]; then
    unlink /bsp/reset/reset_phy
  fi
  if [ "$2" == "fan" ]; then
    unlink /bsp/fan/tacho1_en
    unlink /bsp/fan/tacho2_en
    unlink /bsp/fan/tacho3_en
    unlink /bsp/fan/tacho4_en
    unlink /bsp/fan/tacho5_en
    unlink /bsp/fan/tacho6_en
    unlink /bsp/fan/tacho7_en
    unlink /bsp/fan/tacho8_en

    unlink /bsp/fan/tacho1_rpm
    unlink /bsp/fan/tacho2_rpm
    unlink /bsp/fan/tacho3_rpm
    unlink /bsp/fan/tacho4_rpm
    unlink /bsp/fan/tacho5_rpm
    unlink /bsp/fan/tacho6_rpm
    unlink /bsp/fan/tacho7_rpm
    unlink /bsp/fan/tacho8_rpm
    unlink /bsp/fan/pwm_en
    unlink /bsp/fan/pwm
  fi
  if [ "$2" == "fan_4_10" ]; then
    unlink /bsp/fan/tacho1_rpm
    unlink /bsp/fan/tacho2_rpm
    unlink /bsp/fan/tacho3_rpm
    unlink /bsp/fan/tacho4_rpm
    unlink /bsp/fan/tacho5_rpm
    unlink /bsp/fan/tacho6_rpm
    unlink /bsp/fan/tacho7_rpm
    unlink /bsp/fan/tacho8_rpm
    unlink /bsp/fan/pwm
  fi
  if [ "$2" == "eeprom_psu1" ]; then
    unlink /bsp/fru/psu1_eeprom
    status_led.py 0xb5 0 0x2d
  fi
  if [ "$2" == "eeprom_psu2" ]; then
    unlink /bsp/fru/psu2_eeprom
    status_led.py 0xb6 0 0x2d
  fi
  if [ "$2" == "eeprom_fan1" ]; then
    unlink /bsp/fru/fan1_eeprom
    echo 0 > /bsp/leds/fan/green/1/brightness
    echo 1 > /bsp/leds/fan/red/1/brightness
    status_led.py 0xb7 0 0x2e
  fi
  if [ "$2" == "eeprom_fan2" ]; then
    unlink /bsp/fru/fan2_eeprom
    echo 0 > /bsp/leds/fan/green/2/brightness
    echo 1 > /bsp/leds/fan/red/2/brightness
    status_led.py 0xb8 0 0x2e
  fi
  if [ "$2" == "eeprom_fan3" ]; then
    unlink /bsp/fru/fan3_eeprom
    echo 0 > /bsp/leds/fan/green/3/brightness
    echo 1 > /bsp/leds/fan/red/3/brightness
    status_led.py 0xb9 0 0x2e
  fi
  if [ "$2" == "eeprom_fan4" ]; then
    unlink /bsp/fru/fan4_eeprom
    echo 0 > /bsp/leds/fan/green/4/brightness
    echo 1 > /bsp/leds/fan/red/4/brightness
    status_led.py 0xba 0 0x2e
  fi
  if [ "$2" == "fan1_green" ]; then
    unlink /bsp/leds/fan/green/1/brightness
    unlink /bsp/leds/fan/green/1/trigger
    unlink /bsp/leds/fan/green/1/delay_on
    unlink /bsp/leds/fan/green/1/delay_off
  fi
  if [ "$2" == "fan1_red" ]; then
    unlink /bsp/leds/fan/red/1/brightness
    unlink /bsp/leds/fan/red/1/trigger
    unlink /bsp/leds/fan/red/1/delay_on
    unlink /bsp/leds/fan/red/1/delay_off
  fi
  if [ "$2" == "fan2_green" ]; then
    unlink /bsp/leds/fan/green/2/brightness
    unlink /bsp/leds/fan/green/2/trigger
    unlink /bsp/leds/fan/green/2/delay_on
    unlink /bsp/leds/fan/green/2/delay_off
  fi
  if [ "$2" == "fan2_red" ]; then
    unlink /bsp/leds/fan/red/2/brightness
    unlink /bsp/leds/fan/red/2/trigger
    unlink /bsp/leds/fan/red/2/delay_on
    unlink /bsp/leds/fan/red/2/delay_off
  fi
  if [ "$2" == "fan3_green" ]; then
    unlink /bsp/leds/fan/green/3/brightness
    unlink /bsp/leds/fan/green/3/trigger
    unlink /bsp/leds/fan/green/3/delay_on
    unlink /bsp/leds/fan/green/3/delay_off
  fi
  if [ "$2" == "fan3_red" ]; then
    unlink /bsp/leds/fan/red/3/brightness
    unlink /bsp/leds/fan/red/3/trigger
    unlink /bsp/leds/fan/red/3/delay_on
    unlink /bsp/leds/fan/red/3/delay_off
  fi
  if [ "$2" == "fan4_green" ]; then
    unlink /bsp/leds/fan/green/4/brightness
    unlink /bsp/leds/fan/green/4/trigger
    unlink /bsp/leds/fan/green/4/delay_on
    unlink /bsp/leds/fan/green/4/delay_off
  fi
  if [ "$2" == "fan4_red" ]; then
    unlink /bsp/leds/fan/red/4/brightness
    unlink /bsp/leds/fan/red/4/trigger
    unlink /bsp/leds/fan/red/4/delay_on
    unlink /bsp/leds/fan/red/4/delay_off
  fi
  if [ "$2" == "status_green" ]; then
    unlink /bsp/leds/status/green/brightness
    unlink /bsp/leds/status/green/trigger
    unlink /bsp/leds/status/green/delay_on
    unlink /bsp/leds/status/green/delay_off
  fi
  if [ "$2" == "status_red" ]; then
    unlink /bsp/leds/status/red/brightness
    unlink /bsp/leds/status/red/trigger
    unlink /bsp/leds/status/red/delay_on
    unlink /bsp/leds/status/red/delay_off
  fi
  if [ "$2" == "status_amber" ]; then
    unlink /bsp/leds/status/amber/brightness
    unlink /bsp/leds/status/amber/trigger
    unlink /bsp/leds/status/amber/delay_on
    unlink /bsp/leds/status/amber/delay_off
  fi
  if [ "$2" == "thermal_zone0" ]; then
    unlink /bsp/thermal/$2/mode
  fi
fi
