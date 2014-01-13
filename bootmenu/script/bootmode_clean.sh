#!/sbin/sh

PATH=$PATH:/sbin:/system/xbin:/system/bin

mount -t auto -o nosuid,nodev,noatime,nodiratime,barrier=1 /dev/block/mmcblk1p24 /cache
mv /cache/recovery/bootmode.conf /cache/recovery/last_bootmode

