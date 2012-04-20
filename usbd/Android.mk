# Copyright 2005 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        usbd.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := usbd.jordan
LOCAL_MODULE_STEM := usbd

LOCAL_SHARED_LIBRARIES := libcutils libc

include $(BUILD_EXECUTABLE) 
