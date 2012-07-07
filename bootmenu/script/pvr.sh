#!/system/bin/sh
#
# Load PVR Driver.

PATH=/system/xbin:$PATH

MODULE="/system/lib/modules/pvroff.ko"

if [ -f /proc/socinfo ]; then
MODULE="/system/lib/modules/pvroff_plus.ko"
fi

  insmod /system/lib/modules/symsearch.ko
  insmod $MODULE
  echo 0 > /sys/devices/platform/omapdss/overlay0/enabled 
  echo 4194304 > /sys/devices/platform/omapfb/graphics/fb0/size
  echo 1 > /sys/devices/platform/omapdss/overlay0/enabled
  insmod /system/lib/modules/pvrsrvkm.ko
  insmod /system/lib/modules/omaplfb.ko


