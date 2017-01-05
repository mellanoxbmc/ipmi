#!/bin/bash
H_SIZE=$1

if [ $H_SIZE -gt 65536 ] || [ $H_SIZE -lt 1000 ]; then
        echo "Invalid renge. Value must be in range 1000-65535.\n"
else
        sed -i'' "s/^sel_enable \([0-9x]*\) [^ ]* \([0-9x]*\)/sel_enable \1 $H_SIZE \2/g" /etc/ipmi/mellanox.hw
fi
