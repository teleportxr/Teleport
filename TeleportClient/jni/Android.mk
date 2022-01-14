LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# TeleportClient.a
#
# Crossplatform Cient code for Teleport (PC and Android)
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := TeleportClient	        # generate TeleportClient.a
LOCAL_STATIC_LIBRARIES	:= Basis_universal enet

include $(LOCAL_PATH)/../../client/cflags.mk

LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../libavstream/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../thirdparty/enet/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../thirdparty/basis_universal
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../thirdparty/basis_universal/transcoder
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../client/VrApi/Include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../client/VrAppFramework/Include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../client/LibOVRKernel/Src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../client/1stParty/OpenGL_Loader/Include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../firstparty
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES :=	../ClientDeviceState.cpp					\
					../ClientPipeline.cpp						\
					../ServerTimestamp.cpp						\
					../Input.cpp						\
					../Log.cpp						\
					../SessionClient.cpp	\

LOCAL_CFLAGS += -D__ANDROID__
LOCAL_CPPFLAGS += -Wc++17-extensions -Wunused-variable
LOCAL_CPP_FEATURES += exceptions
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module, ../thirdparty/basis_universal/jni)
$(call import-module, 3rdParty/enet/jni)
