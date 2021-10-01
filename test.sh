#!/bin/sh

dmesg -C
insmod char_led.ko
./load.sh
printf "%i %i %i" 2 10 500 > /dev/LED_CTRL&
dmesg -w
