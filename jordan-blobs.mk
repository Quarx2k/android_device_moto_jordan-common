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

device_path = device/moto/jordan-common

PRODUCT_COPY_FILES += \
	$(device_path)/etc/terminfo/l/linux:system/etc/terminfo/l/linux \
	$(device_path)/etc/terminfo/x/xterm:system/etc/terminfo/x/xterm \

# Key layouts, names must fit the ones in /proc/bus/input/devices, qwerty.kl is the fallback one.
PRODUCT_COPY_FILES += \
	$(device_path)/usr/idc/internal.idc:system/usr/idc/lm3530_led.idc \
	$(device_path)/usr/idc/internal.idc:system/usr/idc/accelerometer.idc \
	$(device_path)/usr/idc/internal.idc:system/usr/idc/compass.idc \
	$(device_path)/usr/idc/internal.idc:system/usr/idc/light-prox.idc \
	$(device_path)/usr/idc/internal.idc:system/usr/idc/proximity.idc \
	$(device_path)/usr/idc/sholes-keypad.idc:system/usr/idc/sholes-keypad.idc \
	$(device_path)/usr/idc/cpcap-key.idc:system/usr/idc/cpcap-key.idc \
	$(device_path)/usr/idc/qtouch-touchscreen.idc:system/usr/idc/qtouch-touchscreen.idc \
	$(device_path)/usr/qwerty.kl:system/usr/keylayout/qwerty.kl \
	$(device_path)/usr/qwerty.kl:system/usr/keylayout/qtouch-touchscreen.kl \
	$(device_path)/usr/keypad.kl:system/usr/keylayout/sholes-keypad.kl \
	$(device_path)/usr/keypad.kl:system/usr/keylayout/umts_jordan-keypad.kl \
	$(device_path)/usr/cpcap-key.kl:system/usr/keylayout/cpcap-key.kl \
	$(device_path)/usr/keychars/cpcap-key.kcm:system/usr/keychars/cpcap-key.kcm \

PRODUCT_COPY_FILES += \
	${device_path}/vold.fstab:system/etc/vold.fstab \
	${device_path}/media_profiles.xml:system/etc/media_profiles.xml \
	${device_path}/modules/modules.alias:system/lib/modules/modules.alias \
	${device_path}/modules/modules.dep:system/lib/modules/modules.dep \
	$(device_path)/etc/init.d/01sysctl:system/etc/init.d/01sysctl \
	$(device_path)/etc/init.d/02baseband:system/etc/init.d/02baseband \
	$(device_path)/etc/init.d/03firstboot:system/etc/init.d/03firstboot \
	$(device_path)/etc/init.d/04filesystems:system/etc/init.d/04filesystems \
	$(device_path)/etc/init.d/05mountsd:system/etc/init.d/05mountsd \
	$(device_path)/etc/init.d/08backlight:system/etc/init.d/08backlight \
	$(device_path)/etc/init.d/10gpiofix:system/etc/init.d/10gpiofix \
	$(device_path)/etc/init.d/90multitouch:system/etc/init.d/90multitouch \
	$(device_path)/etc/profile:system/etc/profile \
	$(device_path)/etc/sysctl.conf:system/etc/sysctl.conf \
	$(device_path)/etc/busybox.fstab:system/etc/fstab \
	$(device_path)/etc/wifi/dnsmasq.conf:system/etc/wifi/dnsmasq.conf \
	$(device_path)/etc/wifi/tiwlan.ini:system/etc/wifi/tiwlan.ini \
	$(device_path)/etc/wifi/tiwlan_ap.ini:system/etc/wifi/tiwlan_ap.ini \
	$(device_path)/etc/wifi/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
	$(device_path)/etc/wifi/hostap_wrapper.sh:system/etc/wifi/hostap_wrapper.sh \
	$(device_path)/etc/gpsconfig.xml:system/etc/gpsconfig.xml \
	$(device_path)/etc/location.cfg:system/etc/location.cfg \
	$(device_path)/etc/media_codecs.xml:system/etc/media_codecs.xml \
	$(device_path)/etc/audio_policy.conf:system/etc/audio_policy.conf \

# New CM9 backup list system (addon.d)
PRODUCT_COPY_FILES += \
	${device_path}/releasetools/addon.d/60-baseband.sh:system/addon.d/60-baseband.sh \
	${device_path}/releasetools/addon.d/70-gapps.sh:system/addon.d/70-gapps.sh \
	${device_path}/releasetools/addon.d/80-battd.sh:system/addon.d/80-battd.sh \

# Backup kernel modules and bootmenu overclock config
ifndef CM_RELEASE
PRODUCT_COPY_FILES += \
	${device_path}/releasetools/addon.d/70-bootmenu.sh:system/addon.d/70-bootmenu.sh \

endif

#end of jordan-blobs.mk
