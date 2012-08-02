# reset our local path
LOCAL_PATH :=  $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := 2nd-init.c
LOCAL_CFLAGS := -Os
LOCAL_MODULE := 2nd-init
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES += libc
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/bootmenu/binary

include $(BUILD_EXECUTABLE)

