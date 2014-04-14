LOCAL_PATH:= $(call my-dir)

# Raw memory i/o utility
# http://www.sleepie.demon.co.uk/linuxvme/io.c

# Description:
#  This tool can be used to access physical memory addresses from userspace.
#  It can be useful to access hardware for which no device driver exists!
#
# Forked from OpenWRT io Package (v2.0)
#  Fix pointer arithmetic warnings, and setup for Android Repo

include $(CLEAR_VARS)

LOCAL_SRC_FILES := io.c
LOCAL_MODULE := io
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# devmem2 -- Simple program to read/write from/to any hardware address
# http://man.cx/devmem2(1)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := devmem2.c
LOCAL_MODULE := devmem2
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# cpcap -- Tool for reading cpcap registers.

include $(CLEAR_VARS)

LOCAL_SRC_FILES := cpcap.c
LOCAL_C_INCLUDES := bionic/libc/kernel/common
LOCAL_STATIC_LIBRARIES = libc
LOCAL_MODULE := cpcap
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# 2ndinit
include $(CLEAR_VARS)

LOCAL_SRC_FILES := 2nd-init.c
LOCAL_CFLAGS := -Os
LOCAL_MODULE := 2nd-init
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES += libc
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/system/bootmenu/binary

include $(BUILD_EXECUTABLE)

# Camera_detect
include $(CLEAR_VARS)

LOCAL_SRC_FILES := camera_detect.c
LOCAL_CFLAGS := -Os
LOCAL_MODULE := camera_detect
LOCAL_MODULE_TAGS := optional
LOCAL_STATIC_LIBRARIES += libc libcutils
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/recovery/root/sbin

include $(BUILD_EXECUTABLE)

