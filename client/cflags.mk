# This file is included in all .mk files to ensure their compilation flags are in sync
# across debug and release builds.
ENABLE_SANITIZER := 0

LOCAL_CFLAGS	:= -DANDROID_NDK -DPLATFORM_ANDROID
#LOCAL_CFLAGS	+= -Werror			# error on warnings
LOCAL_CFLAGS	+= -Wall
LOCAL_CFLAGS	+= -Wextra
#LOCAL_CFLAGS	+= -Wlogical-op		# not part of -Wall or -Wextra
#LOCAL_CFLAGS	+= -Weffc++			# too many issues to fix for now
LOCAL_CFLAGS	+= -Wno-strict-aliasing		# TODO: need to rewrite some code
LOCAL_CFLAGS	+= -Wno-unused-parameter
LOCAL_CFLAGS	+= -Wno-missing-field-initializers	# warns on this: SwipeAction	ret = {}
LOCAL_CFLAGS	+= -Wno-multichar	# used in internal Android headers:  DISPLAY_EVENT_VSYNC = 'vsyn',
LOCAL_CPPFLAGS  += -Wno-invalid-offsetof -Wno-address-of-packed-member
LOCAL_CPPFLAGS  += -std=c++17
LOCAL_CPPFLAGS  += -frtti
LOCAL_CPPFLAGS += -fsigned-char

ifeq ($(OVR_DEBUG),1)
  LOCAL_CFLAGS += -DOVR_BUILD_DEBUG=1 -DDEBUG -D_DEBUG -O0 -g
  ifeq ($(ENABLE_SANITIZER),1)
    $(info "-----------ENABLE_SANITIZER-----------")
    ifeq ($(USE_ASAN),1)
      LOCAL_CFLAGS += -fsanitize=address -fno-omit-frame-pointer
      LOCAL_CPPFLAGS += -fsanitize=address -fno-omit-frame-pointer
      LOCAL_LDFLAGS += -fsanitize=address
    endif
  endif
else
  LOCAL_CFLAGS += -DNDEBUG -O3 -g -DOVR_BUILD_DEBUG=0
  LOCAL_CPPFLAGS += -DNDEBUG -O3 -g  -DOVR_BUILD_DEBUG=0
endif

# Explicitly compile for the ARM and not the Thumb instruction set.
LOCAL_ARM_MODE := arm
