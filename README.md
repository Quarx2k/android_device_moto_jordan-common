Jelly Bean for Motorola Defy (Android 4.1.2)

Download:
=========

repo init -u git://github.com/Quarx2k/android.git -b jellybean

repo sync

Build:
======
cd vendor/cm && ./get-prebuilts

copy /device/moto/jordan-common/apply_linaro.sh to root of tree and run it.

For CM10 branch :
  source build/envsetup.sh && lunch cm_mb526-userdebug && make -jX bacon

Use the signed zip to update the defy with the bootmenu recovery, not the ota package !

Links:
======

XDA JB Thread : http://forum.xda-developers.com/showthread.php?t=1768702
