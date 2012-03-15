#!/sbin/busybox ash

######## BootMenu Script
######## Execute [2nd-init] Menu

source /system/bootmenu/script/_config.sh

######## Main Script

toolbox mount -o remount,rw rootfs /
rm -f /*.rc
rm -f /*.sh
rm -f /osh
rm -rf /preinstall
cp -f /system/bootmenu/2nd-init/* /
ln -s /init /sbin/ueventd
cp -f /system/bootmenu/binary/adbd /sbin/adbd
killall ueventd

ADBD_RUNNING=`ps | grep adbd | grep -v grep`
if [ -z "$ADB_RUNNING" ]; then
    rm -f /tmp/usbd_current_state
    #delete if is a symlink
    [ -L "/tmp" ] && rm -f /tmp
    mkdir -p /tmp
else
    # well, not beautiful but do the work
    # to keep current usbd state
    if [ -L "/tmp" ]; then
        mv /tmp/usbd_current_state / 2>/dev/null
        rm -f /tmp
        mkdir -p /tmp
        mv /usbd_current_state /tmp/ 2>/dev/null
    fi
fi

if [ -L /sdcard-ext ]; then
    rm /sdcard-ext
    mkdir -p /sd-ext
fi

## unmount devices
sync
umount /acct
umount /dev/cpuctl
umount /dev/pts
umount /mnt/asec
umount /mnt/obb
umount /cache
umount /data

######## Cleanup

rm -f /sbin/lsof

## busybox cleanup..
for cmd in $(/sbin/busybox --list); do
    [ -L "/sbin/$cmd" ] && [ "$cmd" != "sh" ] && rm "/sbin/$cmd"
done

## used for adbd shell (can be bash also)
#/system/xbin/ln -s /system/bootmenu/binary/busybox /sbin/sh
#rm -f /sbin/busybox

## reduce lcd backlight to save battery
echo 18 > /sys/class/leds/lcd-backlight/brightness

######## Let's go

/system/bootmenu/binary/2nd-init

