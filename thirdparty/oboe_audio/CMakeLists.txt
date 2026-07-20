# Builds oboe as a single static library for the Android AGDE build.
#
# The sources are fetched by the top-level CMakeLists (TELEPORT_ANDROID_MINIMAL_SETUP
# path) into ${CMAKE_BINARY_DIR}/oboe-audio; oboe's own build system is deliberately
# not used, keeping the generated output to one predictable project. When configured
# with -B build_android_vs and the AGDE toolchain, the generated project appears at
# build_android_vs/thirdparty/oboe_audio/oboe_audio.vcxproj, where the
# hand-rolled TeleportVRQuestClient.sln references it by its pinned GUID (see the
# oboe_audio_GUID_CMAKE cache entry set by the top-level CMakeLists).

set(OBOE_DIR ${CMAKE_BINARY_DIR}/oboe-audio)
if(NOT EXISTS ${OBOE_DIR}/src)
	message(FATAL_ERROR "oboe sources not found at ${OBOE_DIR}; configure with TELEPORT_ANDROID_MINIMAL_SETUP=ON from the top level.")
endif()

file(GLOB OBOE_SOURCES
	${OBOE_DIR}/src/*.cpp
	${OBOE_DIR}/src/common/*.cpp
	${OBOE_DIR}/src/aaudio/*.cpp
	${OBOE_DIR}/src/opensles/*.cpp
	${OBOE_DIR}/src/flowgraph/*.cpp
	${OBOE_DIR}/src/resampler/*.cpp)

add_library(oboe_audio STATIC ${OBOE_SOURCES})
target_include_directories(oboe_audio
	PUBLIC ${OBOE_DIR}/include
	PRIVATE ${OBOE_DIR}/src)
target_compile_definitions(oboe_audio PRIVATE OBOE_VERSION_NUMBER=190)
set_target_properties(oboe_audio PROPERTIES FOLDER thirdparty/oboe)
