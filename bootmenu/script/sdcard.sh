#!/sbin/sh

######## BootMenu Script
######## Execute [Mass Storage] Tool


export PATH=/sbin:/system/xbin:/system/bin

######## Main Script

echo 'msc_charge' > /dev/usb_device_mode
umount /sdcard
sleep 1

BOARD_UMS_LUNFILE=/sys/devices/platform/usb_mass_storage/lun0/file
PARTITION=/dev/block/mmcblk0p1

echo $PARTITION > $BOARD_UMS_LUNFILE

echo "msc" > /dev/usb_device_mode

exit
