#!/sbin/sh

######## BootMenu Script
######## Execute [2nd-init] Menu

source /system/bootmenu/script/_config.sh

######## Main Script

toolbox mount -o remount,rw rootfs /
mkdir /2ndboot
cp -f /system/bootmenu/2nd-boot/* /2ndboot
chmod 755 /2ndboot/*

## unmount devices
sync
umount /acct
umount /dev/cpuctl
umount /dev/pts
umount /mnt/asec
umount /mnt/obb
umount /cache
umount /data

## reduce lcd backlight to save battery
echo 18 > /sys/class/leds/lcd-backlight/brightness

cd /2ndboot

echo inserting hbootmod.ko
insmod ./hbootmod.ko emu_uart=115200

echo making node 
mknod /dev/hbootctrl c `cat /proc/devices | grep hboot | awk '{print $1}' ` 0

echo starting hboot 
./hbootuser ./hboot.cfg
	
