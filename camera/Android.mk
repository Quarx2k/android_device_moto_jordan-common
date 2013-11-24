include $(all-subdir-makefiles)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
	CameraHal_Module.cpp \
        V4L2Camera.cpp \
        CameraHardware.cpp \
        converter.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc/ \
    frameworks/base/include/ui \
    frameworks/base/include/utils \
    frameworks/base/include/media/stagefright \
    frameworks/base/include/media/stagefright/openmax \
    external/jpeg \
    external/jhead

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libbinder \
    libutils \
    libcutils \
    libcamera_client \
    libcameraservice \
    libgui \
    libjpeg \
    libexif



ifeq ($(TARGET_PRODUCT), flashboard)
LOCAL_CFLAGS += -DCONFIG_FLASHBOARD
endif

# This is for beaglebone camera cape
ifeq ($(TARGET_PRODUCT), beaglebone)
ifeq ($(BOARD_HAVE_CAMERA_CAPE),true)
LOCAL_CFLAGS += -DCONFIG_CAMERA_CAPE
endif
endif

# This is for beagleboneblack camera cape
ifeq ($(TARGET_PRODUCT), beagleboneblack)
ifeq ($(BOARD_HAVE_CAMERA_CAPE),true)
LOCAL_CFLAGS += -DCONFIG_CAMERA_CAPE
endif
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE:= camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_MODULE_TAGS:= optional

ifeq ($(BOARD_USB_CAMERA), true)
LOCAL_CFLAGS += -D_USE_USB_CAMERA_
endif

include $(BUILD_SHARED_LIBRARY)
