#! /system/bin/sh

WIFION=`getprop init.svc.wpa_supplicant`

case "$WIFION" in
  "running") echo " ********************************************************"
             echo " * Turn Wi-Fi OFF and launch the script for calibration *"
             echo " ********************************************************"
             exit;;
          *) echo " ******************************"
             echo " * Starting Wi-Fi calibration *"
             echo " ******************************";;
esac

HW_MAC=`calibrator get nvs_mac /pds/wifi/nvs_map.bin | grep -o -E "([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}"`
TARGET_FW_DIR=/system/etc/firmware/ti-connectivity
TARGET_NVS_FILE=$TARGET_FW_DIR/wl1271-nvs.bin
TARGET_INI_FILE=/system/etc/wifi/TQS_D_1.7_127x.ini
WL12xx_MODULE=/system/lib/modules/wl12xx_sdio.ko

if [ -e $WL12xx_MODULE ];
then
    echo ""
else
    echo "********************************************************"
    echo "wl12xx_sdio module not found !!"
    echo "********************************************************"
    exit
fi

# Remount system partition as rw
mount -o remount,rw /system

ln -s /system/xbin/busybox /system/bin/rmmod

# Remove old NVS file
rm /system/etc/firmware/ti-connectivity/wl1271-nvs.bin

# Actual calibration...
# calibrator plt autocalibrate <dev> <module path> <ini file1> <nvs file> <mac addr>
# Leaving mac address field empty for random mac
calibrator plt autocalibrate wlan0 $WL12xx_MODULE $TARGET_INI_FILE $TARGET_NVS_FILE $HW_MAC

# Disable Wifi Calibration after first boot
if [ -e $TARGET_NVS_FILE ];
then
	busybox mv -f /system/bin/wifical.sh /system/bin/wifical.sh_disabled
fi

# Remount system partition as ro
mount -o remount,ro /system


echo " ******************************"
echo " * Finished Wi-Fi calibration *"
echo " ******************************"
