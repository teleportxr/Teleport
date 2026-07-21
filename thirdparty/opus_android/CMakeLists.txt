# Builds libopus as a single static library for the Android AGDE build.
#
# The sources are fetched by the top-level CMakeLists (TELEPORT_ANDROID_MINIMAL_SETUP
# path) into ${CMAKE_BINARY_DIR}/opus; opus's own build system is deliberately not
# used, keeping the generated output to one predictable project. When configured
# with -B build_android_vs and the AGDE toolchain, the generated project appears at
# build_android_vs/thirdparty/opus_android/opus.vcxproj, where the
# hand-rolled TeleportVRQuestClient.sln references it by its pinned GUID (see the
# opus_GUID_CMAKE cache entry set by the top-level CMakeLists).
#
# The lists below reproduce the canonical float build from opus_sources.mk,
# celt_sources.mk and silk_sources.mk: the common sources plus silk/float, with
# arch-specific (x86/ARM) and demo program sources excluded.

set(OPUS_DIR ${CMAKE_BINARY_DIR}/opus)
if(NOT EXISTS ${OPUS_DIR}/src)
	message(FATAL_ERROR "opus sources not found at ${OPUS_DIR}; configure with TELEPORT_ANDROID_MINIMAL_SETUP=ON from the top level.")
endif()

file(GLOB OPUS_SOURCES ${OPUS_DIR}/src/*.c)
list(FILTER OPUS_SOURCES EXCLUDE REGEX "(_demo|opus_compare)\\.c$")
file(GLOB CELT_SOURCES ${OPUS_DIR}/celt/*.c)
list(FILTER CELT_SOURCES EXCLUDE REGEX "_demo\\.c$")
file(GLOB SILK_SOURCES ${OPUS_DIR}/silk/*.c ${OPUS_DIR}/silk/float/*.c)

add_library(opus STATIC ${OPUS_SOURCES} ${CELT_SOURCES} ${SILK_SOURCES})
target_include_directories(opus
	PUBLIC ${OPUS_DIR}/include
	PRIVATE ${OPUS_DIR}/celt ${OPUS_DIR}/silk ${OPUS_DIR}/silk/float ${OPUS_DIR}/src)
# USE_ALLOCA/HAVE_ALLOCA_H select opus's temporary-allocation mode (required);
# alloca.h is provided by the Android NDK.
target_compile_definitions(opus PRIVATE OPUS_BUILD USE_ALLOCA HAVE_ALLOCA_H)
set_target_properties(opus PROPERTIES FOLDER thirdparty/opus)
