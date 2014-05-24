# Copyright (C) 2013 The Android Open Source Project
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
# This file sets variables that control the way modules are built
# thorughout the system. It should not be used to conditionally
# disable makefiles (the proper mechanism to control what gets
# included in a build is to use PRODUCT_PACKAGES in a product
# definition file).
#

# WARNING: This line must come *before* including the proprietary
# variant, so that it gets overwritten by the parent (which goes
# against the traditional rules of inheritance).

TARGET_NO_RADIOIMAGE := true
TARGET_NO_BOOTLOADER := true
TARGET_NO_PREINSTALL := true

TARGET_BOOTLOADER_BOARD_NAME := jordan

# Board properties
TARGET_ARCH := arm
TARGET_BOARD_PLATFORM := omap3
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_ARCH_VARIANT_CPU := cortex-a8
TARGET_CPU_VARIANT := cortex-a8
TARGET_ARCH_VARIANT_FPU := neon
TARGET_OMAP3 := true
ARCH_ARM_HAVE_TLS_REGISTER := true
COMMON_GLOBAL_CFLAGS += -DOMAP_ENHANCEMENT -DTARGET_OMAP3
COMMON_GLOBAL_CFLAGS += -DNEEDS_VECTORIMPL_SYMBOLS -DBINDER_COMPAT
TARGET_GLOBAL_CFLAGS += -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp
TARGET_GLOBAL_CPPFLAGS += -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp

# Conserve memory in the Dalvik heap
# Details: https://github.com/CyanogenMod/android_dalvik/commit/15726c81059b74bf2352db29a3decfc4ea9c1428
TARGET_ARCH_LOWMEM := true
TARGET_ARCH_HAVE_NEON := true

TARGET_USE_KERNEL_BACKPORTS      := false
TARGET_SPECIFIC_HEADER_PATH := device/moto/jordan-common/include

# Wifi related defines
USES_TI_MAC80211 := true
COMMON_GLOBAL_CFLAGS += -DUSES_TI_MAC80211
WPA_SUPPLICANT_VERSION           := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_wl12xx
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_wl12xx
PRODUCT_WIRELESS_TOOLS           := true
BOARD_WLAN_DEVICE                := wl12xx_mac80211
BOARD_SOFTAP_DEVICE              := wl12xx_mac80211
ifeq ($(TARGET_USE_KERNEL_BACKPORTS),true)
WIFI_DRIVER_MODULE_PATH          := "/system/lib/modules/wlcore_sdio.ko"
WIFI_DRIVER_MODULE_NAME          := "wlcore_sdio"
else
WIFI_DRIVER_MODULE_PATH          := "/system/lib/modules/wl12xx_sdio.ko"
WIFI_DRIVER_MODULE_NAME          := "wl12xx_sdio"
endif
BOARD_WIFI_SKIP_CAPABILITIES     := true

# Bluetooth
BOARD_HAVE_BLUETOOTH := true
TARGET_USE_BLUEDROID_STACK := true
ifeq ($(TARGET_USE_BLUEDROID_STACK),true)
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/moto/jordan-common/bluetooth_bluedroid
#BOARD_HAVE_BLUETOOTH_TI := true  # We use own libs.
endif

# Usb Specific
BOARD_VOLD_EMMC_SHARES_DEV_MAJOR := true

# OMX Stuff
# BOARD_USES_TI_CAMERA_HAL := true
HARDWARE_OMX := true
OMX_JPEG := true
OMX_VENDOR := ti
OMX_VENDOR_INCLUDES := \
   hardware/ti/omx/system/src/openmax_il/omx_core/inc \
   hardware/ti/omx/image/src/openmax_il/jpeg_enc/inc
OMX_VENDOR_WRAPPER := TI_OMX_Wrapper
BOARD_OPENCORE_LIBRARIES := libOMX_Core
BOARD_OPENCORE_FLAGS := -DHARDWARE_OMX=1
#BOARD_CAMERA_LIBRARIES := libcamera

