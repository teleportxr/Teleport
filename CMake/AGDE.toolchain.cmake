# AGDE.toolchain.cmake
#
# Toolchain file that makes CMake's Visual Studio generator emit project files for the
# Android Game Development Extension (AGDE), i.e. vcxproj files targeting the MSBuild
# platform "Android-arm64-v8a" (or another Android-<abi>) with the "Clang" toolset —
# the same form as the hand-written projects in build_android_vs/.
#
# AGDE does not officially support CMake, so this works by construction rather than by
# any supported code path; see the notes below and CMake/README.md.
#
# Usage (Windows host with Visual Studio 2022, the AGDE extension, and the Android NDK):
#
#   cmake -B build_agde -S . -G "Visual Studio 17 2022" -A Android-arm64-v8a -T Clang ^
#         -DCMAKE_TOOLCHAIN_FILE=CMake/AGDE.toolchain.cmake
#
# The -A and -T arguments are optional; they default to Android-arm64-v8a and Clang.
#
# How it works
# ------------
# CMake's built-in Visual Studio support for CMAKE_SYSTEM_NAME=Android targets the
# *legacy* "Visual Studio Tools for Android" component: it emits
# <ApplicationType>Android</ApplicationType>, <Keyword>Android</Keyword>,
# <AndroidAPILevel> and <UseOfStl>, and defaults the toolset to Clang_5_0. The
# ApplicationType element reroutes MSBuild to
# "$(VCTargetsPath)\Application Type\Android\...", which does not contain AGDE's
# platforms, so those projects cannot build with AGDE.
#
# AGDE instead installs plain MSBuild platforms under
# "$(VCTargetsPath)\Platforms\Android-<abi>\", resolved purely from the <Platform>
# element of an ordinary Win32Proj-style vcxproj — which is exactly what the Visual
# Studio generator emits for any system name it does not special-case. So this file
# sets a *custom* system name, AndroidAGDE, to bypass the legacy path, and passes the
# AGDE platform and toolset through the normal -A / -T mechanism. The companion module
# Platform/AndroidAGDE.cmake (found via CMAKE_MODULE_PATH) supplies Android-appropriate
# platform defaults (artefact naming, ANDROID/UNIX variables, etc.).

if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
	message(FATAL_ERROR "AGDE.toolchain.cmake requires a Visual Studio generator (got '${CMAKE_GENERATOR}'). "
		"AGDE builds are driven by MSBuild; use e.g. -G \"Visual Studio 17 2022\".")
endif()

# Custom system name: deliberately NOT "Android", to avoid CMake's legacy VS-Android
# project emission described above.
set(CMAKE_SYSTEM_NAME AndroidAGDE)

# Make Platform/AndroidAGDE.cmake discoverable.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# ---------------------------------------------------------------------------
# Target ABI and platform selection.
# ---------------------------------------------------------------------------
if(NOT DEFINED TELEPORT_AGDE_ABI)
	if(DEFINED CMAKE_GENERATOR_PLATFORM AND CMAKE_GENERATOR_PLATFORM MATCHES "^Android-(.+)$")
		# Derive the ABI from a platform given on the command line via -A.
		set(TELEPORT_AGDE_ABI "${CMAKE_MATCH_1}")
	else()
		set(TELEPORT_AGDE_ABI "arm64-v8a" CACHE STRING "Android ABI for AGDE project generation")
	endif()
endif()

if(NOT DEFINED CMAKE_GENERATOR_PLATFORM OR CMAKE_GENERATOR_PLATFORM STREQUAL "")
	set(CMAKE_GENERATOR_PLATFORM "Android-${TELEPORT_AGDE_ABI}")
