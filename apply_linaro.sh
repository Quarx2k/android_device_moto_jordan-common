echo Applying linaro patches
echo
cd external/dnsmasq
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_dnsmasq refs/changes/41/26541/1 && git cherry-pick FETCH_HEAD
echo 
cd  ../chromium
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_chromium refs/changes/40/26540/1 && git cherry-pick FETCH_HEAD
echo 
cd ../icu4c
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_icu4c refs/changes/43/26543/1 && git cherry-pick FETCH_HEAD
echo 
cd ../e2fsprogs
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_e2fsprogs refs/changes/42/26542/1 && git cherry-pick FETCH_HEAD
echo
cd ../ping
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_ping refs/changes/48/26548/1 && git cherry-pick FETCH_HEAD
echo
cd ../openssl
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_openssl refs/changes/46/26546/1 && git cherry-pick FETCH_HEAD
echo
cd ../openssh
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_openssh refs/changes/45/26545/1 && git cherry-pick FETCH_HEAD
echo
cd ../stlport
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_stlport refs/changes/50/26550/1 && git cherry-pick FETCH_HEAD
echo
cd ../skia
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_skia refs/changes/49/26549/1 && git cherry-pick FETCH_HEAD
echo
cd ../webkit
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_webkit refs/changes/52/26552/1 && git cherry-pick FETCH_HEAD
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_webkit refs/changes/53/26553/1 && git cherry-pick FETCH_HEAD
echo
cd ../elfutils
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_elfutils refs/changes/67/26567/1 && git cherry-pick FETCH_HEAD
echo
cd ../ping6
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_ping6 refs/changes/68/26568/1 && git cherry-pick FETCH_HEAD
echo
cd ../v8
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_v8 refs/changes/51/26551/1 && git cherry-pick FETCH_HEAD
echo
cd ../openvpn
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_openvpn refs/changes/47/26547/1 && git cherry-pick FETCH_HEAD
echo
cd ../busybox
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_busybox refs/changes/39/26539/1 && git cherry-pick FETCH_HEAD
echo
cd ../tcpdump
git fetch http://review.cyanogenmod.org/CyanogenMod/android_external_tcpdump refs/changes/72/26572/1 && git cherry-pick FETCH_HEAD
echo
cd ../../frameworks/rs
git fetch http://review.cyanogenmod.org/CyanogenMod/android_frameworks_rs refs/changes/58/26558/1 && git cherry-pick FETCH_HEAD
echo
cd ../ex
git fetch http://review.cyanogenmod.org/CyanogenMod/android_frameworks_ex refs/changes/57/26557/1 && git cherry-pick FETCH_HEAD
echo
cd ../wilhelm
git fetch http://review.cyanogenmod.org/CyanogenMod/android_frameworks_wilhelm refs/changes/59/26559/1 && git cherry-pick FETCH_HEAD
echo
cd ../../libcore
git fetch http://review.cyanogenmod.org/CyanogenMod/android_libcore refs/changes/62/26562/1 && git cherry-pick FETCH_HEAD
echo
cd ../bionic
git fetch http://review.cyanogenmod.org/CyanogenMod/android_bionic refs/changes/36/26536/1 && git cherry-pick FETCH_HEAD
echo
cd ../system/security
git fetch http://review.cyanogenmod.org/CyanogenMod/android_system_security refs/changes/64/26564/1 && git cherry-pick FETCH_HEAD
echo
cd ../../packages/inputmethods/LatinIME
git fetch http://review.cyanogenmod.org/CyanogenMod/android_packages_inputmethods_LatinIME refs/changes/63/26563/1 && git cherry-pick FETCH_HEAD
echo
echo Finished
