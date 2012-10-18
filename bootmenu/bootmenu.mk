# Required tools and blobs for bootmenu
bm_device = device/moto/jordan-common

PRODUCT_PACKAGES += \
	bootmenu \
	utility_lsof \
	static_busybox \
	static_logwrapper \
	2nd-init \
	utility_mke2fs \
	utility_tune2fs \
	e2fsck recovery_pigz \

# config
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/config/bootmenu_bypass:system/bootmenu/config/bootmenu_bypass \
	${bm_device}/bootmenu/config/default.prop:system/bootmenu/config/default.prop \
	${bm_device}/bootmenu/config/default_bootmode.conf:system/bootmenu/config/default_bootmode.conf \
	${bm_device}/bootmenu/config/overclock.conf:system/bootmenu/config/overclock.conf \
	${bm_device}/bootmenu/script/_config.sh:system/bootmenu/script/_config.sh \

# 2nd-boot profiles
PRODUCT_COPY_FILES += \
	${bm_device}/profiles/2nd-boot/2ndboot.ko:system/bootmenu/2nd-boot/2ndboot.ko \
	${bm_device}/profiles/2nd-boot/hbootuser:system/bootmenu/2nd-boot/hbootuser \
	${bm_device}/profiles/2nd-boot/hboot.cfg:system/bootmenu/2nd-boot/hboot.cfg \
	${bm_device}/profiles/2nd-boot/devtree:system/bootmenu/2nd-boot/devtree \

# Ramdisk
PRODUCT_COPY_FILES += \
	${bm_device}/profiles/ramdisk/ueventd.rc:root/ueventd.rc \
	${bm_device}/profiles/ramdisk/init.usb.rc:root/init.usb.rc \
	${bm_device}/profiles/ramdisk/init.mapphone_umts.rc:root/init.mapphone_umts.rc \
	${bm_device}/bootmenu/binary/adbd:root/sbin/adbd \
# scripts
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/script/2nd-boot.sh:system/bootmenu/script/2nd-boot.sh \
	${bm_device}/bootmenu/script/adbd.sh:system/bootmenu/script/adbd.sh \
	${bm_device}/bootmenu/script/bootmode_clean.sh:system/bootmenu/script/bootmode_clean.sh \
	${bm_device}/bootmenu/script/cdrom.sh:system/bootmenu/script/cdrom.sh \
	${bm_device}/bootmenu/script/data.sh:system/bootmenu/script/data.sh \
	${bm_device}/bootmenu/script/overclock.sh:system/bootmenu/script/overclock.sh \
	${bm_device}/bootmenu/script/post_bootmenu.sh:system/bootmenu/script/post_bootmenu.sh \
	${bm_device}/bootmenu/script/pre_bootmenu.sh:system/bootmenu/script/pre_bootmenu.sh \
	${bm_device}/bootmenu/script/reboot_command.sh:system/bootmenu/script/reboot_command.sh \
	${bm_device}/bootmenu/script/recovery.sh:system/bootmenu/script/recovery.sh \
	${bm_device}/bootmenu/script/recovery_stable.sh:system/bootmenu/script/recovery_stable.sh \
	${bm_device}/bootmenu/script/sdcard.sh:system/bootmenu/script/sdcard.sh \
	${bm_device}/bootmenu/script/system.sh:system/bootmenu/script/system.sh \
	${bm_device}/bootmenu/script/pdsbackup.sh:system/bootmenu/script/pdsbackup.sh \

# prebuilt binaries (to clean...)
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/binary/adbd:system/bootmenu/binary/adbd \
	${bm_device}/bootmenu/binary/logwrapper.bin:system/bootmenu/binary/logwrapper.bin \
	${bm_device}/bootmenu/binary/logwrapper.bin:system/bin/logwrapper.bin \

# images
PRODUCT_COPY_FILES += \
	external/bootmenu/images/indeterminate1.png:system/bootmenu/images/indeterminate.png \
	external/bootmenu/images/progress_empty.png:system/bootmenu/images/progress_empty.png \
	external/bootmenu/images/progress_fill.png:system/bootmenu/images/progress_fill.png \
	${bm_device}/bootmenu/images/background.png:system/bootmenu/images/background.png \

