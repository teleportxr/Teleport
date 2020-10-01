LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../cflags.mk

LOCAL_MODULE			:= ovrapp
LOCAL_LDLIBS			:= -llog -landroid -lGLESv3 -lEGL  		# include default libraries
LOCAL_STATIC_LIBRARIES	:=  vrsound vrmodel vrlocale vrgui vrappframework libovrkernel enet libavstream SimulCasterRenderer SimulCasterAudio
LOCAL_SHARED_LIBRARIES	:= vrapi

LOCAL_SRC_FILES			:= \
    ../src/AndroidDiscoveryService.cpp \
    ../src/Application.cpp \
    ../src/Controllers.cpp \
    ../src/GLESDebug.cpp \
    ../src/GlobalGraphicsResources.cpp \
    ../src/OVRActorManager.cpp \
    ../src/VideoStreamClient.cpp \
    ../src/VideoDecoderProxy.cpp \
	../src/ClientRenderer.cpp\
	../src/LobbyRenderer.cpp\
    ../src/SCR_Class_GL_Impl/GL_DeviceContext.cpp \
    ../src/SCR_Class_GL_Impl/GL_Effect.cpp \
    ../src/SCR_Class_GL_Impl/GL_FrameBuffer.cpp \
    ../src/SCR_Class_GL_Impl/GL_IndexBuffer.cpp \
    ../src/SCR_Class_GL_Impl/GL_RenderPlatform.cpp \
    ../src/SCR_Class_GL_Impl/GL_Sampler.cpp \
    ../src/SCR_Class_GL_Impl/GL_Shader.cpp \
    ../src/SCR_Class_GL_Impl/GL_ShaderStorageBuffer.cpp \
    ../src/SCR_Class_GL_Impl/GL_Texture.cpp \
    ../src/SCR_Class_GL_Impl/GL_UniformBuffer.cpp \
    ../src/SCR_Class_GL_Impl/GL_VertexBuffer.cpp \

ifeq ($(APP_PLATFORM),$(filter $(APP_PLATFORM), android-26 android-27 android-28 android-29 android-30))
LOCAL_CFLAGS += -USING_AAUDIO
LOCAL_LDLIBS += -laaudio
LOCAL_SRC_FILES += ../src/SCR_Class_AAudio_Impl/AA_AudioPlayer.cpp
else
LOCAL_LDLIBS += -lOpenSLES
LOCAL_SRC_FILES += ../src/SCR_Class_SL_Impl/SL_AudioPlayer.cpp
endif

LOCAL_C_INCLUDES += ../1stParty/OVR/Include
LOCAL_C_INCLUDES += ../VrAppSupport/VrModel/Src
LOCAL_C_INCLUDES += ../VrAppSupport/VrSound/Include
LOCAL_C_INCLUDES += ../VrAppSupport/VrGUI/Src
LOCAL_C_INCLUDES += ../VrAppSupport/VrLocale/Include
LOCAL_C_INCLUDES += ../libavstream/include
LOCAL_C_INCLUDES += ../thirdparty/basis_universal
LOCAL_C_INCLUDES += ../SimulCasterRenderer/src
LOCAL_C_INCLUDES += ../SimulCasterAudio/src
LOCAL_C_INCLUDES += 3rdParty/enet/Include

LOCAL_CFLAGS += -D__ANDROID__
LOCAL_CPPFLAGS += -Wc++17-extensions -Wunused-variable -Wno-abstract-final-class
LOCAL_CPP_FEATURES += exceptions

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,VrAppFramework/Projects/Android/jni)
$(call import-module,VrAppSupport/VrGUI/Projects/Android/jni)
$(call import-module,VrAppSupport/VrLocale/Projects/Android/jni)
$(call import-module,VrAppSupport/VrModel/Projects/Android/jni)
$(call import-module,VrAppSupport/VrSound/Projects/Android/jni)
$(call import-module,../libavstream/jni)
$(call import-module,../libavstream/thirdparty/srt/build_android/jni)

$(call import-module,../SimulCasterRenderer/jni)
$(call import-module,../SimulCasterAudio/jni)
$(call import-module,3rdParty/enet/jni)
