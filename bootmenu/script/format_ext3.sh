#!/sbin/sh

######## BootMenu Script
######## Format to ext3

source /system/bootmenu/script/_config.sh

######## Main Script
#Unmount 
umount /data
umount /cache

#Scan on errors
/system/bin/e2fsck -y $PART_DATA
/system/bin/e2fsck -y $PART_CACHE

#Format to ext3
/system/bin/mke2fs -j $PART_DATA 
/system/bin/mke2fs -j $PART_CACHE

#Mount cache again
mount -t ext3 -o nosuid,nodev,noatime,nodiratime,barrier=1 $PART_CACHE /cache

exit