elseif(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Android-${TELEPORT_AGDE_ABI}")
	message(FATAL_ERROR "Generator platform '${CMAKE_GENERATOR_PLATFORM}' does not match TELEPORT_AGDE_ABI "
		"'${TELEPORT_AGDE_ABI}'. Pass -A Android-<abi> or set TELEPORT_AGDE_ABI, not both.")
endif()

# AGDE's toolset name is plain "Clang" (unlike the legacy Clang_5_0).
if(NOT DEFINED CMAKE_GENERATOR_TOOLSET OR CMAKE_GENERATOR_TOOLSET STREQUAL "")
	set(CMAKE_GENERATOR_TOOLSET "Clang")
endif()

if(TELEPORT_AGDE_ABI STREQUAL "arm64-v8a")
	set(CMAKE_SYSTEM_PROCESSOR aarch64)
elseif(TELEPORT_AGDE_ABI STREQUAL "armeabi-v7a")
	set(CMAKE_SYSTEM_PROCESSOR armv7-a)
elseif(TELEPORT_AGDE_ABI STREQUAL "x86_64")
	set(CMAKE_SYSTEM_PROCESSOR x86_64)
elseif(TELEPORT_AGDE_ABI STREQUAL "x86")
	set(CMAKE_SYSTEM_PROCESSOR i686)
else()
	message(FATAL_ERROR "Unknown Android ABI '${TELEPORT_AGDE_ABI}'.")
endif()

# ---------------------------------------------------------------------------
# Android API level and NDK version.
# ---------------------------------------------------------------------------
# API level: matches AndroidMinSdkVersion in the hand-written build_android_vs projects.
if(NOT DEFINED TELEPORT_AGDE_API)
	set(TELEPORT_AGDE_API 29)
endif()
set(CMAKE_SYSTEM_VERSION ${TELEPORT_AGDE_API})

if(NOT DEFINED TELEPORT_AGDE_MIN_SDK)
	set(TELEPORT_AGDE_MIN_SDK ${TELEPORT_AGDE_API})
endif()

# NDK version: read from release.properties (TELEPORT_ANDROID_NDK=...) so the single
# source of truth is shared with CI and the hand-written projects.
if(NOT DEFINED TELEPORT_AGDE_NDK_VERSION)
	set(_agde_release_properties "${CMAKE_CURRENT_LIST_DIR}/../release.properties")
	if(EXISTS "${_agde_release_properties}")
		file(STRINGS "${_agde_release_properties}" _agde_ndk_line REGEX "^TELEPORT_ANDROID_NDK=")
		if(_agde_ndk_line)
			string(REPLACE "TELEPORT_ANDROID_NDK=" "" TELEPORT_AGDE_NDK_VERSION "${_agde_ndk_line}")
		endif()
	endif()
	if(NOT TELEPORT_AGDE_NDK_VERSION)
		set(TELEPORT_AGDE_NDK_VERSION "27.2.12479018")
	endif()
endif()

# ---------------------------------------------------------------------------
# NDK discovery (for find_* isolation; AGDE locates the NDK itself when building).
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_ANDROID_NDK)
	if(DEFINED TELEPORT_ANDROID_NDK AND EXISTS "${TELEPORT_ANDROID_NDK}")
		set(CMAKE_ANDROID_NDK "${TELEPORT_ANDROID_NDK}")
	elseif(DEFINED ENV{ANDROID_NDK_ROOT} AND EXISTS "$ENV{ANDROID_NDK_ROOT}")
		set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
	elseif(DEFINED ENV{ANDROID_NDK_HOME} AND EXISTS "$ENV{ANDROID_NDK_HOME}")
		set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
	elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND EXISTS "$ENV{ANDROID_SDK_ROOT}/ndk/${TELEPORT_AGDE_NDK_VERSION}")
		set(CMAKE_ANDROID_NDK "$ENV{ANDROID_SDK_ROOT}/ndk/${TELEPORT_AGDE_NDK_VERSION}")
	elseif(DEFINED ENV{LOCALAPPDATA} AND EXISTS "$ENV{LOCALAPPDATA}/Android/Sdk/ndk/${TELEPORT_AGDE_NDK_VERSION}")
		set(CMAKE_ANDROID_NDK "$ENV{LOCALAPPDATA}/Android/Sdk/ndk/${TELEPORT_AGDE_NDK_VERSION}")
	endif()
	if(CMAKE_ANDROID_NDK)
		file(TO_CMAKE_PATH "${CMAKE_ANDROID_NDK}" CMAKE_ANDROID_NDK)
	endif()
