#!/system/bootmenu/binary/busybox ash

######## BootMenu Script
######## Execute Post BootMenu

source /system/bootmenu/script/_config.sh

export PATH=/system/xbin:/system/bin:/sbin

######## Main Script

# there is a problem, this script is executed if we 
# exit from recovery... (init.rc re-start)

echo 0 > /sys/class/leds/blue/brightness

## Run Init Script

######## Don't Delete.... ########################
mount -o remount,rw rootfs /
mount -o remount,rw $PART_SYSTEM /system
##################################################

# fast button warning (if this script is used in recovery)
if [ -e /sbin/recovery ]; then
    echo 1 > /sys/class/leds/button-backlight/brightness
    usleep 150000
    echo 0 > /sys/class/leds/button-backlight/brightness
    usleep 50000
    echo 1 > /sys/class/leds/button-backlight/brightness
    usleep 150000
    echo 0 > /sys/class/leds/button-backlight/brightness
    exit 1
fi

if [ -d /system/bootmenu/init.d ]; then
    chmod 755 /system/bootmenu/init.d/*
    run-parts /system/bootmenu/init.d
fi

# reset any root perms for hwa settings
chown -R system.nobody /data/local/hwui.deny
chmod +rw /data/local/hwui.deny/*

# adb shell
ln -s /system/xbin/busybox /sbin/sh

######## Don't Delete.... ########################
mount -o remount,ro rootfs /
mount -o remount,ro $PART_SYSTEM /system
##################################################

exit 0
