#!/system/bin/sh
#
# Load PVR Driver.

export PATH=/sbin:/system/xbin:/system/bin

MAJOR=252
HOOK=0

if [ -f /proc/socinfo ]; then
MAJOR=246
HOOK=1
fi

  /system/xbin/insmod /system/lib/modules/symsearch.ko
  /system/xbin/insmod /system/lib/modules/pvroff.ko major_number=$MAJOR hook_enable=$HOOK
  echo 0 > /sys/devices/platform/omapdss/overlay0/enabled 
  echo 4194304 > /sys/devices/platform/omapfb/graphics/fb0/size
  echo 1 > /sys/devices/platform/omapdss/overlay0/enabled
  /system/xbin/insmod /system/lib/modules/pvrsrvkm.ko
  /system/xbin/insmod /system/lib/modules/omaplfb.ko