# TWRP Recovery
BOARD_HAS_NO_SELECT_BUTTON := true
TARGET_RECOVERY_FSTAB := device/moto/jordan-common/recovery.fstab
RECOVERY_FSTAB_VERSION := 2
TARGET_RECOVERY_INITRC := device/moto/jordan-common/ramdisk/init-recovery.rc
BOARD_HAS_LARGE_FILESYSTEM := true
BOARD_HAS_NO_MISC_PARTITION := true
BOARD_USES_MMCUTILS := true
TARGET_RECOVERY_PIXEL_FORMAT := "BGRA_8888"
DEVICE_RESOLUTION := 480x854
RECOVERY_GRAPHICS_USE_LINELENGTH := true
TW_EXTERNAL_STORAGE_PATH := "/sdcard"
TW_EXTERNAL_STORAGE_MOUNT_POINT := "sdcard"
TW_DEFAULT_EXTERNAL_STORAGE := true
TW_FLASH_FROM_STORAGE := true 
BOARD_USE_CUSTOM_RECOVERY_FONT:= \"roboto_10x18.h\"
TW_BRIGHTNESS_PATH := /sys/class/leds/lcd-backlight/brightness
TW_MAX_BRIGHTNESS := 255
TW_NO_BATT_PERCENT := false
TW_NO_REBOOT_RECOVERY := true
TW_NO_REBOOT_BOOTLOADER := true
TW_ALWAYS_RMRF := true
BOARD_UMS_LUNFILE := /sys/class/android_usb/f_mass_storage/lun/file
TW_NO_SCREEN_BLANK := true
TW_HAS_NO_RECOVERY_PARTITION := true
TW_HAS_NO_BOOT_PARTITION := true
TW_NO_SCREEN_TIMEOUT := true

TARGET_RECOVERY_PRE_COMMAND :=  "echo recovery > /cache/recovery/bootmode.conf; sync; \#"
TARGET_RECOVERY_PRE_COMMAND_CLEAR_REASON := true

# Egl Specific
USE_OPENGL_RENDERER := true
ENABLE_WEBGL := true
COMMON_GLOBAL_CFLAGS += -DSYSTEMUI_PBSIZE_HACK=1
COMMON_GLOBAL_CFLAGS += -DWORKAROUND_BUG_10194508=1
COMMON_GLOBAL_CFLAGS += -DHAS_CONTEXT_PRIORITY -DDONT_USE_FENCE_SYNC
TARGET_DISABLE_TRIPLE_BUFFERING := true
TARGET_RUNNING_WITHOUT_SYNC_FRAMEWORK := false
# Increase EGL cache size to 2MB
#MAX_EGL_CACHE_SIZE := 2097152
#MAX_EGL_CACHE_KEY_SIZE := 4096
# OMAP3 HWC: disable use of YUV overlays
# Prevents stuttering/compositing artifacts and sync loss during video playback
TARGET_OMAP3_HWC_DISABLE_YUV_OVERLAY := true

# Camera
USE_CAMERA_STUB := false
BOARD_OVERLAY_BASED_CAMERA_HAL := true

# Audio
BOARD_USES_AUDIO_LEGACY := true

ifeq ($(BOARD_USES_AUDIO_LEGACY),false)
BOARD_USES_GENERIC_AUDIO := false
BOARD_USES_ALSA_AUDIO := true
BUILD_WITH_ALSA_UTILS := true
else
COMMON_GLOBAL_CFLAGS += -DUSES_AUDIO_LEGACY
TARGET_PROVIDES_LIBAUDIO := true
BOARD_USE_KINETO_COMPATIBILITY := true
BOARD_USE_HARDCODED_FAST_TRACK_LATENCY_WHEN_DENIED := 160
endif
HAVE_2_3_DSP := 1

# Boot animation
TARGET_BOOTANIMATION_PRELOAD := true
TARGET_BOOTANIMATION_TEXTURE_CACHE := true
TARGET_BOOTANIMATION_USE_RGB565 := true
TARGET_CONTINUOUS_SPLASH_ENABLED := false

# Other..
BOARD_USES_LEGACY_RIL := true
BOARD_USE_LEGACY_SENSORS_FUSION := false
BOARD_HARDWARE_CLASS := device/moto/jordan-common/cmhw/

# Override healthd HAL to use charge_counter for 1%
BOARD_HAL_STATIC_LIBRARIES := libhealthd.omap3

# Release tool
TARGET_PROVIDES_RELEASETOOLS := true
TARGET_RELEASETOOL_OTA_FROM_TARGET_SCRIPT := build/tools/releasetools/ota_from_target_files --device_specific device/moto/jordan-common/releasetools/jordan-common_ota_from_target_files.py
TARGET_SYSTEMIMAGE_USE_SQUISHER := true

