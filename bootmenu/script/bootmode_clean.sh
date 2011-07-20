#!/sbin/sh

mount_ext3.sh cache /cache

mv /cache/recovery/bootmode.conf /cache/recovery/last_bootmode

# busybox mount -o remount,rw /system
# rm /system/bootmenu/config/bootmode.conf

