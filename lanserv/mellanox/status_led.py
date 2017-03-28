#!/usr/bin/env python

'''
[c_mykolak@r-vnc15 ~]$ ipmitool -I lanplus -H 10.209.51.230 -U ADMIN -P ADMIN sdr elist
ambient carrier temp | 01h | ok  | 42.1 | 24 degrees C
ambient switch temp | 02h | ucr | 42.1 | 24 degrees C
PSU1 temp        | 03h | ok  | 42.1 | 0 degrees C
PSU2 temp        | 04h | ok  | 42.1 | 0 degrees C
asic temp        | 05h | ok  | 42.1 | 0 degrees C
PSU1 vin         | 9Ah | ok  | 10.1 | 0 Volts
PSU1 vout        | 9Bh | ok  | 10.1 | 0 Volts
PSU2 vin         | 9Ch | ok  | 10.1 | 0 Volts
PSU2 vout        | 9Dh | ok  | 10.1 | 0 Volts
A2D 1.8v         | 9Eh | ok  | 10.1 | 1.82 Volts
A2D 1.2v         | 9Fh | ok  | 10.1 | 1.20 Volts
A2D Vcore        | A0h | ok  | 10.1 | 0.95 Volts
SWB 12v          | A1h | ok  | 10.1 | 11.98 Volts
SWB 3.3v_aux     | A2h | ok  | 10.1 | 3.37 Volts
SWB 3.3v_sen     | A3h | ucr | 10.1 | 3.84 Volts
ADC 12v          | A4h | ok  | 10.1 | 11.93 Volts
ADC 5v           | A5h | ok  | 10.1 | 5.08 Volts
ADC 5v usb       | A6h | ok  | 10.1 | 5.05 Volts
ADC 3.3v_aux     | A7h | ok  | 10.1 | 3.34 Volts
ADC 3.3v_bmc     | A8h | ok  | 10.1 | 3.33 Volts
ADC 2.5v_ddr     | A9h | ok  | 10.1 | 2.50 Volts
ADC 1.2v_ddr     | AAh | ok  | 10.1 | 1.19 Volts
ADC 1.15v_Vcore  | ABh | ok  | 10.1 | 1.20 Volts
UCD 3.3v_sen     | ACh | ok  | 10.1 | 3.29 Volts
UCD 1.2v         | ADh | ok  | 10.1 | 1.21 Volts
UCD Vcore        | AEh | ok  | 10.1 | 0.96 Volts
PSU1_pin         | 14h | ok  | 10.1 | 0 Watts
PSU1_pout        | 15h | ok  | 10.1 | 0 Watts
PSU2_pin         | 16h | ok  | 10.1 | 0 Watts
PSU2_pout        | 17h | ok  | 10.1 | 0 Watts
PSU1_iin         | 1Eh | ok  | 10.1 | 0 Amps
PSU1_iout        | 1Fh | ok  | 10.1 | 0 Amps
PSU2_iin         | 20h | ok  | 10.1 | 0 Amps
PSU2_iout        | 21h | ok  | 10.1 | 0 Amps
fan1_1           | 70h | ok  | 29.1 | 0 RPM
fan1_2           | 71h | ok  | 29.1 | 0 RPM
fan2_1           | 73h | ok  | 29.1 | 0 RPM
fan2_2           | 73h | ok  | 29.1 | 0 RPM
fan3_1           | 74h | ok  | 29.1 | 0 RPM
fan3_2           | 75h | ok  | 29.1 | 0 RPM
fan4_1           | 76h | ok  | 29.1 | 0 RPM
fan4_2           | 77h | ok  | 29.1 | 0 RPM
PSU1_fan         | 78h | ok  | 29.1 | 0 RPM
PSU2_fan         | 79h | ok  | 29.1 | 0 RPM

Sensor Types:
	Temperature               (0x01)   Voltage                   (0x02)
	Current                   (0x03)   Fan                       (0x04)
	Physical Security         (0x05)   Platform Security         (0x06)
	Processor                 (0x07)   Power Supply              (0x08)
	Power Unit                (0x09)   Cooling Device            (0x0a)
	Other                     (0x0b)   Memory                    (0x0c)
	Drive Slot / Bay          (0x0d)   POST Memory Resize        (0x0e)
	System Firmwares          (0x0f)   Event Logging Disabled    (0x10)
	Watchdog1                 (0x11)   System Event              (0x12)
	Critical Interrupt        (0x13)   Button                    (0x14)
	Module / Board            (0x15)   Microcontroller           (0x16)
	Add-in Card               (0x17)   Chassis                   (0x18)
	Chip Set                  (0x19)   Other FRU                 (0x1a)
	Cable / Interconnect      (0x1b)   Terminator                (0x1c)
	System Boot Initiated     (0x1d)   Boot Error                (0x1e)
	OS Boot                   (0x1f)   OS Critical Stop          (0x20)
	Slot / Connector          (0x21)   System ACPI Power State   (0x22)
	Watchdog2                 (0x23)   Platform Alert            (0x24)
	Entity Presence           (0x25)   Monitor ASIC              (0x26)
	LAN                       (0x27)   Management Subsys Health  (0x28)
	Battery                   (0x29)   Session Audit             (0x2a)
	Version Change            (0x2b)   FRU State                 (0x2c)

'''
import sys
import os

import json

config = {}
entrys = {}

#read config with allowed ids and something else
def read_config():
    global config
    with open('/etc/status_led.json') as data_file:
        config = json.load(data_file)

    return 1

#here we will check id and exit if this id dont allowed to set status led
def filter_id(id):
    global config

    if id in config["ids"]:
        return 1
    else:
        print "Not allowed id for change status led."
        return 0

# 1 - event id
# 2 - event status ( 0 - Asserted, 1 - Deassered)
def get_args():
    global entrys

    if sys.argv[2] in ['0','1']:
        if filter_id(sys.argv[1]):
            if int(sys.argv[2]):
                entrys[sys.argv[1]]= [0, sys.argv[3]]
            else:
                entrys[sys.argv[1]]= [config["ids"][sys.argv[1]], sys.argv[3]]
            return

    else:
        print "2-nd argument only 0,1 allowed."
    exit()

def main():
    global entrys
    read_config()
    s = open('/tmp/led_status', 'a+').read()
    if s:
        entrys = eval(s)

    get_args()
    open('/tmp/led_status', 'w').write(str(entrys))
    set_status_led()

#allways set worst status
def set_status_led():
    status = 0 #green by default
    global status_led_sysfs
    global config
    type_counter = {}

    for value in entrys.values():
        status = status if (status > value[0]) else value[0]

        if value[1] in config["type"]:
            if int(value[0]) > 0:
                if type_counter.get(value[1]):
                    type_counter[value[1]] += int(1)
                else:
                    type_counter[value[1]] = 1
    if type_counter:
        for key, value in type_counter.iteritems():
            max_type_id = key
            max_type_cnt = value
            cfg_type_status = config["type"].get(max_type_id).get("status")
            cfg_type_max_cnt = config["type"].get(max_type_id).get("max_count")
            if int(max_type_cnt) > int(cfg_type_max_cnt):
                status = status if (status > cfg_type_status) else cfg_type_status

    os.system(config["status"][int(status)].get("cmd"))


# execute main
main()
