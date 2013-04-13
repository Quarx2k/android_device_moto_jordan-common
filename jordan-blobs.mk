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
	$(device_path)/prebuilt/etc/terminfo/l/linux:system/etc/terminfo/l/linux \
	$(device_path)/prebuilt/etc/terminfo/x/xterm:system/etc/terminfo/x/xterm \

# Key layouts, names must fit the ones in /proc/bus/input/devices, qwerty.kl is the fallback one.
PRODUCT_COPY_FILES += \
	$(device_path)/prebuilt/usr/idc/internal.idc:system/usr/idc/lm3530_led.idc \
	$(device_path)/prebuilt/usr/idc/internal.idc:system/usr/idc/accelerometer.idc \
	$(device_path)/prebuilt/usr/idc/internal.idc:system/usr/idc/compass.idc \
	$(device_path)/prebuilt/usr/idc/internal.idc:system/usr/idc/light-prox.idc \
	$(device_path)/prebuilt/usr/idc/internal.idc:system/usr/idc/proximity.idc \
	$(device_path)/prebuilt/usr/idc/sholes-keypad.idc:system/usr/idc/sholes-keypad.idc \
	$(device_path)/prebuilt/usr/idc/cpcap-key.idc:system/usr/idc/cpcap-key.idc \
	$(device_path)/prebuilt/usr/idc/qtouch-touchscreen.idc:system/usr/idc/qtouch-touchscreen.idc \
	$(device_path)/prebuilt/usr/qwerty.kl:system/usr/keylayout/qwerty.kl \
	$(device_path)/prebuilt/usr/qwerty.kl:system/usr/keylayout/qtouch-touchscreen.kl \
	$(device_path)/prebuilt/usr/keypad.kl:system/usr/keylayout/sholes-keypad.kl \
	$(device_path)/prebuilt/usr/keypad.kl:system/usr/keylayout/umts_jordan-keypad.kl \
	$(device_path)/prebuilt/usr/cpcap-key.kl:system/usr/keylayout/cpcap-key.kl \
	$(device_path)/prebuilt/usr/keychars/cpcap-key.kcm:system/usr/keychars/cpcap-key.kcm \

# scripts
PRODUCT_COPY_FILES += \
	${device_path}/prebuilt/bin/handle_bp_panic.sh:system/bin/handle_bp_panic.sh \
	${device_path}/prebuilt/bin/wifical.sh:system/bin/wifical.sh \

PRODUCT_COPY_FILES += \
	${device_path}/prebuilt/etc/vold.fstab:system/etc/vold.fstab \
	$(device_path)/prebuilt/etc/init.d/01sysctl:system/etc/init.d/01sysctl \
	$(device_path)/prebuilt/etc/init.d/02baseband:system/etc/init.d/02baseband \
	$(device_path)/prebuilt/etc/init.d/05mountsd:system/etc/init.d/05mountsd \
	$(device_path)/prebuilt/etc/init.d/08backlight:system/etc/init.d/08backlight \
	$(device_path)/prebuilt/etc/init.d/90multitouch:system/etc/init.d/90multitouch \
	$(device_path)/prebuilt/etc/profile:system/etc/profile \
	$(device_path)/prebuilt/etc/sysctl.conf:system/etc/sysctl.conf \
	$(device_path)/prebuilt/etc/busybox.fstab:system/etc/fstab \
	$(device_path)/prebuilt/etc/gpsconfig.xml:system/etc/gpsconfig.xml \
	$(device_path)/prebuilt/etc/location.cfg:system/etc/location.cfg \
	$(device_path)/prebuilt/etc/media_codecs.xml:system/etc/media_codecs.xml \
	$(device_path)/prebuilt/etc/audio_policy.conf:system/etc/audio_policy.conf \

# WLAN/WPAN firmware
PRODUCT_COPY_FILES += \
    $(device_path)/prebuilt/etc/firmware/ti-connectivity/wl127x-fw-4-mr.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-mr.bin \
    $(device_path)/prebuilt/etc/firmware/ti-connectivity/wl127x-fw-4-plt.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-plt.bin \
    $(device_path)/prebuilt/etc/firmware/ti-connectivity/wl127x-fw-4-sr.bin:system/etc/firmware/ti-connectivity/wl127x-fw-4-sr.bin \
    $(device_path)/prebuilt/etc/firmware/TIInit_7.2.31.bts:system/etc/firmware/TIInit_7.2.31.bts \


# New CM9 backup list system (addon.d)
PRODUCT_COPY_FILES += \
	${device_path}/releasetools/addon.d/60-baseband.sh:system/addon.d/60-baseband.sh \
	${device_path}/releasetools/addon.d/70-gapps.sh:system/addon.d/70-gapps.sh \
	${device_path}/releasetools/addon.d/80-battd.sh:system/addon.d/80-battd.sh \
	${device_path}/releasetools/addon.d/70-bootmenu.sh:system/addon.d/70-bootmenu.sh \

# copy all others kernel modules under the "modules" directory to system/lib/modules
PRODUCT_COPY_FILES += $(shell test -d device/moto/jordan-common/modules/prebuilt && \
	find device/moto/jordan-common/modules/prebuilt -name '*.ko' \
	-printf '%p:system/lib/modules/%f ')

#end of jordan-blobs.mk
