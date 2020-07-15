LOCAL_PATH := $(call my-dir)


#--------------------------------------------------------
# libsrt.so
#
# LibSrt Loader
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := srt
LOCAL_SRC_FILES := ../lib/$(TARGET_ARCH_ABI)/lib/libsrt.a
#  export  headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../lib/$(TARGET_ARCH_ABI)/include/srt
include $(PREBUILT_STATIC_LIBRARY)

LOCAL_MODULE := crypto
LOCAL_SRC_FILES := ../lib/$(TARGET_ARCH_ABI)/lib/libcrypto.a
include $(PREBUILT_STATIC_LIBRARY)