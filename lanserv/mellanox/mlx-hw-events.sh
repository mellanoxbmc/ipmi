#!/bin/bash

if [ "$1" == "add" ]; then
  if [ "$2" == "amb_current" ] || [ "$2" == "amb_switch" ]; then
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
  fi
  if [ "$2" == "11-0041" ] || [ "$2" == "11-0027" ]; then
    mkdir -p /bsp/environment/
    ln -s $3$4/in2_input /bsp/environment/$2_vin
    ln -s $3$4/in3_input /bsp/environment/$2_vout
  fi
  if [ "$2" == "reset" ]]; then
    mkdir -p /bsp/reset/
    ln -s $3$4/bmc_reset_soft /bsp/reset/bmc_reset_soft
    ln -s $3$4/cpu_reset_hard /bsp/reset/cpu_reset_hard
    ln -s $3$4/cpu_reset_soft /bsp/reset/cpu_reset_soft
    ln -s $3$4/system_reset_hard /bsp/reset/system_reset_hard
    ln -s $3$4/system_reset_hard /bsp/reset/reset_phy
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
fi
