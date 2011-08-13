#!/sbin/sh

######## BootMenu Script
######## Execute [USB mtd] Tool


export PATH=/sbin:/system/xbin:/system/bin

######## Main Script

echo 'msc_charge' > /dev/usb_device_mode
sleep 1

BOARD_UMS_LUNFILE=/sys/devices/platform/usb_mass_storage/lun0/file
PARTITION=/dev/block/mmcblk1

echo $PARTITION > $BOARD_UMS_LUNFILE


echo 'msc' > /dev/usb_device_mode

exit
