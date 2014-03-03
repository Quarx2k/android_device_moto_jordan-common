# Required tools and blobs for bootmenu
bm_device = device/moto/jordan-common

PRODUCT_PACKAGES += \
	bootmenu \
	utility_lsof \
	static_busybox \
	static_logwrapper \
	hbootuser \
	safestrapmenu \
	utility_mke2fs \
	utility_tune2fs \
	e2fsck \

# config
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/config/bootmenu_bypass:system/bootmenu/config/bootmenu_bypass \
	${bm_device}/bootmenu/config/default_bootmode.conf:system/bootmenu/config/default_bootmode.conf \
	${bm_device}/bootmenu/config/overclock.conf:system/bootmenu/config/overclock.conf \
	${bm_device}/bootmenu/script/_config.sh:system/bootmenu/script/_config.sh \

# 2nd-boot profiles
PRODUCT_COPY_FILES += \
	${bm_device}/profiles/2nd-boot/hbootmod.ko:system/bootmenu/2nd-boot/hbootmod.ko \
	${bm_device}/profiles/2nd-boot/hboot.cfg:system/bootmenu/2nd-boot/hboot.cfg \

# Ramdisk
PRODUCT_COPY_FILES += \
	${bm_device}/profiles/ramdisk/ueventd.mapphone_umts.rc:root/ueventd.mapphone_umts.rc \
	${bm_device}/profiles/ramdisk/init.usb.rc:root/init.usb.rc \
	${bm_device}/profiles/ramdisk/init.mapphone_umts.rc:root/init.mapphone_umts.rc \
	${bm_device}/profiles/ramdisk/fstab.mapphone_umts:root/fstab.mapphone_umts \

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
	${bm_device}/bootmenu/script/sdcard.sh:system/bootmenu/script/sdcard.sh \
	${bm_device}/bootmenu/script/system.sh:system/bootmenu/script/system.sh \
	${bm_device}/bootmenu/script/pdsbackup.sh:system/bootmenu/script/pdsbackup.sh \
	${bm_device}/bootmenu/script/format_ext3.sh:system/bootmenu/script/format_ext3.sh \
	${bm_device}/bootmenu/script/format_ext4.sh:system/bootmenu/script/format_ext4.sh \

# prebuilt modules for stock kernel
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/modules/tls-enable.ko:system/bootmenu/modules/tls-enable.ko \
	${bm_device}/bootmenu/modules/symsearch.ko:system/bootmenu/modules/symsearch.ko \
	${bm_device}/bootmenu/modules/jbd2.ko:system/bootmenu/modules/jbd2.ko \
	${bm_device}/bootmenu/modules/ext4.ko:system/bootmenu/modules/ext4.ko \
	${bm_device}/bootmenu/modules/cpcap-battery.ko:system/bootmenu/modules/cpcap-battery.ko \

# prebuilt binaries (to clean...)
PRODUCT_COPY_FILES += \
	${bm_device}/bootmenu/binary/adbd:system/bootmenu/binary/adbd \
	${bm_device}/bootmenu/binary/logwrapper.bin:system/bootmenu/binary/logwrapper.bin \
	${bm_device}/bootmenu/binary/logwrapper:system/bin/logwrapper \
	${bm_device}/bootmenu/binary/logwrapper.bin:system/bin/logwrapper.bin \

# images
PRODUCT_COPY_FILES += \
	external/bootmenu/images/indeterminate1.png:system/bootmenu/images/indeterminate.png \
	external/bootmenu/images/progress_empty.png:system/bootmenu/images/progress_empty.png \
	external/bootmenu/images/progress_fill.png:system/bootmenu/images/progress_fill.png \
	${bm_device}/bootmenu/images/background.png:system/bootmenu/images/background.png \
	${bm_device}/bootmenu/images/background-def.png:system/bootmenu/images/background-def.png \
	${bm_device}/bootmenu/images/background-blank.png:system/bootmenu/images/background-blank.png \
# recovery
PRODUCT_COPY_FILES += \
	${bm_device}/profiles/2nd-boot/hboot_recovery.cfg:system/bootmenu/2nd-boot/hboot_recovery.cfg \
	$(bm_device)/temp/busybox:system/bootmenu/binary/busybox \
	$(bm_device)/twrp.fstab:recovery/root/etc/twrp.fstab \
	$(OUT)/ramdisk-recovery.img:system/bootmenu/2nd-boot/ramdisk-recovery \
	${bm_device}/profiles/2nd-boot/zImage-recovery:system/bootmenu/2nd-boot/zImage-recovery \

