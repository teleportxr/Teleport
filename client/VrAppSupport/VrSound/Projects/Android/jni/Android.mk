LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvrsound.a
#
# VrSound
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE := vrsound

LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../../../../cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Include \
    $(LOCAL_PATH)/../../../../../VrApi/Include \
    $(LOCAL_PATH)/../../../../../1stParty/OVR/Include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../Include


LOCAL_SRC_FILES := 	../../../Src/SoundAssetMapping.cpp \
					../../../Src/SoundEffectContext.cpp \
					../../../Src/SoundPool.cpp

LOCAL_STATIC_LIBRARIES := vrappframework

include $(BUILD_STATIC_LIBRARY)

$(call import-module,VrAppFramework/Projects/Android/jni)
