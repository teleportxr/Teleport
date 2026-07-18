# Standalone smoke test for CMake/AGDE.toolchain.cmake.
#
# Generates one static library and one application target so that AGDE-style vcxproj
# generation and an MSBuild build can be verified without touching the main Teleport
# build. See CMake/README.md for the full verification steps. Configure with:
#
#   cmake -B build_agde_test -S Teleport/CMake/AGDETest -G "Visual Studio 17 2022" ^
#         -A Android-arm64-v8a -T Clang -DCMAKE_TOOLCHAIN_FILE=../AGDE.toolchain.cmake
#
# The sources also build on a host compiler (no toolchain) as a quick sanity check.

cmake_minimum_required(VERSION 3.21)
project(AGDESmokeTest CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(SmokeLib STATIC SmokeLib.cpp SmokeLib.h)
target_include_directories(SmokeLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(SmokeApp SmokeMain.cpp)
target_link_libraries(SmokeApp PRIVATE SmokeLib)

if(ANDROID)
	message(STATUS "AGDE smoke test: generating for platform '${CMAKE_GENERATOR_PLATFORM}', "
		"toolset '${CMAKE_GENERATOR_TOOLSET}', NDK ${TELEPORT_AGDE_NDK_VERSION}")
endif()
