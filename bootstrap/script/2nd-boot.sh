#!/sbin/sh

######## Execute [2nd-boot] Script

######## Main Script
BB_STATIC="/system/bootstrap/binary/busybox"

$BB_STATIC mount -o remount,rw rootfs /
$BB_STATIC mkdir /2ndboot
$BB_STATIC cp -f /system/bootstrap/2nd-boot/* /2ndboot
$BB_STATIC cp -f /system/bootstrap/binary/hbootuser /2ndboot/hbootuser
$BB_STATIC cp -f /system/bootstrap/modules/hbootmod.ko /2ndboot/hbootmod.ko
$BB_STATIC chmod 755 /2ndboot/*

## unmount devices
$BB_STATIC sync
umount /acct
umount /dev/cpuctl
umount /dev/pts
umount /mnt/asec
umount /mnt/obb
umount /cache
umount /data

## reduce lcd backlight to save battery
$BB_STATIC echo 18 > /sys/class/leds/lcd-backlight/brightness

cd /2ndboot

$BB_STATIC echo "inserting hbootmod.ko"
$BB_STATIC insmod ./hbootmod.ko kill_dss=1

$BB_STATIC echo "making node"
$BB_STATIC mknod /dev/hbootctrl c `$BB_STATIC cat /proc/devices | $BB_STATIC grep hboot | $BB_STATIC awk '{print $1}' ` 0

$BB_STATIC echo "starting hboot"
./hbootuser ./hboot.cfg
