#!/bin/bash

if [ "$1" == "add" ]; then
  if [ "$2" == "amb_carrier" ] || [ "$2" == "amb_switch" ]; then
    mkdir -p /bsp/thermal/
    ln -s $3$4/temp1_input /bsp/thermal/$2_temp
    ln -s $3$4/temp1_max /bsp/thermal/$2_temp_max
    ln -s $3$4/temp1_max_hyst /bsp/thermal/$2_temp_hyst
  fi
  if [ "$2" == "psu1" ] || [ "$2" == "psu2" ]; then
    mkdir -p /bsp/thermal/
    mkdir -p /bsp/environment/
    ln -s $3$4/temp1_input /bsp/thermal/$2_temp
    ln -s $3$4/temp1_max /bsp/thermal/$2_temp_max
    ln -s $3$4/temp1_max_alarm /bsp/thermal/$2_temp_alarm
    ln -s $3$4/in2_input /bsp/environment/$2_vin
    ln -s $3$4/in3_input /bsp/environment/$2_vout
    ln -s $3$4/power1_input /bsp/environment/$2_pin
    ln -s $3$4/power2_input /bsp/environment/$2_pout
    ln -s $3$4/current1_input /bsp/environment/$2_iin
    ln -s $3$4/current2_input /bsp/environment/$2_iout
    ln -s $3$4/eeprom /bsp/environment/$2_eeprom
    #PSU FAN spped set
  fi
  if [ "$2" == "11-0041" ] || [ "$2" == "11-0027" ]; then
    mkdir -p /bsp/environment/
    ln -s $3$4/in2_input /bsp/environment/$2_vin
    ln -s $3$4/in3_input /bsp/environment/$2_vout
  fi
  if [ "$2" == "reset" ]; then
    mkdir -p /bsp/reset/
    ln -s $3$4/bmc_reset_soft /bsp/reset/bmc_reset_soft
    ln -s $3$4/cpu_reset_hard /bsp/reset/cpu_reset_hard
    ln -s $3$4/cpu_reset_soft /bsp/reset/cpu_reset_soft
    ln -s $3$4/system_reset_hard /bsp/reset/system_reset_hard
    ln -s $3$4/system_reset_hard /bsp/reset/reset_phy
  fi
  if [ "$2" == "fan" ]]; then
    mkdir -p /bsp/fan/
    ln -s $3$4/tacho1_en /bsp/fan/tacho1_en
    ln -s $3$4/tacho2_en /bsp/fan/tacho2_en
    ln -s $3$4/tacho3_en /bsp/fan/tacho3_en
    ln -s $3$4/tacho4_en /bsp/fan/tacho4_en
    ln -s $3$4/tacho5_en /bsp/fan/tacho5_en
    ln -s $3$4/tacho6_en /bsp/fan/tacho6_en
    ln -s $3$4/tacho7_en /bsp/fan/tacho7_en
    ln -s $3$4/tacho8_en /bsp/fan/tacho8_en

    ln -s $3$4/tacho1_rpm /bsp/fan/tacho1_rpm
    ln -s $3$4/tacho2_rpm /bsp/fan/tacho2_rpm
    ln -s $3$4/tacho3_rpm /bsp/fan/tacho3_rpm
    ln -s $3$4/tacho4_rpm /bsp/fan/tacho4_rpm
    ln -s $3$4/tacho5_rpm /bsp/fan/tacho5_rpm
    ln -s $3$4/tacho6_rpm /bsp/fan/tacho6_rpm
    ln -s $3$4/tacho7_rpm /bsp/fan/tacho7_rpm
    ln -s $3$4/tacho8_rpm /bsp/fan/tacho8_rpm

    ln -s $3$4/pwm0_en /bsp/fan/pwm_en
    ln -s $3$4/pwm0_falling /bsp/fan/pwm

    echo 1 > /bsp/fan/tacho1_en
    echo 1 > /bsp/fan/tacho2_en
    echo 1 > /bsp/fan/tacho3_en
    echo 1 > /bsp/fan/tacho4_en
    echo 1 > /bsp/fan/tacho5_en
    echo 1 > /bsp/fan/tacho6_en
    echo 1 > /bsp/fan/tacho7_en
    echo 1 > /bsp/fan/tacho8_en
  fi
  if [ "$2" == "eeprom_fan1" ]]; then
    mkdir -p /bsp/fan/
    ln -s $3$4/eeprom /bsp/fan/fan1_eeprom
  fi
  if [ "$2" == "eeprom_fan2" ]]; then
    mkdir -p /bsp/fan/
    ln -s $3$4/eeprom /bsp/fan/fan2_eeprom
  fi
  if [ "$2" == "eeprom_fan3" ]]; then
    mkdir -p /bsp/fan/
    ln -s $3$4/eeprom /bsp/fan/fan3_eeprom
  fi
  if [ "$2" == "eeprom_fan4" ]]; then
    mkdir -p /bsp/fan/
    ln -s $3$4/eeprom /bsp/fan/fan4_eeprom
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
    unlink /bsp/environment/$2_eeprom
  fi
  if [ "$2" == "11-0041" ] || [ "$2" == "11-0027" ]; then
    unlink /bsp/environment/$2_vin
    unlink /bsp/environment/$2_vout
  fi
  if [ "$2" == "reset" ]]; then
    unlink /bsp/reset/bmc_reset_soft
    unlink /bsp/reset/cpu_reset_hard
    unlink /bsp/reset/cpu_reset_soft
    unlink /bsp/reset/system_reset_hard
    unlink /bsp/reset/reset_phy
  fi
    if [ "$2" == "fan" ]]; then
    echo 0 > /bsp/fan/tacho1_en
    echo 0 > /bsp/fan/tacho2_en
    echo 0 > /bsp/fan/tacho3_en
    echo 0 > /bsp/fan/tacho4_en
    echo 0 > /bsp/fan/tacho5_en
    echo 0 > /bsp/fan/tacho6_en
    echo 0 > /bsp/fan/tacho7_en
    echo 0 > /bsp/fan/tacho8_en

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
  fi
  if [ "$2" == "eeprom_fan1" ]]; then
    unlink /bsp/fan/fan1_eeprom
  fi
  if [ "$2" == "eeprom_fan2" ]]; then
    unlink /bsp/fan/fan2_eeprom
  fi
  if [ "$2" == "eeprom_fan3" ]]; then
    unlink /bsp/fan/fan3_eeprom
  fi
  if [ "$2" == "eeprom_fan4" ]]; then
    unlink /bsp/fan/fan4_eeprom
  fi
fi