endif()

# CMake's Visual Studio flow cannot extract the compiler path from AGDE builds: its
# probe resolves cl.exe via PATH inside a post-build event, and AGDE's build
# environment provides neither cl.exe nor clang.exe on PATH, so "No CMAKE_C_COMPILER
# could be found" results. Point CMake at the NDK's clang directly — the build itself
# is still driven by MSBuild and AGDE's Clang toolset; these paths inform CMake only.
# (Compiler *identification* still runs through MSBuild/AGDE and works.)
if(CMAKE_HOST_WIN32)
	if(NOT CMAKE_ANDROID_NDK)
		message(FATAL_ERROR "AGDE toolchain: Android NDK not found. Set ANDROID_NDK_HOME or "
			"ANDROID_NDK_ROOT in the environment, or pass -DTELEPORT_ANDROID_NDK=<path>.")
	endif()
	set(_agde_llvm_bin "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/bin")
	if(NOT DEFINED CMAKE_C_COMPILER)
		set(CMAKE_C_COMPILER "${_agde_llvm_bin}/clang.exe")
	endif()
	if(NOT DEFINED CMAKE_CXX_COMPILER)
		set(CMAKE_CXX_COMPILER "${_agde_llvm_bin}/clang++.exe")
	endif()
endif()

# Keep find_package/find_library/find_path away from host (Windows) libraries; programs
# such as build tools must still be found on the host.
if(CMAKE_ANDROID_NDK)
	set(CMAKE_FIND_ROOT_PATH "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# MSBuild properties injected into every generated vcxproj.
# ---------------------------------------------------------------------------
# These land in the Globals property group and mirror the values the hand-written
# build_android_vs projects set: minimum SDK, pinned NDK version and C++ standard.
# AGDE derives AndroidAPILevel and the toolchain paths from these plus the platform.
set(CMAKE_VS_GLOBALS
	"AndroidMinSdkVersion=${TELEPORT_AGDE_MIN_SDK}"
	"AndroidNdkVersion=${TELEPORT_AGDE_NDK_VERSION}"
	"CppLanguageStandard=cpp2a"
)

# ---------------------------------------------------------------------------
# agde_enable_packaging(<target> GRADLE_DIR <dir> [MODULE <name>])
#
# Marks an application target for AGDE's Gradle packaging step, mirroring the
# properties of build_android_vs/AndroidClient/TeleportVRQuestClientAGDE.vcxproj.
# GRADLE_DIR is the directory containing the Gradle project (settings.gradle etc.);
# MODULE defaults to "app". The application variant follows the build configuration.
# ---------------------------------------------------------------------------
function(agde_enable_packaging target)
	cmake_parse_arguments(AGDE "" "GRADLE_DIR;MODULE" "" ${ARGN})
	if(NOT AGDE_GRADLE_DIR)
		message(FATAL_ERROR "agde_enable_packaging: GRADLE_DIR is required.")
	endif()
	if(NOT AGDE_MODULE)
		set(AGDE_MODULE "app")
	endif()
	file(TO_NATIVE_PATH "${AGDE_GRADLE_DIR}" _agde_gradle_dir)
	set_target_properties(${target} PROPERTIES
		VS_GLOBAL_AndroidEnablePackaging "true"
		VS_GLOBAL_AndroidGradleBuildDir "${_agde_gradle_dir}\\"
		VS_GLOBAL_AndroidApplicationModule "${AGDE_MODULE}"
		VS_GLOBAL_AndroidApplicationVariant "$(Configuration)"
	)
endfunction()
