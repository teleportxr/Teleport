LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../../cflags.mk

LOCAL_MODULE			:= ovrapp
LOCAL_STATIC_LIBRARIES	:= vrsound vrmodel vrlocale vrgui vrappframework libovrkernel enet
LOCAL_SHARED_LIBRARIES	:= vrapi libavstream

LOCAL_SRC_FILES			:= \
    ../../../Src/Application.cpp \
    ../../../Src/SessionClient.cpp \
    ../../../Src/VideoStreamClient.cpp \
    ../../../Src/VideoDecoderProxy.cpp

LOCAL_C_INCLUDES += ../../../../../../../libavstream/include
LOCAL_C_INCLUDES += ../../../../../../libavstream/include
LOCAL_C_INCLUDES += ../../../../../../../../libavstream/include

include $(BUILD_SHARED_LIBRARY)

$(call import-module,LibOVRKernel/Projects/Android/jni)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrModel/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)
$(call import-module,../libavstream/jni)
$(call import-module,3rdParty/enet/Projects/AndroidPrebuilt/jni)
