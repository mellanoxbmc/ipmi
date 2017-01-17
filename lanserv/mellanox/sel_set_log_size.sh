#!/bin/bash
H_SIZE=$1

if [ $H_SIZE -gt 4095 ] || [ $H_SIZE -lt 64 ]; then
        echo "Invalid range. Value must be in range 64-4095\n"
else
        sed -i'' "s/^sel_enable \([0-9x]*\) [^ ]* \([0-9x]*\)/sel_enable \1 $H_SIZE \2/g" /etc/ipmi/mellanox.hw
fi
