#!/bin/bash
#
# Set macro key $1 to $2
#
# Note that this script sets all attached Gojira keyboards, and could
# be refined some.

MACRO_NUMBER=$(printf '%02x' $1)
MACRO_VALUE=$2

SERIALS=$(
    for sysdevpath in $(find /sys/bus/usb/devices/usb*/ -name dev); do
        (
            syspath="${sysdevpath%/dev}"
            devname="$(udevadm info -q name -p $syspath)"
            [[ "$devname" != "ttyACM"* ]] && exit
            eval "$(udevadm info -q property --export -p $syspath)"
            [[ "$ID_SERIAL" != "dijkstra.xyz_Gojira_"* ]] && exit
            echo "/dev/$devname"
        )
    done
)
for serial in $SERIALS; do
    echo -e "\nM${MACRO_NUMBER}${MACRO_VALUE}\n" > ${serial}
    while read -t 0 -e output < $serial; do
        echo $output
    done
done
