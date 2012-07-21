Jelly Bean for Motorola Defy (Android 4.1.1 AOSP)


Download:
=========

repo init -u git://github.com/CyanogenDefy/android.git -b jellybean

repo sync


Download RomManager (DELETED BY OUR BUILD SYSTEM)
=================================================

mkdir -p vendor/cm/proprietary
cd vendor/cm && ./get-prebuilts

Build:
======

rm -rf out/target

For CM10 branch :
  source build/envsetup.sh && brunch mb525

or for AOSP :
  source build/envsetup.sh && lunch full_mb525-eng
  mka bacon

Use the signed zip to update the defy with the bootmenu recovery, not the ota package !

Links:
======

XDA ICS Thread : http://forum.xda-developers.com/showthread.php?t=1353003

XDA JB Thread : http://forum.xda-developers.com/showthread.php?t=1768702
