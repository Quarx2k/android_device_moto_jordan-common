ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),jordan)

LOCAL_PATH:= $(call my-dir)

#Using prebuilt until fix.

#include $(CLEAR_VARS)

#LOCAL_SRC_FILES:= JordanCameraWrapper_test.cpp

#LOCAL_SHARED_LIBRARIES:= libdl libutils libcutils libcamera_client

#LOCAL_C_INCLUDES := $(ANDROID_BUILD_TOP)/frameworks/base/services/camera/libcameraservice
#LOCAL_MODULE := libcamera
#LOCAL_MODULE_TAGS := optional

#include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia \
    libcamera_client \
    libbinder \
    libhardware_legacy

LOCAL_SHARED_LIBRARIES += libdl

LOCAL_SRC_FILES := camera.cpp
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils
LOCAL_SHARED_LIBRARIES += libui libhardware libcamera_client
LOCAL_SHARED_LIBRARIES += libcamera
LOCAL_PRELINK_MODULE := false

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper

include $(BUILD_SHARED_LIBRARY)

endif
