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

H_SIZE=$1

if [ $H_SIZE -gt 4095 ] || [ $H_SIZE -lt 64 ]; then
        echo "Invalid range. Value must be in range 64-4095\n"
else
        sed -i'' "s/^sel_enable \([0-9x]*\) [^ ]* \([0-9x]*\)/sel_enable \1 $H_SIZE \2/g" /etc/ipmi/mellanox.hw
fi
