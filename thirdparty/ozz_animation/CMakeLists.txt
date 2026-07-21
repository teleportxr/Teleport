# Builds ozz-animation as a single static library for the Android AGDE build.
#
# The sources are fetched by the top-level CMakeLists (TELEPORT_ANDROID_MINIMAL_SETUP
# path) into ${CMAKE_BINARY_DIR}/ozz-animation; ozz's own build system is deliberately
# not used, keeping the generated output to one predictable project. When configured
# with -B build_android_vs and the AGDE toolchain, the generated project appears at
# build_android_vs/thirdparty/ozz_animation/ozz_animation.vcxproj, where the
# hand-rolled TeleportVRQuestClient.sln references it by its pinned GUID (see the
# ozz_animation_GUID_CMAKE cache entry set by the top-level CMakeLists).
#
# The library combines ozz_base, ozz_animation (runtime) and ozz_animation_offline —
# the three targets ClientRender links on PC. The globs below reproduce those targets'
# source lists for the pinned ozz version.

set(OZZ_DIR ${CMAKE_BINARY_DIR}/ozz-animation)
if(NOT EXISTS ${OZZ_DIR}/src)
	message(FATAL_ERROR "ozz-animation sources not found at ${OZZ_DIR}; configure with TELEPORT_ANDROID_MINIMAL_SETUP=ON from the top level.")
endif()

file(GLOB OZZ_BASE_SOURCES ${OZZ_DIR}/src/base/*.cc ${OZZ_DIR}/src/base/*/*.cc)
file(GLOB OZZ_RUNTIME_SOURCES ${OZZ_DIR}/src/animation/runtime/*.cc)
file(GLOB OZZ_OFFLINE_SOURCES ${OZZ_DIR}/src/animation/offline/*.cc)

add_library(ozz_animation STATIC ${OZZ_BASE_SOURCES} ${OZZ_RUNTIME_SOURCES} ${OZZ_OFFLINE_SOURCES})
target_include_directories(ozz_animation
	PUBLIC ${OZZ_DIR}/include
	PRIVATE ${OZZ_DIR}/src)
set_target_properties(ozz_animation PROPERTIES FOLDER thirdparty/ozz-animation)
