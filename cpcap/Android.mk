LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := cpcap.c

LOCAL_C_INCLUDES := bionic/libc/kernel/common

LOCAL_SHARED_LIBRARIES := liblog libcutils

LOCAL_MODULE := cpcap
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

