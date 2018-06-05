LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := enet
LOCAL_SRC_FILES := ../../../Libs/Android/$(TARGET_ARCH_ABI)/lib$(LOCAL_MODULE).a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include

include $(PREBUILT_STATIC_LIBRARY)
