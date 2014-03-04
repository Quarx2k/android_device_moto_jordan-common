# Required tools and blobs for bootstrap
bm_device = device/moto/jordan-common
include $(all-subdir-makefiles)

# Ramdisk
PRODUCT_COPY_FILES += \
	${bm_device}/ramdisk/ueventd.mapphone_umts.rc:root/ueventd.mapphone_umts.rc \
	${bm_device}/ramdisk/init.usb.rc:root/init.usb.rc \
	${bm_device}/ramdisk/init.mapphone_umts.rc:root/init.mapphone_umts.rc \
	${bm_device}/ramdisk/fstab.mapphone_umts:root/fstab.mapphone_umts \

# scripts
PRODUCT_COPY_FILES += \
	${bm_device}/bootstrap/script/2nd-boot.sh:system/bootstrap/script/2nd-boot.sh \
	${bm_device}/bootstrap/script/reboot_command.sh:system/bootstrap/script/reboot_command.sh \
	${bm_device}/bootstrap/script/recovery.sh:system/bootstrap/script/recovery.sh \
	${bm_device}/bootstrap/script/pdsbackup.sh:system/bootstrap/script/pdsbackup.sh \

# prebuilt binaries
PRODUCT_COPY_FILES += \
	${bm_device}/bootstrap/binary/logwrapper:system/bin/logwrapper \
	${bm_device}/bootstrap/binary/logwrapper.bin:system/bin/logwrapper.bin \
	${bm_device}/bootstrap/binary/hbootuser:system/bootstrap/binary/hbootuser \
	${bm_device}/bootstrap/binary/safestrapmenu:system/bootstrap/binary/safestrapmenu \
	${bm_device}/bootstrap/binary/busybox:system/bootstrap/binary/busybox \
	${bm_device}/bootstrap/modules/hbootmod.ko:system/bootstrap/modules/hbootmod.ko \
	${bm_device}/bootstrap/2nd-boot/hboot.cfg:system/bootstrap/2nd-boot/hboot.cfg \
	${bm_device}/bootstrap/2nd-boot/hboot_recovery.cfg:system/bootstrap/2nd-boot/hboot_recovery.cfg \
	${bm_device}/bootstrap/2nd-boot/zImage-recovery:system/bootstrap/2nd-boot/zImage-recovery \
	$(bm_device)/twrp.fstab:recovery/root/etc/twrp.fstab \
	${bm_device}/bootstrap/modules/jbd2.ko:system/bootstrap/modules/jbd2.ko \
	${bm_device}/bootstrap/modules/ext4.ko:system/bootstrap/modules/ext4.ko \
	${bm_device}/bootstrap/images/background-def.png:system/bootstrap/images/background-def.png \
	${bm_device}/bootstrap/images/background-blank.png:system/bootstrap/images/background-blank.png \
	$(OUT)/ramdisk-recovery.img:system/bootstrap/2nd-boot/ramdisk-recovery \
	$(OUT)/ramdisk.img:system/bootstrap/2nd-boot/ramdisk \
	$(OUT)/kernel:system/bootstrap/2nd-boot/zImage \

