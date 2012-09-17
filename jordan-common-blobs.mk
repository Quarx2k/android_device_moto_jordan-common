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

DEVICE_PATH = device/motorola/jordan-common
DEVICE_PREBUILT := ${DEVICE_PATH}/prebuilt

#key layouts, names must fit the ones in /proc/bus/input/devices, qwerty.kl is the fallback one.
PRODUCT_COPY_FILES += \
	${DEVICE_PREBUILT}/usr/qwerty.kl:system/usr/keylayout/qwerty.kl \
	${DEVICE_PREBUILT}/usr/qwerty.kl:system/usr/keylayout/qtouch-touchscreen.kl \
	${DEVICE_PREBUILT}/usr/keypad.kl:system/usr/keylayout/sholes-keypad.kl \
	${DEVICE_PREBUILT}/usr/keypad.kl:system/usr/keylayout/umts_jordan-keypad.kl \
	${DEVICE_PREBUILT}/usr/cpcap.kl:system/usr/keylayout/cpcap-key.kl \

#etc
PRODUCT_COPY_FILES += \
	${DEVICE_PATH}/vold.fstab:system/etc/vold.fstab \
	${DEVICE_PATH}/bootmenu/recovery/recovery.fstab:system/etc/recovery.fstab \
	${DEVICE_PREBUILT}/etc/init.d/00baseband:system/etc/init.d/00baseband \
	${DEVICE_PREBUILT}/etc/init.d/01sysctl:system/etc/init.d/01sysctl \
	${DEVICE_PREBUILT}/etc/init.d/05mountsd:system/etc/init.d/05mountsd \
	${DEVICE_PREBUILT}/etc/init.d/08backlight:system/etc/init.d/08backlight \
	${DEVICE_PREBUILT}/etc/init.d/80kineto:system/etc/init.d/80kineto \
	${DEVICE_PREBUILT}/etc/init.d/90multitouch:system/etc/init.d/90multitouch \
	${DEVICE_PREBUILT}/etc/profile:system/etc/profile \
	${DEVICE_PREBUILT}/etc/sysctl.conf:system/etc/sysctl.conf \
	${DEVICE_PREBUILT}/etc/busybox.fstab:system/etc/fstab \
	${DEVICE_PREBUILT}/etc/wifi/dnsmasq.conf:system/etc/wifi/dnsmasq.conf \
	${DEVICE_PREBUILT}/etc/wifi/tiwlan.ini:system/etc/wifi/tiwlan.ini \
	${DEVICE_PREBUILT}/etc/wifi/tiwlan_ap.ini:system/etc/wifi/tiwlan_ap.ini \
	${DEVICE_PREBUILT}/etc/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
	${DEVICE_PREBUILT}/etc/gpsconfig.xml:system/etc/gpsconfig.xml \
	${DEVICE_PREBUILT}/etc/location.cfg:system/etc/location.cfg \
	${DEVICE_PATH}/media_profiles.xml:system/etc/media_profiles.xml \
	${DEVICE_PATH}/modules/modules.alias:system/lib/modules/modules.alias \
	${DEVICE_PATH}/modules/modules.dep:system/lib/modules/modules.dep \

# copy all of our kernel modules under the "modules" directory to system/lib/modules
PRODUCT_COPY_FILES += $(shell test -d device/motorola/jordan-common/modules && \
	find device/motorola/jordan-common/modules -name '*.ko' \
	-printf '%p:system/lib/modules/%f ')

#end of jordan-blobs.mk
