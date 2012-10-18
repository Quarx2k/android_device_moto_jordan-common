LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := hbootuser.c
LOCAL_MODULE := hbootuser
LOCAL_STATIC_LIBRARIES:= libc libcutils
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