# recovery
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/recovery/recovery.fstab:system/etc/recovery.fstab \
	${bm_device}/bootmenu/recovery/res/keys:system/bootmenu/recovery/res/keys \
	${bm_device}/bootmenu/recovery/res/images/icon_error.png:system/bootmenu/recovery/res/images/icon_error.png \
	${bm_device}/bootmenu/recovery/res/images/icon_done.png:system/bootmenu/recovery/res/images/icon_done.png \
	${bm_device}/bootmenu/recovery/res/images/icon_installing.png:system/bootmenu/recovery/res/images/icon_installing.png \
	${bm_device}/bootmenu/recovery/res/images/icon_firmware_error.png:system/bootmenu/recovery/res/images/icon_firmware_error.png \
	${bm_device}/bootmenu/recovery/res/images/icon_firmware_install.png:system/bootmenu/recovery/res/images/icon_firmware_install.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate1.png:system/bootmenu/recovery/res/images/indeterminate1.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate2.png:system/bootmenu/recovery/res/images/indeterminate2.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate3.png:system/bootmenu/recovery/res/images/indeterminate3.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate4.png:system/bootmenu/recovery/res/images/indeterminate4.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate5.png:system/bootmenu/recovery/res/images/indeterminate5.png \
	${bm_device}/bootmenu/recovery/res/images/indeterminate6.png:system/bootmenu/recovery/res/images/indeterminate6.png \
	${bm_device}/bootmenu/recovery/res/images/progress_empty.png:system/bootmenu/recovery/res/images/progress_empty.png \
	${bm_device}/bootmenu/recovery/res/images/progress_fill.png:system/bootmenu/recovery/res/images/progress_fill.png \
	${bm_device}/bootmenu/recovery/res/images/icon_cm.png:system/bootmenu/recovery/res/images/icon_bootmenu.png \
	${bm_device}/bootmenu/recovery/res/images/icon_cm.png:system/bootmenu/recovery/res/images/icon_clockwork.png \
	${bm_device}/bootmenu/recovery/sbin/badblocks:system/bootmenu/recovery/sbin/badblocks \
	${bm_device}/bootmenu/recovery/sbin/dedupe:system/bootmenu/recovery/sbin/dedupe \
	${bm_device}/bootmenu/recovery/sbin/dump_image:system/bootmenu/recovery/sbin/dump_image \
	${bm_device}/bootmenu/recovery/sbin/fix_permissions:system/bootmenu/recovery/sbin/fix_permissions \
	${bm_device}/bootmenu/recovery/sbin/killrecovery.sh:system/bootmenu/recovery/sbin/killrecovery.sh \
	${bm_device}/bootmenu/recovery/sbin/nandroid-md5.sh:system/bootmenu/recovery/sbin/nandroid-md5.sh \
	${bm_device}/bootmenu/recovery/sbin/parted:system/bootmenu/recovery/sbin/parted \
	${bm_device}/bootmenu/recovery/sbin/postrecoveryboot.sh:system/bootmenu/recovery/sbin/postrecoveryboot.sh \
	${bm_device}/bootmenu/recovery/sbin/recovery:system/bootmenu/recovery/sbin/recovery_stable \
	${bm_device}/bootmenu/recovery/sbin/recoveryexit.sh:system/bootmenu/recovery/sbin/recoveryexit.sh \
	${bm_device}/bootmenu/recovery/sbin/resize2fs:system/bootmenu/recovery/sbin/resize2fs \
	${bm_device}/bootmenu/recovery/sbin/sdparted:system/bootmenu/recovery/sbin/sdparted \
	${bm_device}/bootmenu/recovery/sbin/mke2fs:system/bootmenu/recovery/sbin/mke2fs \
	${bm_device}/bootmenu/recovery/recovery.fstab:system/bootmenu/recovery/recovery.fstab \


