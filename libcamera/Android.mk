ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),jordan)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS    := eng debug
LOCAL_MODULE_PATH    := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE         := camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_SRC_FILES      := cameraHal.cpp JordanCameraWrapper.cpp
LOCAL_PRELINK_MODULE := false

LOCAL_C_INCLUDES += $(ANDROID_BUILD_TOP)/frameworks/native/include

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libmedia \
    libhardware \
    libcamera_client \
    libdl \
    libui \
    libstlport \

include external/stlport/libstlport.mk

include $(BUILD_SHARED_LIBRARY)

endif 
