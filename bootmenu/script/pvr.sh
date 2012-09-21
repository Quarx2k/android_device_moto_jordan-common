#!/system/xbin/ash
#
# Load PVR Driver.

export PATH=/system/xbin:/system/bin:/sbin

MAJOR=252
HOOK=0

if [ -f /proc/socinfo ]; then
   MAJOR=246
   HOOK=1
fi

insmod /system/lib/modules/symsearch.ko 2>/dev/null # must be already loaded

# Execute the SGX unloader module
insmod /system/lib/modules/pvroff.ko major_number=$MAJOR hook_enable=$HOOK

echo 0 > /sys/devices/platform/omapdss/overlay0/enabled
echo 4194304 > /sys/devices/platform/omapfb/graphics/fb0/size
echo 1 > /sys/devices/platform/omapdss/overlay0/enabled

# Load the new SGX modules
insmod /system/lib/modules/pvrsrvkm.ko
insmod /system/lib/modules/omaplfb.ko

# Now free the unloader
log -t PVR Unload pvroff module...
rmmod /system/lib/modules/pvroff.ko

