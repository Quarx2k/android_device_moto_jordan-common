#
# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# This is the product configuration for a generic Motorola Defy (jordan)
#

# The gps config appropriate for this device
$(call inherit-product, device/common/gps/gps_eu_supl.mk)

ifeq ($(TARGET_PRODUCT),$(filter $(TARGET_PRODUCT),cm_mb525 cm_mb526))
$(call inherit-product, vendor/motorola/jordan-common/jordan-vendor.mk)
endif

## (3)  Finally, the least specific parts, i.e. the non-GSM-specific aspects
PRODUCT_PROPERTY_OVERRIDES += \
	ro.media.capture.flip=horizontalandvertical \
	ro.com.google.locationfeatures=1 \
	ro.media.dec.jpeg.memcap=20000000 \
	net.dns1=8.8.8.8 \
	net.dns2=8.8.4.4 \
	ro.opengles.version = 131072 \
	persist.sys.usb.config=mass_storage,adb \
	ro.product.use_charge_counter=1 \
	hwui.use.blacklist=true \
	ro.sf.lcd_density=240 \

# wifi props
PRODUCT_PROPERTY_OVERRIDES += \
	wifi.interface=wlan0 \
	softap.interface=wlan0 \
	wifi.supplicant_scan_interval=60 \

# telephony props
PRODUCT_PROPERTY_OVERRIDES += \
	ro.telephony.ril.v3=skipbrokendatacall,signalstrength \
	ro.telephony.ril_class=MotoWrigley3GRIL \
	ro.telephony.call_ring.multiple=false \
	ro.telephony.call_ring.delay=3000 \
	ro.telephony.default_network=3 \
	mobiledata.interfaces=rmnet0 \

DEVICE_PACKAGE_OVERLAYS += device/moto/jordan-common/overlay

# Permissions
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:/system/etc/permissions/android.software.live_wallpaper.xml \

# FIXME in repo 
PRODUCT_PACKAGES += rild

# ICS sound
PRODUCT_PACKAGES += \
	hcitool hciattach hcidump \
	libaudioutils audio.a2dp.default  \
	libaudiohw_legacy \

# TO FIX for ICS
PRODUCT_PACKAGES += power.omap3

# OMX stuff
PRODUCT_PACKAGES += dspexec libbridge libLCML libOMX_Core libstagefrighthw
PRODUCT_PACKAGES += libOMX.TI.AAC.encode libOMX.TI.AAC.decode libOMX.TI.AMR.decode libOMX.TI.AMR.encode
PRODUCT_PACKAGES += libOMX.TI.WBAMR.encode libOMX.TI.MP3.decode libOMX.TI.WBAMR.decode
PRODUCT_PACKAGES += libOMX.TI.Video.Decoder libOMX.TI.Video.encoder
PRODUCT_PACKAGES += libOMX.TI.JPEG.Encoder #libskiahw libOMX.TI.JPEG.decoder

# Defy stuff
PRODUCT_PACKAGES += libfnc DefyParts Usb MotoFM MotoFMService

# Core stuff
PRODUCT_PACKAGES += charge_only_mode mot_boot_mode

# Publish that we support the live wallpaper feature.
PRODUCT_PACKAGES += librs_jni

# CM9 apps
PRODUCT_PACKAGES += Torch HwaSettings make_ext4fs

# Experimental TI OpenLink
PRODUCT_PACKAGES += libnl_2 iw libbt-vendor uim-sysfs

# Wifi
PRODUCT_PACKAGES += \
    lib_driver_cmd_wl12xx \
    dhcpcd.conf \
    hostapd.conf \
    wifical.sh \
    wpa_supplicant.conf \
    TQS_D_1.7.ini \
    TQS_D_1.7_127x.ini \
    crda \
    regulatory.bin \
    calibrator 

# Wifi Direct and WPAN
PRODUCT_PACKAGES += \
    ti_wfd_libs \
    ti-wpan-fw

PRODUCT_COPY_FILES += \
    $(OUT)/ramdisk.img:system/bootmenu/2nd-boot/ramdisk \
    $(OUT)/kernel:system/bootmenu/2nd-boot/zImage \
    $(OUT)/utilities/busybox:system/bootmenu/binary/busybox \
    $(OUT)/utilities/lsof:system/bootmenu/binary/lsof \

# Blobs and bootmenu stuff
$(call inherit-product, device/moto/jordan-common/jordan-blobs.mk)
$(call inherit-product, device/moto/jordan-common/bootmenu/bootmenu.mk)
$(call inherit-product, build/target/product/full_base.mk)
$(call inherit-product, frameworks/native/build/phone-hdpi-512-dalvik-heap.mk)

# Should be after the full_base include, which loads languages_full
PRODUCT_LOCALES += hdpi

PRODUCT_NAME := full_jordan
PRODUCT_DEVICE := MB52x

