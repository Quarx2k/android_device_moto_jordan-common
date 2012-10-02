#!/system/bin/sh
#
# This script make a backup of pds partition to your data partition
# to allow us to use tuned battd versions without data loss risks
#
# Note: This pds partition contains unique informations related to
#       your device, like battery calibration, wifi and baseband
#

PATH=/system/xbin:$PATH
PDS_FILE=/data/pdsdata.img

mount_pds_image() {
    mkdir -p /pds
    umount /pds 2>/dev/null
    losetup -d /dev/block/loop7 2>/dev/null
    losetup /dev/block/loop7 $PDS_FILE
    busybox mount -o rw,nosuid,nodev,noatime,nodiratime,barrier=1 /dev/block/loop7 /pds
}

if [ -f /data/pds.img ]; then
    #delete old pds image that may have broken permissions
    rm -f /data/pds.img
fi

if [ ! -f $PDS_FILE ] ; then
    #make a copy of pds in /data
    dd if=/dev/block/mmcblk1p7 of=$PDS_FILE bs=4096

    #mount the fake pds
    mount_pds_image

    cd /pds
    #find and change moto users first
    busybox find -user 9000 -exec chown 1000 {} \;
    busybox find -user 9003 -exec chown 1000 {} \;
    busybox find -user 9004 -exec chown 1000 {} \;
    busybox find -user 9007 -exec chown 1000 {} \;

    #find and change moto groups
    busybox find -group 9000 -exec chgrp 1000 {} \;
    busybox find -group 9003 -exec chgrp 1000 {} \;
    busybox find -group 9004 -exec chgrp 1000 {} \;
    busybox find -group 9007 -exec chgrp 1000 {} \;
    busybox find -group 9009 -exec chgrp 1000 {} \;

    echo "PDS Backed up, permissions fixed and mounted"

    if [ -d /data/battd ] ; then
        cd /data/battd
        busybox find -user 9000 -exec chown 1000 {} \;
        busybox find -group 9000 -exec chgrp 1000 {} \;
    fi

else

    #mount the existing pds backup
    mount_pds_image

    if [ -d /pds/public ] ; then
        echo "PDS partition mounted from data image."
    fi
fi
