#!/system/bin/sh

ifconfig tiap0 up
exec /system/bin/hostap -d /data/misc/wifi/hostapd.conf
