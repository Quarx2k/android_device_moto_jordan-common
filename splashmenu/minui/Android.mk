LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := events.c resources.c

ifneq ($(TW_BOARD_CUSTOM_GRAPHICS),)
    LOCAL_SRC_FILES += $(TW_BOARD_CUSTOM_GRAPHICS) graphics_overlay.c
else
    LOCAL_SRC_FILES += graphics.c graphics_overlay.c
endif

ifndef RECOVERY_INCLUDE_DIR
    RECOVERY_INCLUDE_DIR := bootable/recovery/safestrap/devices/common/include
endif

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib \
    system/core/include \
    $(RECOVERY_INCLUDE_DIR)


ifeq ($(RECOVERY_TOUCHSCREEN_SWAP_XY), true)
LOCAL_CFLAGS += -DRECOVERY_TOUCHSCREEN_SWAP_XY
endif

ifeq ($(RECOVERY_TOUCHSCREEN_FLIP_X), true)
LOCAL_CFLAGS += -DRECOVERY_TOUCHSCREEN_FLIP_X
endif

ifeq ($(RECOVERY_TOUCHSCREEN_FLIP_Y), true)
LOCAL_CFLAGS += -DRECOVERY_TOUCHSCREEN_FLIP_Y
endif

ifeq ($(RECOVERY_GRAPHICS_USE_LINELENGTH), true)
LOCAL_CFLAGS += -DRECOVERY_GRAPHICS_USE_LINELENGTH
endif

ifeq ($(RECOVERY_GRAPHICS_DONT_DOUBLE_BUFFER), true)
LOCAL_CFLAGS += -DRECOVERY_GRAPHICS_DONT_DOUBLE_BUFFER
endif

ifeq ($(RECOVERY_GRAPHICS_DONT_SET_ACTIVATE), true)
LOCAL_CFLAGS += -DRECOVERY_GRAPHICS_DONT_SET_ACTIVATE
endif

#Remove the # from the line below to enable event logging
#TWRP_EVENT_LOGGING := true
ifeq ($(TWRP_EVENT_LOGGING), true)
LOCAL_CFLAGS += -D_EVENT_LOGGING
endif

ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGBX_8888")
  LOCAL_CFLAGS += -DRECOVERY_RGBX
endif
ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"BGRA_8888")
  LOCAL_CFLAGS += -DRECOVERY_BGRA
endif
ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGB_565")
  LOCAL_CFLAGS += -DRECOVERY_RGB_565
endif

ifeq ($(BOARD_HAS_FLIPPED_SCREEN), true)
LOCAL_CFLAGS += -DBOARD_HAS_FLIPPED_SCREEN
endif

ifeq ($(TW_IGNORE_VIRTUAL_KEYS), true)
LOCAL_CFLAGS += -DTW_IGNORE_VIRTUAL_KEYS
endif

ifeq ($(RECOVERY_SKIP_SCREENINFO_PUT), true)
  LOCAL_CFLAGS += -DSKIP_SCREENINFO_PUT
endif

ifneq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  LOCAL_CFLAGS += -DBOARD_USE_CUSTOM_RECOVERY_FONT=$(BOARD_USE_CUSTOM_RECOVERY_FONT)
endif

ifeq ($(TW_TARGET_USES_QCOM_BSP), true)
    LOCAL_CFLAGS += -DTW_QCOM_BSP
endif

LOCAL_STATIC_LIBRARIES += liblog libpng libpixelflinger_static
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libminui_ss

include $(BUILD_STATIC_LIBRARY)
