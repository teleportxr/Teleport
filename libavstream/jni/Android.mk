LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libvavstream.a
#
# AVStream
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := avstream	        # generate libavstream.a

include $(LOCAL_PATH)/../../client/cflags.mk
LOCAL_CFLAGS	+= -DLIBAV_USE_SRT=1

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../include

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/nv/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/asio/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/efp

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../thirdparty/srt/build_android/$(TARGET_ARCH_ABI)/include/srt

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := 	../src/context.cpp \
                        ../src/pipeline.cpp \
                        ../src/node.cpp \
                        ../src/decoder.cpp \
                        ../src/audiodecoder.cpp \
                        ../src/geometrydecoder.cpp \
                        ../src/geometryencoder.cpp \
                        ../src/mesh.cpp \
                        ../src/surface.cpp \
                        ../src/buffer.cpp \
                        ../src/queue.cpp \
                        ../src/file.cpp \
                        ../src/memory.cpp \
                        ../src/forwarder.cpp \
                        ../src/packetizer.cpp \
                        ../src/nullsink.cpp \
                        ../src/networksink.cpp \
                        ../src/networksource.cpp \
                        ../src/api/cuda.cpp \
                        ../src/stream/parser.cpp \
                        ../src/stream/parser_avc.cpp \
                        ../src/stream/parser_geo.cpp \
                        ../src/decoders/dec_nvidia.cpp \
                        ../src/libraryloader.cpp \
                        ../src/platforms/platform_posix.cpp \
                        ../src/geometrydecoder.cpp \
                        ../src/mesh.cpp \
                        ../src/util/srtutil.cpp \
                        ../src/audio/audiotarget.cpp

LOCAL_CFLAGS += -DASIO_STANDALONE -DSRT_NO_DEPRECATED
LOCAL_CPPFLAGS += -Wc++17-extensions
LOCAL_CPP_FEATURES += exceptions
LOCAL_STATIC_LIBRARIES += stb srt crypto efp
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,3rdParty/minizip/build/android/jni)
$(call import-module,3rdParty/stb/build/android/jni)
$(call import-module,../libavstream/thirdparty/srt/build_android/jni)
$(call import-module,../libavstream/thirdparty/efp/jni)

# Note: Even though we depend on LibOVRKernel, we don't explicitly import it since our
# dependents may want either a prebuilt or from-source LibOVRKernel.
