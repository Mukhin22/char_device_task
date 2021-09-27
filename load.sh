#!/bin/sh

device="LED_CTRL"
major=200

rm -f /dev/${device} c $major 0
mknod /dev/${device} c $major 0

rm -f /dev/${device}1 c $major 1
mknod /dev/${device}1 c $major 1

chmod 666 /dev/${device}
chmod 666 /dev/${device}1