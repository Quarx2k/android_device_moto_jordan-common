#!/system/bootmenu/binary/busybox ash

######## BootMenu Script
######## Execute Pre BootMenu

#We should insmod TLS Module before start bootmenu
/system/bootmenu/binary/busybox insmod /system/bootmenu/modules/symsearch.ko
/system/bootmenu/binary/busybox insmod /system/bootmenu/modules/tls-enable.ko
#Insmod ext4 modules 
/system/bootmenu/binary/busybox insmod /system/bootmenu/modules/jbd2.ko
/system/bootmenu/binary/busybox insmod /system/bootmenu/modules/ext4.ko
/system/bootmenu/binary/busybox insmod /system/bootmenu/modules/cpcap-battery.ko

source /system/bootmenu/script/_config.sh

######## Main Script


BB_STATIC="/system/bootmenu/binary/busybox"

BB="/sbin/busybox"

## reduce lcd backlight to save battery
echo 64 > /sys/class/leds/lcd-backlight/brightness


# these first commands are duplicated for broken systems
mount -o remount,rw rootfs /
$BB_STATIC mount -o remount,rw /

# we will use the static busybox
cp -f $BB_STATIC $BB
$BB_STATIC cp -f $BB_STATIC $BB

chmod 755 /sbin
chmod 755 $BB
$BB chown 0.0 $BB
$BB chmod 4755 $BB

if [ -f /sbin/chmod ]; then
    # job already done...
    exit 0
fi

# busybox sym link..
for cmd in $($BB --list); do
    $BB ln -s /sbin/busybox /sbin/$cmd
done

# add lsof to debug locks
cp -f /system/bootmenu/binary/lsof /sbin/lsof

$BB chmod +rx /sbin/*

# custom adbd (allow always root)
cp -f /system/bootmenu/binary/adbd /sbin/adbd
chown 0.0  /sbin/adbd
chmod 4750 /sbin/adbd

## missing system files
[ ! -c /dev/tty0 ]  && ln -s /dev/tty /dev/tty0

## mount cache
mkdir -p /cache

# mount cache for boot mode and recovery logs
if [ ! -d /cache/recovery ]; then
    mount -t auto -o nosuid,nodev,noatime,nodiratime,barrier=1 $PART_CACHE /cache
fi

mkdir -p /cache/bootmenu

# load ondemand safe settings to reduce heat and battery use
#if [ -x /system/bootmenu/script/overclock.sh ]; then
#    /system/bootmenu/script/overclock.sh safe
#fi

# must be restored in stock.sh
if [ -L /tmp ]; then
  mv /tmp /tmp.bak
  mkdir /tmp && busybox mount -t ramfs ramfs /tmp
fi

exit 0
