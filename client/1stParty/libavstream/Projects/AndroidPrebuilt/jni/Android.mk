LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libavstream
LOCAL_SRC_FILES := ../../../Libs/Android/$(TARGET_ARCH_ABI)/$(LOCAL_MODULE).so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include

include $(PREBUILT_SHARED_LIBRARY)
