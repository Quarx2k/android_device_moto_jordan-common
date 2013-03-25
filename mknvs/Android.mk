LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := mknvs.c
LOCAL_MODULE := mknvs
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

