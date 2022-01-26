LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# TeleportAudio.a
#
# Crossplatform Audio library for SimulCasterClients (PC and Android)
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

include $(LOCAL_PATH)/../../client/cflags.mk

LOCAL_MODULE    := TeleportAudio	        # generate TeleportAudio.a

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src/crossplatform
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../libavstream/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../TeleportCore

LOCAL_STATIC_LIBRARIES	:= libavstream

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := 	../src/AudioCommon.cpp				        \
						../src/crossplatform/AudioPlayer.cpp		\
						../src/crossplatform/AudioStreamTarget.cpp  \
						../src/crossplatform/CustomAudioStreamTarget.cpp  \
						../src/crossplatform/NetworkPipeline.cpp



LOCAL_CFLAGS += -D__ANDROID__
LOCAL_CPPFLAGS += -Wc++17-extensions -Wunused-variable
LOCAL_CPP_FEATURES += exceptions
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS



