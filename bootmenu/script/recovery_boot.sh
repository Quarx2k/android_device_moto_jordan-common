#!/sbin/sh

busybox mount -o remount,rw /system

echo "bootmenu" > /system/bootmenu/config/default_bootmode.conf

