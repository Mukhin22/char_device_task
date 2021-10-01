#!/bin/sh
led_num=$1
bl_times=$2
bl_int=$3
dmesg -C
insmod char_led.ko
./load.sh

if [[ led_num -eq 1 ]]; then
        printf "%d %d %d" 2 "$bl_times" "$bl_int"  > /dev/LED_CTRL&
else
        printf "%d %d %d" 2 "$bl_times" "$bl_int"  > /dev/LED_CTRL1&
fi

dmesg -w

