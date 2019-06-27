LOCAL_PATH := $(call my-dir)

#--------------------------------------------------------
# libenet.a
#
# enet
#--------------------------------------------------------
include $(CLEAR_VARS)				# clean everything up to prepare for a module

LOCAL_MODULE    := enet	            # generate enet.a

include $(LOCAL_PATH)/../../../cflags.mk

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../Include

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES  := 	../src/callbacks.c \
                        ../src/compress.c \
                        ../src/host.c \
                        ../src/list.c \
                        ../src/packet.c \
                        ../src/peer.c \
                        ../src/protocol.c \
                        ../src/unix.c \
                        ../src/win32.c

LOCAL_CFLAGS += -DENET_DEBUG
LOCAL_CFLAGS += -DHAS_SOCKLEN_T

LOCAL_CPPFLAGS += -Wc++17-extensions
LOCAL_CPP_FEATURES += exceptions

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS
