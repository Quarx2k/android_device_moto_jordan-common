ifeq ($(TARGET_INIT_VENDOR_LIB),libinit_omap3)

LOCAL_PATH := $(call my-dir)
LIBINIT_MSM_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := system/core/init
LOCAL_CFLAGS := -Wall
LOCAL_SRC_FILES := init_omap3.c
LOCAL_MODULE := libinit_omap3
include $(BUILD_STATIC_LIBRARY)

endif
