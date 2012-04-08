LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := cpcap.c

LOCAL_C_INCLUDES := bionic/libc/kernel/common

LOCAL_STATIC_LIBRARIES = libc
#LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_MODULE := cpcap
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

