# This script is included in releasetools/addons
# It is the final build step (after OTA package)

# DEVICE_OUT=$ANDROID_BUILD_TOP/out/target/product/jordan
# DEVICE_TOP=$ANDROID_BUILD_TOP/device/motorola/jordan
# VENDOR_TOP=$ANDROID_BUILD_TOP/vendor/motorola/jordan

echo "addons.sh: $1"

# remove dummy kernel module (created by kernel gb repo)
rm -f $REPACK/ota/system/lib/modules/dummy.ko

if [ -z "$1" ]; then
	echo "addons.sh: error ! no target specified"
	exit 1;
fi

if [ "$1" = "kernel" ]; then
	cat $DEVICE_TOP/releasetools/updater-addons-kernel > $REPACK/ota/META-INF/com/google/android/updater-script
	cp -f $VENDOR_TOP/boot-222-179-4.smg $REPACK/ota/boot.img
	cp -f $VENDOR_TOP/devtree-222-179-2.smg $REPACK/ota/devtree.img
	OUTFILE=$OUT/kernel-froyo-222-179-4-signed.zip
fi

if [ "$1" = "bootmenu" ]; then

	cd $REPACK/ota
	unzip -q $OTAPACKAGE "system/bootmenu/*" "system/bin/*"

	cat $DEVICE_TOP/releasetools/updater-addons-bootmenu > $REPACK/ota/META-INF/com/google/android/updater-script

	#atrix bootmenu require a custom kernel to allow the framebuffer
	cp $DEVICE_OUT/boot.img $REPACK/ota/
	cat $DEVICE_TOP/releasetools/updater-addons-kernel >> $REPACK/ota/META-INF/com/google/android/updater-script

	mv $REPACK/ota/system/bin/logwrapper $REPACK/ota/system/bootmenu/binary/
	rm -r -f $REPACK/ota/system/bin
	mkdir -p $REPACK/ota/system/bin
	cp $DEVICE_OUT/system/bin/bootmenu $REPACK/ota/system/bin/

	mkdir -p $REPACK/ota/system/bootmenu/2nd-init
	cp $DEVICE_OUT/root/init $REPACK/ota/system/bootmenu/2nd-init/
	cp $DEVICE_OUT/root/*.rc $REPACK/ota/system/bootmenu/2nd-init/

	mkdir -p $REPACK/ota/system/bootmenu/2nd-boot
	cp $REPACK/ota/system/bootmenu/binary/2nd-init $REPACK/ota/system/bootmenu/binary/2nd-boot
	cp $DEVICE_OUT/root/init $REPACK/ota/system/bootmenu/2nd-boot/
	cp $DEVICE_OUT/root/*.rc $REPACK/ota/system/bootmenu/2nd-boot/

	OUTFILE=$OUT/bootmenu.zip
fi

if [ "$1" = "twrp-recovery" ]; then
	cat $DEVICE_TOP/releasetools/updater-addons-twrp-recovery > $REPACK/ota/META-INF/com/google/android/updater-script
	rm -rf $REPACK/ota/system
	mkdir -p $REPACK/ota/system/bootmenu/script
	cp -p $DEVICE_TOP/twrp/recovery_stable.sh $REPACK/ota/system/bootmenu/script/recovery_stable.sh
	cp -r $DEVICE_TOP/twrp/recovery $REPACK/ota/system/bootmenu/
	OUTFILE=$OUT/openrecovery-twrp-2.1.0-jordan-signed.zip
fi

