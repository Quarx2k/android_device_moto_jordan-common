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

DEVICE_PREBUILT := device/motorola/jordan/prebuilt

#temporary cm9 bootanimation
PRODUCT_COPY_FILES += \
	$(DEVICE_PREBUILT)/bootanimation.zip:system/media/bootanimation.zip 

PRODUCT_COPY_FILES += \
	$(DEVICE_PREBUILT)/etc/terminfo/l/linux:system/etc/terminfo/l/linux \
	$(DEVICE_PREBUILT)/etc/terminfo/x/xterm:system/etc/terminfo/x/xterm 

#key layouts, names must fit the ones in /proc/bus/input/devices, qwerty.kl is the fallback one.
PRODUCT_COPY_FILES += \
	$(DEVICE_PREBUILT)/usr/idc/internal.idc:system/usr/idc/lm3530_led.idc \
	$(DEVICE_PREBUILT)/usr/idc/internal.idc:system/usr/idc/accelerometer.idc \
	$(DEVICE_PREBUILT)/usr/idc/internal.idc:system/usr/idc/compass.idc \
	$(DEVICE_PREBUILT)/usr/idc/internal.idc:system/usr/idc/light-prox.idc \
	$(DEVICE_PREBUILT)/usr/idc/internal.idc:system/usr/idc/proximity.idc \
	$(DEVICE_PREBUILT)/usr/idc/sholes-keypad.idc:system/usr/idc/sholes-keypad.idc \
	$(DEVICE_PREBUILT)/usr/idc/cpcap-key.idc:system/usr/idc/cpcap-key.idc \
	$(DEVICE_PREBUILT)/usr/idc/qtouch-touchscreen.idc:system/usr/idc/qtouch-touchscreen.idc \
	$(DEVICE_PREBUILT)/usr/qwerty.kl:system/usr/keylayout/qwerty.kl \
	$(DEVICE_PREBUILT)/usr/qwerty.kl:system/usr/keylayout/qtouch-touchscreen.kl \
	$(DEVICE_PREBUILT)/usr/keypad.kl:system/usr/keylayout/sholes-keypad.kl \
	$(DEVICE_PREBUILT)/usr/keypad.kl:system/usr/keylayout/umts_jordan-keypad.kl \
	$(DEVICE_PREBUILT)/usr/cpcap-key.kl:system/usr/keylayout/cpcap-key.kl \
	$(DEVICE_PREBUILT)/usr/keychars/cpcap-key.kcm:system/usr/keychars/cpcap-key.kcm 


PRODUCT_COPY_FILES += \
	device/motorola/jordan/bootmenu/recovery/recovery.fstab:system/etc/recovery.fstab \
	$(DEVICE_PREBUILT)/etc/init.d/01sysctl:system/etc/init.d/01sysctl \
	$(DEVICE_PREBUILT)/etc/init.d/02ipv6:system/etc/init.d/02ipv6 \
	$(DEVICE_PREBUILT)/etc/init.d/03firstboot:system/etc/init.d/03firstboot \
	$(DEVICE_PREBUILT)/etc/init.d/04pdsbackup:system/etc/init.d/04pdsbackup \
	$(DEVICE_PREBUILT)/etc/init.d/05mountsd:system/etc/init.d/05mountsd \
	$(DEVICE_PREBUILT)/etc/init.d/07camera:system/etc/init.d/07camera \
	$(DEVICE_PREBUILT)/etc/init.d/08backlight:system/etc/init.d/08backlight \
	$(DEVICE_PREBUILT)/etc/init.d/90multitouch:system/etc/init.d/90multitouch \
	$(DEVICE_PREBUILT)/etc/profile:system/etc/profile \
	$(DEVICE_PREBUILT)/etc/sysctl.conf:system/etc/sysctl.conf \
	$(DEVICE_PREBUILT)/etc/busybox.fstab:system/etc/fstab \
	$(DEVICE_PREBUILT)/etc/wifi/dnsmasq.conf:system/etc/wifi/dnsmasq.conf \
	$(DEVICE_PREBUILT)/etc/wifi/tiwlan.ini:system/etc/wifi/tiwlan.ini \
	$(DEVICE_PREBUILT)/etc/wifi/tiwlan_ap.ini:system/etc/wifi/tiwlan_ap.ini \
	$(DEVICE_PREBUILT)/etc/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
	$(DEVICE_PREBUILT)/etc/gpsconfig.xml:system/etc/gpsconfig.xml \
	$(DEVICE_PREBUILT)/etc/location.cfg:system/etc/location.cfg \
	$(DEVICE_PREBUILT)/bin/hostap:system/bin/hostap \
	device/motorola/jordan/vold.fstab:system/etc/vold.fstab \
	device/motorola/jordan/media_profiles.xml:system/etc/media_profiles.xml \
	device/motorola/jordan/modules/modules.alias:system/lib/modules/modules.alias \
	device/motorola/jordan/modules/modules.dep:system/lib/modules/modules.dep 

ifdef CM_RELEASE
	PRODUCT_COPY_FILES += device/motorola/jordan/custom_backup_release.txt:system/etc/custom_backup_list.txt
else
	PRODUCT_COPY_FILES += device/motorola/jordan/custom_backup_list.txt:system/etc/custom_backup_list.txt
endif

#end of jordan-blobs.mk
