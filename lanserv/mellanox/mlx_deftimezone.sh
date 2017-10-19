#!/bin/bash

ln -sf /usr/share/zoneinfo/Asia/Hong_Kong /etc/localtime
echo Asia/Hong_Kong > /etc/timezone 
hwclock --systz Asia/Hong_Kong
hwclock -w
timedatectl set-ntp 0
timedatectl set-local-rtc 1
