#! /system/bin/sh

echo " ******************************"
echo " * Starting Wi-Fi calibration *"
echo " ******************************"

HW_MAC=`calibrator get nvs_mac /pds/wifi/nvs_map.bin | grep -o -E "([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}"`
TARGET_FW_DIR=/system/etc/firmware/ti-connectivity
TARGET_NVS_FILE=$TARGET_FW_DIR/wl1271-nvs.bin
TARGET_INI_FILE=/system/etc/wifi/TQS_D_1.7_127x.ini

# Remount system partition as rw
mount -o remount,rw /system

# Actual calibration...
# calibrator plt autocalibrate <dev> <module path> <ini file1> <nvs file> <mac addr>
# Leaving mac address field empty for random mac
#calibrator plt autocalibrate wlan0 $WL12xx_MODULE $TARGET_INI_FILE $TARGET_NVS_FILE $HW_MAC 

if [ -e /pds/wifi/nvs_map.bin ];
then
	calibrator set nvs_mac $TARGET_NVS_FILE $HW_MAC 
else
	calibrator set nvs_mac $TARGET_NVS_FILE  00:12:34:56:78:90   #Used when nvs_map.bin not exists.
fi

# Remount system partition as ro
mount -o remount,ro /system

echo " ******************************"
echo " * Finished Wi-Fi calibration *"
echo " ******************************"
