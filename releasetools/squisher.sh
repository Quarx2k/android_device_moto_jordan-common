# This script is included in squisher
# It is the final build step (after OTA package)

# Delete unwanted apps
rm -f $REPACK/ota/system/app/RomManager.apk
# rm -f $REPACK/ota/system/xbin/irssi

# these scripts are not required
rm -f $REPACK/ota/system/etc/init.d/03firstboot
rm -f $REPACK/ota/system/etc/init.d/04modules

# add an empty script to prevent logcat errors (moto init.rc)
touch $REPACK/ota/system/bin/mount_ext3.sh
chmod +x $REPACK/ota/system/bin/mount_ext3.sh

mkdir -p $REPACK/ota/system/etc/terminfo/x
cp $REPACK/ota/system/etc/terminfo/l/linux $REPACK/ota/system/etc/terminfo/x/xterm

# prebuilt boot, devtree, logo & updater-script
rm -f $REPACK/ota/boot.img
cp -f $ANDROID_BUILD_TOP/device/motorola/jordan/updater-script $REPACK/ota/META-INF/com/google/android/updater-script
if [ -n "$CYANOGEN_RELEASE" ]; then
  cat $ANDROID_BUILD_TOP/device/motorola/jordan/updater-script-rel >> $REPACK/ota/META-INF/com/google/android/updater-script
  cp -f $ANDROID_BUILD_TOP/vendor/motorola/jordan/boot-222-179-4.smg $REPACK/ota/boot.img
  cp -f $ANDROID_BUILD_TOP/vendor/motorola/jordan/devtree-222-179-2.smg $REPACK/ota/devtree.img
  cp -f $ANDROID_BUILD_TOP/device/motorola/jordan/logo-google.raw $REPACK/ota/logo.img
fi
cp -f $ANDROID_BUILD_TOP/out/target/product/jordan/root/init $REPACK/ota/system/bootmenu/2nd-init/init
cp -f $REPACK/ota/system/bootmenu/binary/2nd-boot $REPACK/ota/system/bootmenu/binary/2nd-init
cp -f $ANDROID_BUILD_TOP/out/target/product/jordan/root/init.rc $REPACK/ota/system/bootmenu/2nd-init/init.rc
cp -f $ANDROID_BUILD_TOP/out/target/product/jordan/root/sbin/adbd $REPACK/ota/system/bin/adbd

# use the static busybox in bootmenu, this helps a lot on incomplete roms and ics
cp -f $ANDROID_BUILD_TOP/out/target/product/jordan/utilities/busybox $REPACK/ota/system/bootmenu/binary/busybox

# ics: to fix later
cp -f $REPACK/ota/system/lib/hw/hwcomposer.default.so $REPACK/ota/system/lib/hw/hwcomposer.jordan.so
cp -f $REPACK/ota/system/lib/hw/gralloc.default.so $REPACK/ota/system/lib/hw/gralloc.omap3.so

echo "0 0 android" > $REPACK/ota/system/lib/egl/egl.cfg
# echo "0 1 POWERVR_SGX530_125" >> $REPACK/ota/system/lib/egl/egl.cfg