ext_modules:
	make -C $(TARGET_KERNEL_MODULES_EXT) modules KERNEL_DIR=$(KERNEL_OUT) ARCH=arm CROSS_COMPILE="arm-eabi-"
	find $(TARGET_KERNEL_MODULES_EXT)/ -name "*.ko" -exec mv {} \
		$(KERNEL_MODULES_OUT) \; || true

COMPAT_MODULES:
	make mrproper -C device/moto/jordan-common/modules/backports
	make -C device/moto/jordan-common/modules/backports KERNEL_DIR=$(KERNEL_OUT) KLIB=$(KERNEL_OUT) KLIB_BUILD=$(KERNEL_OUT) ARCH=arm CROSS_COMPILE="arm-eabi-" defconfig-mapphone
	make -C device/moto/jordan-common/modules/backports KERNEL_DIR=$(KERNEL_OUT) KLIB=$(KERNEL_OUT) KLIB_BUILD=$(KERNEL_OUT) ARCH=arm CROSS_COMPILE="arm-eabi-"
	mv device/moto/jordan-common/modules/backports/compat/compat.ko $(KERNEL_MODULES_OUT)
ifeq ($(TARGET_USE_BLUEDROID_STACK),false)
	mv device/moto/jordan-common/modules/backports/net/bluetooth/bluetooth.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/net/bluetooth/bnep/bnep.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/net/bluetooth/rfcomm/rfcomm.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/drivers/bluetooth/btwilink.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/drivers/bluetooth/hci_uart.ko $(KERNEL_MODULES_OUT)
endif
	mv device/moto/jordan-common/modules/backports/net/mac80211/mac80211.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/net/wireless/cfg80211.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/drivers/net/wireless/ti/wlcore/wlcore.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/drivers/net/wireless/ti/wl12xx/wl12xx.ko $(KERNEL_MODULES_OUT)
	mv device/moto/jordan-common/modules/backports/drivers/net/wireless/ti/wlcore/wlcore_sdio.ko $(KERNEL_MODULES_OUT)
	arm-linux-androideabi-strip --strip-unneeded $(KERNEL_MODULES_OUT)/*

WLAN_MODULES:
	make -C hardware/ti/wlan/mac80211/compat_wl12xx KERNEL_DIR=$(KERNEL_OUT) KLIB=$(KERNEL_OUT) KLIB_BUILD=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) $(ARM_CROSS_COMPILE) clean
	make -C hardware/ti/wlan/mac80211/compat_wl12xx KERNEL_DIR=$(KERNEL_OUT) KLIB=$(KERNEL_OUT) KLIB_BUILD=$(KERNEL_OUT) ARCH=arm CROSS_COMPILE="arm-eabi-"
	mv hardware/ti/wlan/mac80211/compat_wl12xx/compat/compat.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/net/mac80211/mac80211.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/net/wireless/cfg80211.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/drivers/net/wireless/wl12xx/wl12xx.ko $(KERNEL_MODULES_OUT)
	mv hardware/ti/wlan/mac80211/compat_wl12xx/drivers/net/wireless/wl12xx/wl12xx_sdio.ko $(KERNEL_MODULES_OUT)
	arm-linux-androideabi-strip --strip-unneeded $(KERNEL_MODULES_OUT)/*

hboot:
	mkdir -p $(PRODUCT_OUT)/system/bootstrap/2nd-boot   
	make -C  $(ANDROID_BUILD_TOP)/device/moto/jordan-common/bootstrap/hboot ARCH=arm CROSS_COMPILE="arm-eabi-"
	mv $(ANDROID_BUILD_TOP)/device/moto/jordan-common/bootstrap/hboot/hboot.bin $(PRODUCT_OUT)/system/bootstrap/2nd-boot/
	make clean -C $(ANDROID_BUILD_TOP)/device/moto/jordan-common/bootstrap/hboot

# If kernel sources are present in repo, here is the location
TARGET_KERNEL_SOURCE := $(ANDROID_BUILD_TOP)/jordan-kernel
BOARD_KERNEL_CMDLINE := console=/dev/null mem=500M omapfb.vram=0:4M
# Extra: external modules sources
TARGET_KERNEL_MODULES_EXT := $(ANDROID_BUILD_TOP)/device/moto/jordan-common/modules/sources/

ifeq ($(TARGET_USE_KERNEL_BACKPORTS),true)
TARGET_KERNEL_MODULES := ext_modules hboot WLAN_MODULES COMPAT_MODULES
else
TARGET_KERNEL_MODULES := hboot WLAN_MODULES #ext_modules
endif

