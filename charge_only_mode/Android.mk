# Copyright 2005 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	alarm.c \
	draw.c \
	events.c \
	hardware.c \
	screen.c \
	main.c

LOCAL_STATIC_LIBRARIES += libunz
LOCAL_SHARED_LIBRARIES += libhardware liblog libc libcutils

LOCAL_C_INCLUDES := external/zlib
LOCAL_MODULE_TAGS := eng debug
LOCAL_MODULE:= charge_only_mode

include $(BUILD_EXECUTABLE)

