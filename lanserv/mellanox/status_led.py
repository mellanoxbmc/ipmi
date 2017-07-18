#!/usr/bin/env python

'''
Sensor number (1-st param):
    ambient carrier temp  0x01
    ambient switch temp   0x02
    asic temp             0x05
    PSU1 vin              0x9A
    PSU2 vin              0x9C
    A2D 1.8v              0x9E
    UCD 3.3v_sen          0xAC
    UCD 1.2v              0xAD
    UCD Vcore             0xAE
    VcoreUCD_curr         0x22
    UCD_3_3v_sen_cur      0x23
    UCD_1_2v_curr         0x24
    fan1_1                0x70
    fan1_2                0x71
    fan2_1                0x73
    fan2_2                0x73
    fan3_1                0x74
    fan3_2                0x75
    fan4_1                0x76
    fan4_2                0x77
    ipmi_wd               0xb4
    PSU1 drw              0xb5
    PSU2 drw              0xb6
    FAN1 drw              0xb7
    FAN2 drw              0xb8
    FAN3 drw              0xb9
    FAN4 drw              0xba
    CPU ready             0xbb
    CPU reboot            0xbc

Sensor Types (3-rd param):
    Temperature           0x01
    Voltage               0x02
    Current               0x03
    Fan                   0x04
    Processor             0x07
    PSU drw               0x2d
    FAN drw               0x2e
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
        print "Not allowed id("+id+") for change status led."
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

#always set worst status
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
