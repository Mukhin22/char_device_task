#!/bin/sh

device="LED_CTRL"
major=200

rm -f /dev/${device} c $major 0

mknod /dev/${device} c $major 0

chmod 666 /dev/${device}
