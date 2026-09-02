cmake_minimum_required(VERSION 3.11)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(TELEPORT_VERSION 1.0.0)

# Ensure CMake doesn't complain about older libraries.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(NOT DEFINED TELEPORT_SOLUTION_NAME OR "${TELEPORT_SOLUTION_NAME}" MATCHES "")
	if(TELEPORT_BUILD_DOCS)
		set(VS_STARTUP_PROJECT Sphinx)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sphinx)
	endif()
	project(Teleport VERSION ${TELEPORT_VERSION})
else() 
	project("${TELEPORT_SOLUTION_NAME}" VERSION ${TELEPORT_VERSION})
endif()

message("TELEPORT_SOLUTION_NAME: ${TELEPORT_SOLUTION_NAME}")
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")


# TELEPORT_UNIX means "POSIX" and is ON for both Linux and macOS; TELEPORT_LINUX and
# TELEPORT_MACOS distinguish the two where the difference matters (packaging, PulseAudio,
# the ThisPlatform shim, prctl-based thread naming).
set(TELEPORT_UNIX OFF)
set(TELEPORT_WINDOWS OFF)
set(TELEPORT_LINUX OFF)
set(TELEPORT_MACOS OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(TELEPORT_WINDOWS ON)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(TELEPORT_MACOS ON)
	set(TELEPORT_UNIX ON)
endif()

set(CMAKE_CXX_STANDARD 20 )
set(PLATFORM_CPP_VERSION 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON )
set(CMAKE_CXX_EXTENSIONS ON )

# assume built-in pthreads on Linux/MacOS
message("CMAKE_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}")
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(CMAKE_THREAD_LIBS_INIT "-lpthread")
	set(CMAKE_HAVE_THREADS_LIBRARY 1)
	set(CMAKE_USE_WIN32_THREADS_INIT 0)
	set(CMAKE_USE_PTHREADS_INIT 1)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	set(TELEPORT_UNIX ON)
	set(TELEPORT_LINUX ON)
	# fix linkage for lots of libraries:
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fPIC)
	include_directories(${CMAKE_SOURCE_DIR}/firstparty/Platform/Linux)
endif()

if(TELEPORT_MACOS)
	set(CMAKE_THREAD_LIBS_INIT "-lpthread")
	set(CMAKE_HAVE_THREADS_LIBRARY 1)
	set(CMAKE_USE_WIN32_THREADS_INIT 0)
	set(CMAKE_USE_PTHREADS_INIT 1)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fPIC)
	# Apple Silicon only. A universal binary would require every dependency
	# (OpenSSL, libdatachannel, usrsctp, juice, draco) to be built for both slices
	# and lipo'd together; not worth it while macOS support is headless-only.
	set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "")
	# ThisPlatform/{StringFunctions,Threads}.h - the macOS counterpart of the Linux shim.
	include_directories(${CMAKE_SOURCE_DIR}/firstparty/Platform/MacOS)
endif()

# Runtime library search path for the *installed* binaries. The packages bundle their own
# copies of libktx, libdraco, libozz_*, libcurl and libopus under <prefix>/lib, and CMake
# strips the build-tree rpath when installing, so without this the installed client dies at
# startup with "libktx.so.4: cannot open shared object file: No such file or directory".
#   $ORIGIN/../lib — executables in <prefix>/bin finding the bundled libraries
#   $ORIGIN        — bundled libraries finding each other (ozz-animation is three .so files)
# This has to be set before any dependency is fetched or added: a target's INSTALL_RPATH is
# initialised from this variable at target-creation time, so it does not reach targets that
# already exist. Deliberately not CMAKE_INSTALL_RPATH_USE_LINK_PATH, which would bake the
# absolute build-tree directories of the linked libraries into the shipped binaries.
if(TELEPORT_MACOS)
	set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib")
elseif(TELEPORT_LINUX)
	set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib")
endif()

# Default install() component for dependencies that don't declare one of their own. CPack's
# productbuild (macOS) generator treats CPACK_COMPONENTS_ALL as a filter: an install() rule
# with no explicit COMPONENT falls into the synthetic "Unspecified" component, which isn't
# "client", so it is silently dropped from the .pkg. libcurl (pulled in by libavstream's
# FetchContent'd curl), libktx, libdraco and ozz-animation all install themselves via their
# own upstream CMakeLists, none of which tag COMPONENT client — so without this they vanish
# from the macOS installer while still working by accident on Linux/Windows, where the
# DEB/NSIS generators package the whole install tree regardless of component. Must be set
# before those dependencies are fetched, same as CMAKE_INSTALL_RPATH above.
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "client")

# FetchContent download timestamp policy:
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)

set(PLATFORM_USE_FMT ON)
# CACHE (without FORCE) only seeds the default when the variable isn't already defined, so a
# command-line -DPLATFORM_SUPPORT_D3D12=... (as build_pc_client.yml passes) still wins. A plain
# set() here would silently stomp that override on every configure, which used to leave the
# DirectX12 platform library configured (and install()-ed by CPack) even when the CI workflow
# explicitly disabled it and nothing links it — the narrow MSBuild target list then never built
# it, so packaging failed with "file INSTALL cannot find ... PlatformDirectX12_MT.lib".
set(PLATFORM_SUPPORT_D3D12 ${TELEPORT_WINDOWS} CACHE BOOL "Support Direct3D 12 in Platform")
set(PLATFORM_USE_ASSIMP OFF CACHE BOOL "" FORCE)
set(PLATFORM_BUILD_ASSIMP OFF CACHE BOOL "" FORCE)
set(PLATFORM_SUPPORT_D3D11 OFF)
set(PLATFORM_SUPPORT_OPENGL OFF)
set(PLATFORM_BUILD_MD_LIBS OFF)
set(TELEPORT_SOLUTION_NAME "" CACHE STRING "")
option(TELEPORT_SERVER_SUPPORT_WEBRTC "Should the server support WebRTC connections?" ON)
set(TELEPORT_LIBWEBRTC_BUILD_DIR "" CACHE PATH "WebRTC Build path")
set(TELEPORT_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/firstparty/Platform" )
set(SIMUL_PLATFORM_FOLDER_PREFIX "Platform" )
set(SIMUL_PLATFORM_DIR ${TELEPORT_PLATFORM_DIR})
set(PLATFORM_IMGUI ON CACHE BOOL "" FORCE)
option(TELEPORT_TEST "Enable Teleport unit testing?" OFF)
option(TELEPORT_UNITY "Build for Unity?" ${TELEPORT_UNITY})
option(TELEPORT_BUILD_DOCS "Build documentation?" OFF)
option(TELEPORT_INTERNAL_CHECKS "Internal checks for development?" OFF)
option(TELEPORT_TRACY_PROFILING "Use Tracy to profile Teleport SDK?" OFF)
option(TELEPORT_USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime? Only for server builds." OFF)
option(TELEPORT_ASAN "Enable AddressSanitizer?" OFF)
option(TELEPORT_ANDROID_MINIMAL_SETUP "Only fetch dependencies for the Android AGDE build; do not build the client." OFF)
if(TELEPORT_ANDROID_MINIMAL_SETUP)
	message(STATUS "Teleport Android minimal setup enabled: dependencies will be fetched, but the client/platform targets will not be built.")
endif()
if(TELEPORT_USE_DYNAMIC_RUNTIME)
	set(TELEPORT_RUNTIME dynamic)
else()
	set(TELEPORT_RUNTIME static)
endif()
Include(FetchContent)

# Single source of truth for the ozz-animation version, used by both the full client
# build and the Android minimal setup below.
set(TELEPORT_OZZ_GIT_TAG 0.16.0)
# Single source of truth for the libopus version, likewise shared.
set(TELEPORT_OPUS_GIT_TAG v1.5.2)

if(TELEPORT_ANDROID_MINIMAL_SETUP)
	# Android AGDE minimal setup: fetch ozz-animation and generate its static-library
	# project, then stop — nothing else in this file is configured. Intended to be run
	# with the AGDE toolchain (CMake/AGDE.toolchain.cmake) and -B build_android_vs, so
	# that the generated thirdparty/ozz_animation/ozz_animation.vcxproj sits alongside
	# the hand-rolled projects and is referenced by TeleportVRQuestClient.sln.
	if(CMAKE_GENERATOR MATCHES "^Visual Studio")
		# The hand-rolled solution only has Debug and Release configurations.
		set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
		# No ZERO_CHECK: the generated project must build inside the hand-rolled
		# solution without referencing CMake's regeneration machinery.
		set(CMAKE_SUPPRESS_REGENERATION ON)
	endif()
	# Pin the project GUID so the hand-rolled TeleportVRQuestClient.sln and the
	# ProjectReferences in the hand-rolled vcxproj files can refer to the generated
	# project by a stable GUID on every machine.
	set(ozz_animation_GUID_CMAKE "8C9ECEE0-7B1A-4E9F-9B7E-4D2A1F7C6B3A" CACHE INTERNAL "Pinned project GUID for ozz_animation")
	# Fetch only — ozz's own build system is not used; thirdparty/ozz_animation
	# compiles the fetched sources as a single static library.
	if(POLICY CMP0169)
		cmake_policy(SET CMP0169 OLD)
	endif()
	FetchContent_Declare(
	  ozz-animation
	  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
		GIT_SHALLOW 1
	  GIT_TAG        ${TELEPORT_OZZ_GIT_TAG}
	  SOURCE_DIR ozz-animation
	)
	FetchContent_Populate(ozz-animation)
	add_subdirectory(thirdparty/ozz_animation)

	# Oboe audio library, built from source to avoid LFS-tracked prebuilt .a.
	set(oboe_audio_GUID_CMAKE "A1B2C3D4-E5F6-4A5B-9C8D-7E6F5A4B3C2D" CACHE INTERNAL "Pinned project GUID for oboe_audio")
	FetchContent_Declare(
	  oboe-audio
	  GIT_REPOSITORY https://github.com/google/oboe.git
	  GIT_SHALLOW 1
	  GIT_TAG 1.9.0
	  SOURCE_DIR oboe-audio
	)
	FetchContent_Populate(oboe-audio)
	add_subdirectory(thirdparty/oboe_audio)

	# libopus, needed by TeleportAudio's SpatialAudioMixer and libavstream's Opus
	# encoder/decoder on Android. Fetch only — thirdparty/opus_android compiles the
	# fetched sources as a single static library, as for ozz/oboe above.
	set(opus_GUID_CMAKE "C3D4E5F6-A7B8-49C0-91D2-E3F4A5B6C7D8" CACHE INTERNAL "Pinned project GUID for opus")
	FetchContent_Declare(
	  opus
	  GIT_REPOSITORY https://github.com/xiph/opus.git
	  GIT_SHALLOW 1
	  GIT_TAG ${TELEPORT_OPUS_GIT_TAG}
	  SOURCE_DIR opus
	)
	FetchContent_Populate(opus)
	add_subdirectory(thirdparty/opus_android)

	# OpenXR loader for the TeleportVRClient link step. The copies formerly stored in
	# git LFS are not fetchable when the LFS budget is exhausted, so take the loader
	# from the same Khronos Maven artefact that the Gradle packaging step uses (keep
	# the version in sync with AndroidPackaging/app/build.gradle), and place it where
	# the hand-rolled vcxproj expects it.
	set(TELEPORT_OPENXR_LOADER_VERSION 1.0.33)
	set(_oxr_aar ${CMAKE_BINARY_DIR}/openxr_loader_for_android-${TELEPORT_OPENXR_LOADER_VERSION}.aar)
	file(DOWNLOAD
		https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/${TELEPORT_OPENXR_LOADER_VERSION}/openxr_loader_for_android-${TELEPORT_OPENXR_LOADER_VERSION}.aar
		${_oxr_aar}
		EXPECTED_HASH SHA256=ff815aa7e5418bf8488577029ded65fb113bc38d0e574e972039f5aac2f4a0f8
		SHOW_PROGRESS)
	if(NOT DEFINED TELEPORT_AGDE_ABI)
		set(TELEPORT_AGDE_ABI arm64-v8a)
	endif()
	set(_oxr_so prefab/modules/openxr_loader/libs/android.${TELEPORT_AGDE_ABI}/libopenxr_loader.so)
	file(ARCHIVE_EXTRACT INPUT ${_oxr_aar} DESTINATION ${CMAKE_BINARY_DIR}/openxr_loader_aar PATTERNS ${_oxr_so})
	foreach(_oxr_config Debug Release)
		file(COPY ${CMAKE_BINARY_DIR}/openxr_loader_aar/${_oxr_so}
			DESTINATION ${CMAKE_SOURCE_DIR}/client/OpenXR/Libs/Android/${TELEPORT_AGDE_ABI}/${_oxr_config})
	endforeach()
	return()
endif()

macro(get_all_targets_recursive targets dir)
	get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
	foreach(subdir ${subdirectories})
		get_all_targets_recursive(${targets} ${subdir})
	endforeach()
	get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
	list(APPEND ${targets} ${current_targets})
endmacro()

# Get nlohmann json

#FetchContent_Declare(  nlohmann_json  GIT_REPOSITORY https://github.com/nlohmann/json.git	GIT_SHALLOW   GIT_TAG        v3.11.2  SOURCE_DIR nlohmann_json)
#FetchContent_MakeAvailable(nlohmann_json)
# Get the Khronos ktx2 repo
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD Off CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS Off CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS Off CACHE BOOL "" FORCE)
set(KTX_FEATURE_Vulkan On CACHE BOOL "" FORCE)
FetchContent_Declare(
  ktx
  GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
	GIT_SHALLOW 1
  GIT_TAG        v5.0.0-rc1
  SOURCE_DIR ktx
)
FetchContent_MakeAvailable(ktx)

function(set_target_runtime targname rt)
	if(TELEPORT_WINDOWS)
		# Default to statically-linked runtime.
		if("${rt}" STREQUAL "")
			set(rt "static")
		endif()
		if(${rt} STREQUAL "static")
			SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MT$<$<CONFIG:Debug>:d>")
		else()	
			SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MD$<$<CONFIG:Debug>:d>")
		endif()
	endif()
endfunction()

# Apply the project's runtime to every compilable target in a fetched subproject's directory tree.
# KTX v5 splits what v4 compiled into the ktx target itself into separate libraries
# (basisu_encoder, dfdutils, astcenc-*). Those are created in directories whose
# cmake_minimum_required is recent enough that CMP0091 is NEW, so each defaults to /MD, while ktx
# itself is forced to /MT here - a RuntimeLibrary mismatch as soon as ktx.dll links them. Note that
# the /MT in CMAKE_CXX_FLAGS_RELEASE is set further down this file, i.e. after the KTX tree has
# already been added, so it never reaches these targets either. Naming targets one at a time (as we
# did for ktx, ktx_read and astcenc-avx2-static) misses whatever the next KTX release adds, so
# apply the runtime to the whole tree instead. Both the MSVC_RUNTIME_LIBRARY property and the
# compile option are set: the property is the mechanism CMP0091 NEW scopes honour, the flag covers
# any scope where the policy is still OLD. They agree, so no "overriding /MD with /MT" warning.
function(RecurseApplyRuntime dir rt)
	if(NOT TELEPORT_WINDOWS)
		return()
	endif()
	# Default to statically-linked runtime.
	if("${rt}" STREQUAL "")
		set(rt "static")
	endif()
	if(${rt} STREQUAL "static")
		set(runtime_lib "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		set(runtime_lib "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
	set(targets)
	get_all_targets_recursive(targets ${dir})
	foreach(targ ${targets})
		# INTERFACE libraries reject non-whitelisted properties, and UTILITY targets have nothing to compile.
		get_target_property(targ_type ${targ} TYPE)
		if(targ_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
			set_property(TARGET ${targ} PROPERTY MSVC_RUNTIME_LIBRARY "${runtime_lib}")
			set_target_runtime(${targ} ${rt})
		endif()
	endforeach()
endfunction()

RecurseApplyRuntime(${CMAKE_BINARY_DIR}/ktx ${TELEPORT_RUNTIME})

function(RecurseApplyFolder dir dest)
	set(targets)
	get_all_targets_recursive(targets ${dir})
	foreach(targ ${targets})
		set_target_properties( ${targ} PROPERTIES FOLDER ${dest})
	endforeach()
endfunction()

RecurseApplyFolder(${CMAKE_BINARY_DIR}/ktx thirdparty/ktx)

# Get the "safe" header library for lockable resources
FetchContent_Declare(
  safe
  GIT_REPOSITORY https://github.com/LouisCharlesC/safe.git
	GIT_SHALLOW 1
  GIT_TAG        v1.1.0
  SOURCE_DIR safe
)
FetchContent_MakeAvailable(safe)

# Get cpp-httplib (header-only HTTP library, teleportxr fork)
FetchContent_Declare(
  cpp-httplib
  GIT_REPOSITORY https://github.com/teleportxr/cpp-httplib.git
  GIT_TAG        d4d4c9599c0df978b9ad4d1c2425ed26a9c12058
  SOURCE_DIR cpp-httplib
)
# On Windows, cpp-httplib's own CMakeLists.txt installs its CMake package
# config files to "${CMAKE_INSTALL_PREFIX}/cmake/httplib", baking the
# absolute prefix into the install DESTINATION. CPack's NSIS component
# install forbids absolute DESTINATION paths, so packaging fails even
# though the compile succeeds. We only ever use cpp-httplib as a
# header-only include (see TeleportClient/TeleportServer CMakeLists), so
# shadow CMAKE_INSTALL_PREFIX with a relative value just for this fetch to
# keep its computed destination relative.
set(_cpp_httplib_saved_install_prefix ${CMAKE_INSTALL_PREFIX})
set(CMAKE_INSTALL_PREFIX ".")
FetchContent_MakeAvailable(cpp-httplib)
set(CMAKE_INSTALL_PREFIX ${_cpp_httplib_saved_install_prefix})
unset(_cpp_httplib_saved_install_prefix)

# Get libopus (audio codec for WebRTC media transport - see docs/protocol/audio.rst)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  opus
  GIT_REPOSITORY https://github.com/xiph/opus.git
  GIT_SHALLOW 1
  GIT_TAG        ${TELEPORT_OPUS_GIT_TAG}
  SOURCE_DIR opus
)
FetchContent_MakeAvailable(opus)
if(TARGET opus)
	set_target_properties(opus PROPERTIES FOLDER thirdparty)
endif()

# Get websocketpp (source-only fetch; not built by Teleport)
FetchContent_Declare(
  websocketpp
  GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
  GIT_TAG        1b11fd301531e6df35a6107c1e8665b1e77a2d8e
  SOURCE_DIR websocketpp
)
FetchContent_GetProperties(websocketpp)
if(NOT websocketpp_POPULATED)
  FetchContent_Populate(websocketpp)
endif()

# Get zlib (source-only fetch; not built by Teleport)
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG        cacf7f1d4e3d44d871b605da3b647f07d718623f
  SOURCE_DIR zlib
)
FetchContent_GetProperties(zlib)
if(NOT zlib_POPULATED)
  FetchContent_Populate(zlib)
endif()


if(TELEPORT_TEST)
	FetchContent_Declare(
	  Catch2
	  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_SHALLOW 1
	  GIT_TAG        v3.4.0 # or a later release
	  SOURCE_DIR catch2
	)
	#Replace variables inside @@ with the current values
	configure_file(${Catch2_SOURCE_DIR}/CMake/Catch2Config.cmake.in ${Catch2_SOURCE_DIR}/extras/Catch2Config.cmake @ONLY)
	list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
	set( Catch2_DIR ${Catch2_SOURCE_DIR}/extras)
	FetchContent_MakeAvailable(Catch2)
endif()

if(TELEPORT_TRACY_PROFILING)
	FetchContent_Declare (
		tracy
		GIT_REPOSITORY https://github.com/wolfpld/tracy.git
		GIT_TAG v0.10
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
	)
	FetchContent_MakeAvailable(tracy)
endif()
if(TELEPORT_UNITY)
	set(TELEPORT_SERVER true)
else()
	option(TELEPORT_SERVER "Build for server?" ${TELEPORT_SERVER})
endif()
if(TELEPORT_SERVER)
#E:\Code\Teleport\TeleportUnity\teleport_build\_deps\freetype-build\Release
	set(FREETYPE_LIBRARY "${CMAKE_BINARY_DIR}/_deps/freetype-build/Release/freetype.lib" CACHE STRING "" FORCE)
	set(FREETYPE_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/freetype/include" CACHE STRING "" FORCE)
	set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
	set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)

	set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(
	  freetype
	  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
		GIT_SHALLOW 1
	  GIT_TAG        VER-2-13-3
	  SOURCE_DIR freetype
	)
	FetchContent_MakeAvailable(freetype)
	
	# Don't look for TinyXML library:
	set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
	set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
	set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
	 
	# Only need to generate MSDF fonts server-side.
	FetchContent_Declare(
	  msdfgen
	  GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
		GIT_SHALLOW 1
	  GIT_TAG        v1.12
	  SOURCE_DIR msdfgen
	)
	FetchContent_MakeAvailable(msdfgen)

	
	set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
	set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
	set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
	set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
	set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON CACHE BOOL "" FORCE)

	# Only need to generate MSDF fonts server-side.
	FetchContent_Declare(
	  msdf-atlas-gen
	  GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
		GIT_SHALLOW 1
	  GIT_TAG        v1.3
	  SOURCE_DIR msdf-atlas-gen
	)
	FetchContent_MakeAvailable(msdf-atlas-gen)
	RecurseApplyFolder(${CMAKE_BINARY_DIR}/freetype thirdparty/freetype)
	RecurseApplyFolder(${CMAKE_BINARY_DIR}/msdf-atlas-gen thirdparty/msdf)
	RecurseApplyFolder(${CMAKE_BINARY_DIR}/msdfgen thirdparty/msdf)
	
	set(OPTION_ENABLE_ALL_APPS OFF CACHE BOOL "" FORCE)
	set(OPTION_BUILD_BROTLIG OFF CACHE BOOL "" FORCE)
	set(OPTION_BUILD_GUI OFF CACHE BOOL "" FORCE)
	set(OPTION_CMP_DIRECTX OFF CACHE BOOL "" FORCE)
	# On Windows we use the prebuilt Compressonator .lib files in thirdparty/compressonator/lib/bin/x64/.
	# Those are PE import libraries and cannot link on Linux, so fetch the upstream source and add only the
	# CMP_Core + CMP_Framework + CMP_Compressonator subdirectories directly — running the upstream root
	# CMakeLists drags in Qt/Vulkan/etc. CMP_Compressonator provides the high-level CMP_ConvertTexture API
	# used by TeleportServer/Texture.cpp (CMP_Framework alone does not define it).
	if(NOT WIN32)
		FetchContent_Declare(
			compressonator
			GIT_REPOSITORY https://github.com/GPUOpen-Tools/compressonator.git
			GIT_TAG        V4.5.52
			GIT_SHALLOW    TRUE
			SOURCE_DIR     compressonator-src
		)
		FetchContent_GetProperties(compressonator)
		if(NOT compressonator_POPULATED)
			FetchContent_Populate(compressonator)
		endif()
		# Upstream V4.5.52 bug: cmp_compressonatorlib/dxtc/codec_dxtc_alpha.cpp defines a file-local
		# static cmp_getBlockPackedIndicesSNorm() that collides with an identically-named function
		# pulled in from CMP_Core's bcn_common_kernel.h (USE_CMP_CORE_API is set in common.h). MSVC
		# tolerates the duplicate; clang reports an ambiguous call. Rename the file-local copy (and its
		# single call site) so the call binds to it, preserving the legacy behaviour the alpha codec was
		# written against. Idempotent, so it is safe to re-run on every configure.
		set(_cmp_alpha_src ${compressonator_SOURCE_DIR}/cmp_compressonatorlib/dxtc/codec_dxtc_alpha.cpp)
		if(EXISTS ${_cmp_alpha_src})
			file(READ ${_cmp_alpha_src} _cmp_alpha_contents)
			if(NOT _cmp_alpha_contents MATCHES "cmp_getBlockPackedIndicesSNorm_legacy")
				string(REPLACE "cmp_getBlockPackedIndicesSNorm" "cmp_getBlockPackedIndicesSNorm_legacy" _cmp_alpha_contents "${_cmp_alpha_contents}")
				file(WRITE ${_cmp_alpha_src} "${_cmp_alpha_contents}")
				message(STATUS "Patched compressonator codec_dxtc_alpha.cpp for clang overload ambiguity")
			endif()
		endif()
		set(PROJECT_FOLDER_SDK_LIBS "thirdparty/compressonator")
		# cmp_framework references ${PROJECT_SOURCE_DIR}/applications/... — override PROJECT_SOURCE_DIR
		# so those paths resolve under the fetched compressonator source instead of the Teleport root.
		set(_teleport_project_source_dir_saved ${PROJECT_SOURCE_DIR})
		set(PROJECT_SOURCE_DIR ${compressonator_SOURCE_DIR})
		add_subdirectory(${compressonator_SOURCE_DIR}/cmp_core ${compressonator_BINARY_DIR}/cmp_core EXCLUDE_FROM_ALL)
		add_subdirectory(${compressonator_SOURCE_DIR}/cmp_framework ${compressonator_BINARY_DIR}/cmp_framework EXCLUDE_FROM_ALL)
		add_subdirectory(${compressonator_SOURCE_DIR}/cmp_compressonatorlib ${compressonator_BINARY_DIR}/cmp_compressonatorlib EXCLUDE_FROM_ALL)
		set(PROJECT_SOURCE_DIR ${_teleport_project_source_dir_saved})
	endif()
endif()

option(TELEPORT_CLIENT "Build for client?" ON)
option(TELEPORT_HEADLESS_CLIENT "Build headless terminal client?" ${TELEPORT_CLIENT})
# The GUI client (pc_client/ClientRender) needs Vulkan and GLFW, both of which now build on
# macOS via MoltenVK (see firstparty/Platform/CMake/Variables.cmake). It also links an OpenXR
# loader, but there is still no OpenXR *runtime* for macOS - ENABLE_VR defaults off there
# (TeleportClient/Config.h) so the client never tries to create a real XR session.
option(TELEPORT_GUI_CLIENT "Build the GUI/OpenXR PC client?" ${TELEPORT_CLIENT})
if(NOT TELEPORT_CLIENT)
	set(TELEPORT_GUI_CLIENT OFF CACHE BOOL "" FORCE)
endif()

if(TELEPORT_CLIENT)
	if("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
		option(TELEPORT_CLIENT_USE_D3D12 "Use D3D12?" OFF)
	else()
		# On macOS this builds PlatformVulkan against MoltenVK (Platform submodule's
		# firstparty/Platform/CMake/Variables.cmake and CMakeLists.txt now add a PLATFORM_MACOS
		# branch alongside PLATFORM_WINDOWS/PLATFORM_LINUX). Requires the Vulkan SDK for macOS
		# (bundles MoltenVK + glslangValidator) to be installed.
		option(TELEPORT_CLIENT_USE_VULKAN "Use Vulkan?" ON)
	endif()
	if( ${TELEPORT_CLIENT_USE_D3D12})
		set(TELEPORT_CLIENT_USE_D3D11 OFF)
	endif()
	if( ${TELEPORT_CLIENT_USE_VULKAN})
		set(TELEPORT_CLIENT_USE_D3D12 OFF)
		set(TELEPORT_CLIENT_USE_D3D11 OFF)
		find_package(Vulkan)
	endif()
	option(TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER "Use Platform video decoder? Currently requires TELEPORT_CLIENT_USE_D3D12" OFF)
	if(${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER})
		if(NOT ${TELEPORT_CLIENT_USE_D3D12})
			message("Warning: Setting TELEPORT_CLIENT_USE_D3D12 true because TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER is true.")
			set(TELEPORT_CLIENT_USE_D3D12 ON)
		endif()
	endif()
endif()

set(TELEPORT_SERVER_DLL_DIR "${TELEPORT_UNITY_PLUGINS_DIR}" CACHE PATH "Location to build TeleportServer.dll for use by engine")
set(TELEPORT_SERVER_INCLUDE_DIR "${TELEPORT_SERVER_DLL_DIR}/../../include" CACHE PATH "Location to deploy exported headers for use by engine")

if(TELEPORT_UNITY)
	get_filename_component(PROJECT_DIR ${CMAKE_BINARY_DIR} DIRECTORY)
	set(TELEPORT_UNITY_PROJECT_DIR "${PROJECT_DIR}" CACHE PATH "")
	set(TELEPORT_UNITY_EDITOR_DIR "" CACHE PATH "")
	set(TELEPORT_UNITY_EDITOR_EXE "${TELEPORT_UNITY_EDITOR_DIR}/Unity.exe" CACHE PATH "Unity Editor or game to run when debugging")
endif()

set(CONFIG_NAME $<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>)


option(ENABLE_APPS "" OFF)
option(ENABLE_SHARED "" OFF) 
option(ENABLE_STATIC "" ON)

if(TELEPORT_WINDOWS)
	if(TELEPORT_USE_DYNAMIC_RUNTIME)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DNOMINMAX")
	else()
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /Zi /DNDEBUG /DNOMINMAX /MT /Zc:__cplusplus")
		set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /DNOMINMAX /MTd /Zc:__cplusplus")
		set(CMAKE_C_FLAGS_RELEASE "/MT  /O2 /Ob2 /DNDEBUG /DNOMINMAX")
		set(CMAKE_C_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /DNOMINMAX")
	endif()
	#someday we will add /fsanitize=address - when MS have fixed all the address bugs in their core libs.)
	# linker flags:

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
endif()

#openssl
if(TELEPORT_WINDOWS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS")
	set(OPENSSL_APPLINK_SOURCE "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include/openssl/applink.c" CACHE STRING "" )
	# Force use of the bundled static OpenSSL 3.0.8 (thirdparty/openssl) for every client
	# component, including libdatachannel, which is added below. libdatachannel calls
	# find_package(OpenSSL), and without these hints CMake's FindOpenSSL picks up the CI
	# runner's system OpenSSL in C:\Program Files\OpenSSL (dynamic /MD). That pairs the
	# bundled static libssl with an incompatible libcrypto, so the internal WPACKET_*
	# symbols libssl relies on go unresolved at link time. Pre-seeding the modern
	# FindOpenSSL result variables with the exact bundled .lib paths keeps everything on
	# one matched, static /MT OpenSSL. (The legacy SSL_EAY_*/LIB_EAY_* variables used here
	# previously are ignored by CMake >= 3.x FindOpenSSL.)
	set(OPENSSL_ROOT_DIR ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64 CACHE PATH "" FORCE)
	set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(OPENSSL_MSVC_STATIC_RT ON CACHE BOOL "" FORCE)
	set(OPENSSL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include CACHE PATH "" FORCE)
	set(OPENSSL_SSL_LIBRARY ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib/libssl.lib CACHE FILEPATH "" FORCE)
	set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib/libcrypto.lib CACHE FILEPATH "" FORCE)
	set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} CACHE STRING "" FORCE)
	message("OPENSSL_CRYPTO_LIBRARY ${OPENSSL_CRYPTO_LIBRARY}")
	# The seeding above is not sufficient on its own: on MSVC, FindOpenSSL derives its
	# imported targets from its own LIB_EAY/SSL_EAY search (the pre-seeded
	# OPENSSL_*_LIBRARY values are only honoured on non-MSVC platforms), and that search
	# can still land on the runner's shared OpenSSL. An import libcrypto.lib then enters
	# the link via OpenSSL::SSL alongside the bundled static archive, and any target
	# that pulls members from the static one gets LNK2005 (BIO_*/SHA256 "already defined
	# in libcrypto.lib(libcrypto-3-x64.dll)"), as happened to teleportd. FindOpenSSL only
	# creates OpenSSL::SSL / OpenSSL::Crypto when they do not already exist, so define
	# them here, pinned to the bundled static libs. ws2_32/crypt32 mirror the interface
	# dependencies the module itself adds for static OpenSSL on Windows.
	if(NOT TARGET OpenSSL::Crypto)
		add_library(OpenSSL::Crypto UNKNOWN IMPORTED GLOBAL)
		set_target_properties(OpenSSL::Crypto PROPERTIES
			IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib/libcrypto.lib"
			INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include"
			INTERFACE_LINK_LIBRARIES "ws2_32;crypt32")
	endif()
	if(NOT TARGET OpenSSL::SSL)
		add_library(OpenSSL::SSL UNKNOWN IMPORTED GLOBAL)
		set_target_properties(OpenSSL::SSL PROPERTIES
			IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib/libssl.lib"
			INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include"
			INTERFACE_LINK_LIBRARIES "OpenSSL::Crypto")
	endif()
	include_directories(${CMAKE_SOURCE_DIR}/firstparty/Platform/Windows)
	#add_subdirectory(thirdparty/srt/submodules/pthread-win32)

	option(TELEPORT_CLIENT_SUPPORT_IPSME "Should IPSME be included?" OFF)
	set(TELEPORT_CLIENT_MOSQUITTO_LIB_DIR "E:/Code/Teleport/IPSME/mosquitto-2.0.18/build/lib" CACHE PATH "Mosquitto install path for IPSME support")
	set(TELEPORT_CLIENT_MOSQUITTO_INCLUDE_DIR "E:/Code/Teleport/IPSME/mosquitto-2.0.18/include" CACHE PATH "Mosquitto install path for IPSME support")
endif()

if(TELEPORT_MACOS)
	# On macOS the bundled thirdparty/openssl/x64 directory only contains Windows import
	# libraries, so use Homebrew's openssl@3 instead. On Apple Silicon the keg lives under
	# /opt/homebrew; on Intel under /usr/local. If the user already passed OPENSSL_ROOT_DIR
	# on the command line, leave it alone.
	if(NOT OPENSSL_ROOT_DIR)
		if(EXISTS /opt/homebrew/opt/openssl)
			set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl CACHE PATH "OpenSSL root directory for macOS" FORCE)
		elseif(EXISTS /usr/local/opt/openssl)
			set(OPENSSL_ROOT_DIR /usr/local/opt/openssl CACHE PATH "OpenSSL root directory for macOS" FORCE)
		endif()
	endif()
	if(OPENSSL_ROOT_DIR)
		message(STATUS "Using Homebrew OpenSSL: ${OPENSSL_ROOT_DIR}")
	endif()
endif()

# This must go before the openssl settings, or it won't generate:
# Settings for libdatachannel:
set(NO_WEBSOCKET OFF)
set(NO_MEDIA OFF)
set(NO_EXAMPLES ON)
option(NO_EXAMPLES "" ON)
set(NO_TESTS ON)
set(sctp_werror OFF)
if(TELEPORT_UNIX)
#following libdatachannel's build instructions for Linux:
	set(USE_GNUTLS OFF)
	set(USE_NICE OFF)
endif()
option(NO_TESTS "" ON)

	# EXCLUDE_FROM_ALL: only datachannel-static is linked (by libavstream); this also
	# skips libdatachannel's install() rules, which would otherwise make CPack demand
	# the never-built libdatachannel.so. Dependency targets still build.
	add_subdirectory(thirdparty/libdatachannel EXCLUDE_FROM_ALL)
 
	set(sctp_werror OFF)
	set_target_properties( datachannel PROPERTIES FOLDER thirdparty/datachannel)
	set_target_properties( datachannel-static PROPERTIES FOLDER thirdparty/datachannel)
	set_target_properties( juice-static PROPERTIES FOLDER thirdparty/datachannel)
	if(TARGET srtp2)
		set_target_properties( srtp2 PROPERTIES FOLDER thirdparty/datachannel)
	endif()
	if(TARGET srtp3)
		set_target_properties( srtp3 PROPERTIES FOLDER thirdparty/datachannel)
	endif()

set_target_properties( usrsctp PROPERTIES FOLDER thirdparty/datachannel)

if(TELEPORT_UNITY)
else()
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()

if(TELEPORT_UNIX)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-nullability-extension -Wno-ignored-qualifiers -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-nullability-extension -Wno-ignored-qualifiers -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value")
	# Either use everywhere or not at all: add_compile_definitions(PRIVATE _GLIBCXX_DEBUG)
endif()

if(TELEPORT_SERVER)
	set(TELEPORT_DATACHANNEL_WEBRTC ON) 
	add_compile_definitions(PRIVATE TELEPORT_DATACHANNEL_WEBRTC=1) 
else()
	set(TELEPORT_DATACHANNEL_WEBRTC ON) 
	add_compile_definitions(PRIVATE TELEPORT_CLIENT=1 TELEPORT_DATACHANNEL_WEBRTC=1) 
endif()

option(BUILD_AS_LIBRARY "" ON) 

function(SetTeleportDefaults targname)
	set_target_properties( ${targname} PROPERTIES PREFIX "" )
	if(MSVC)
		set_property(TARGET ${targname} PROPERTY COMPILE_WARNING_AS_ERROR ON)
	else()
		# TODO: re-enable once pre-existing libavstream/TeleportCore header warnings are fixed.
		set_property(TARGET ${targname} PROPERTY COMPILE_WARNING_AS_ERROR OFF)
	endif()
	add_definitions(-DUNICODE -D_UNICODE -DPLATFORM_UNIX)
	if(TELEPORT_TRACY_PROFILING)
		target_include_directories(${targname} PUBLIC ${CMAKE_BINARY_DIR}/_deps/tracy-src/public)
		target_compile_definitions(${targname} PRIVATE TRACY_ENABLE=1 )
		target_link_libraries(${targname} TracyClient )
	endif()
	if(MSVC)
	# Treat angle-brackets as representing external headers. Minimal warnings for external headers.
		target_compile_options(${targname} PRIVATE /W3 /wd4011 /wd4514 /external:anglebrackets /external:W0)
		target_compile_definitions(${targname} PRIVATE TELEPORT_STDCALL=__stdcall)
	else()
	#TODO: -Wno-deprecated-copy should be removed when possible.
		target_compile_options(${targname} PRIVATE -Werror -Wno-ignored-qualifiers -Wno-gnu-zero-variadic-macro-arguments -Wno-address-of-packed-member -Wno-sign-compare -Wno-parentheses -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value)
		# these options are intended to minimize the necessary changes to build on Linux, and should be removed when possible:
		target_compile_options(${targname} PRIVATE -Wno-missing-field-initializers -Wno-unused-variable -Wno-deprecated-copy)
		# TODO: fix the underlying header issues in libavstream (packed non-POD fields, backslash-newline at EOF) and TeleportCore InputTypes.h, then remove these:
		target_compile_options(${targname} PRIVATE -Wno-attributes -Wno-packed-non-pod -Wno-error)

		# TODO: fix basisu compile on Linux, remove when possible.
		target_compile_options(${targname} PRIVATE -Wno-array-bounds -Wno-reorder-ctor)
		# TODO: This is a fix for errors of the form
		# relocation R_X86_64_32S against `.rodata' can not be used when making a shared object; recompile with -fPIC
		target_compile_options(${targname} PRIVATE -fPIC)
		# dummy for stdcall, PLUS make Clang show debug info for strings and other stl stuff:
		target_compile_definitions(${targname} PRIVATE TELEPORT_STDCALL= )
		if(TELEPORT_ASAN)
			target_compile_options(${targname} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
			target_link_options(${targname} PRIVATE -fsanitize=address)
		endif()
	endif()
	target_compile_definitions(${targname} PRIVATE CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} TELEPORT_SUPPORT_AUDIO=$<BOOL:${TELEPORT_SUPPORT_AUDIO}> CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR} TELEPORT_INTERNAL_CHECKS=$<BOOL:${TELEPORT_INTERNAL_CHECKS}>)
endfunction()

function(add_teleport_static_library targname)
  add_library(${ARGV})
  set_target_runtime(${targname} ${TELEPORT_RUNTIME})
  SetTeleportDefaults( ${targname} )
endfunction()

function(add_teleport_static_executable targname)
	add_executable(${ARGV})
	#message( STATUS set_target_runtime ${targname})
	set_target_runtime(${targname} ${TELEPORT_RUNTIME})
	SetTeleportDefaults( ${targname} )
endfunction()

#set_target_properties(zlib PROPERTIES FOLDER thirdparty/zlib)

add_subdirectory(TeleportCore)

if(TELEPORT_SERVER)
	set(LIBAV_USE_DYNAMIC_RUNTIME ${TELEPORT_USE_DYNAMIC_RUNTIME} CACHE BOOL "" FORCE)
	set(LIBAV_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
	set(SA_BUILD_SHARED_LIB ON CACHE BOOL "" FORCE)
endif()

if(TELEPORT_SERVER )
	add_subdirectory(TeleportServer)
endif()
 
if(TELEPORT_SERVER)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TeleportServer)
endif()
# ozz-animation is used only by ClientRender, so a headless-only build skips the fetch entirely.
if(TELEPORT_GUI_CLIENT)
	# ClientRender only links ozz_animation and ozz_animation_offline (see
	# ClientRender/CMakeLists.txt); it never touches ozz_geometry, ozz_options,
	# ozz_animation_tools, or the gltf2ozz command-line converter. Leaving these ON used to
	# configure those extra targets (and their unconditional install() rules) without CI's
	# narrow MSBuild target list ever actually building them, so CPack failed trying to
	# install .dll/.exe files that were never produced.
	set(ozz_build_tools OFF CACHE BOOL "" FORCE)
	set(ozz_build_gltf OFF CACHE BOOL "" FORCE)
	set(ozz_build_samples OFF CACHE BOOL "" FORCE)
	set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
	set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
	set(ozz_build_tests OFF CACHE BOOL "" FORCE)
	set(ozz_build_msvc_rt_dll OFF CACHE BOOL "" FORCE)
	# For animation playback.
	FetchContent_Declare(
	  ozz-animation
	  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
		GIT_SHALLOW 1
	  GIT_TAG        ${TELEPORT_OZZ_GIT_TAG}
	  SOURCE_DIR ozz-animation
	)
	FetchContent_MakeAvailable(ozz-animation)
	RecurseApplyFolder(${CMAKE_BINARY_DIR}/ozz-animation thirdparty/ozz-animation)
endif()

# Build draco before client components that depend on it
set(DRACO_TRANSCODER_SUPPORTED ON CACHE BOOL "" FORCE)
set(TINYGLTF_NO_STB_IMAGE ON CACHE BOOL "" FORCE)
set(TINYGLTF_NO_STB_IMAGE_WRITE ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/draco)

set(SA_BUILD_SHARED_LIB ${TELEPORT_SERVER})

if(TELEPORT_CLIENT OR TELEPORT_BUILD_DOCS)
	# thirdparty/openssl/x64 holds Windows import libraries only. Pointing OPENSSL_ROOT_DIR
	# at it on macOS defeats the FindOpenSSL hints in TeleportClient/CMakeLists.txt, which
	# are what steer the build onto Homebrew's openssl@3.
	if(NOT TELEPORT_MACOS)
		set(OPENSSL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl/x64)
	endif()
	set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(OPENSSL_MSVC_STATIC_RT ON CACHE BOOL "" FORCE)
	set(PLATFORM_IMPLEMENT_STB_IMAGE OFF CACHE BOOL "" FORCE)
	set(PLATFORM_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
	set(PLATFORM_SUPPORT_D3D12_VIEWINSTANCING ON CACHE BOOL "" FORCE)
	add_subdirectory(firstparty/Platform)
	#message("1 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	set(PLATFORM_BUILD_SAMPLES OFF)
	set(LIBAV_USE_DYNAMIC_RUNTIME OFF CACHE BOOL "")
	set(TELEPORT_USE_DYNAMIC_RUNTIME OFF CACHE BOOL "")
	set(LIBAV_BUILD_SHARED_LIBS OFF CACHE BOOL "")
	set(SA_BUILD_SHARED_LIB OFF CACHE BOOL "")
	# TeleportAudio and libavstream must be built before client components that depend on them.
	# TeleportAudio is PulseAudio-only on POSIX and is not used by the headless client, so a
	# headless-only build (macOS) skips it rather than needing a CoreAudio backend first.
	if(TELEPORT_GUI_CLIENT)
		add_subdirectory(TeleportAudio)
	endif()
	add_subdirectory(libavstream)
	if(TELEPORT_GUI_CLIENT)
		add_subdirectory(pc_client)
	endif()
	if(TELEPORT_HEADLESS_CLIENT)
		add_subdirectory(HeadlessClient)
	endif()
	if(TELEPORT_GUI_CLIENT)
		add_subdirectory(ClientRender)
	endif()
	#message("2 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	add_subdirectory(TeleportClient)
	if(TELEPORT_GUI_CLIENT)
		add_subdirectory(client/JavaScriptWorker)
	endif()
	#message("3 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
endif()

# draco (thirdparty/draco/cmake/draco_install.cmake) and ozz-animation both unconditionally
# install a handful of command-line tool targets (draco_decoder, draco_encoder,
# draco_transcoder, ozz_options, ozz_geometry, and — unless ozz_build_tools/ozz_build_gltf
# above suppress them — ozz_animation_tools/gltf2ozz) that nothing in Teleport actually links.
# The Windows CI workflow (build_pc_client.yml) builds only TeleportPCClient/teleportd
# via a targeted MSBuild invocation rather than the full solution, so these extra targets are
# never compiled — yet CPack still tries to install their output files afterwards and fails
# with "file INSTALL cannot find ... .exe/.dll". Force them to build alongside the client so
# their install rules always have something to install. Guarded with if(TARGET ...) since
# which of these exist depends on upstream options and platform.
if(TARGET TeleportPCClient)
	foreach(_extra_install_only_target draco_decoder draco_encoder draco_transcoder ozz_options ozz_geometry ozz_animation_tools gltf2ozz)
		if(TARGET ${_extra_install_only_target})
			add_dependencies(TeleportPCClient ${_extra_install_only_target})
		endif()
	endforeach()
	unset(_extra_install_only_target)
endif()

if(TELEPORT_CLIENT_USE_D3D12)
	target_compile_definitions(libavstream PRIVATE LIBAV_USE_D3D12=1)
else()
	target_compile_definitions(libavstream PRIVATE LIBAV_USE_D3D12=0)
endif()

# Here we will put all the draco targets into the thirdparty/draco solution folder:
set(targets)
get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/draco)
foreach(targ ${targets})
	set_target_properties( ${targ} PROPERTIES FOLDER thirdparty/draco)
endforeach()

if(TELEPORT_SERVER)
	set(STATIC OFF)
else()
	set(STATIC ON)
endif()
#add_subdirectory(thirdparty/basis_universal)
#dangerous to keep a variable with this kind of name around. It breaks draco, for example. Kill it with fire.
unset(STATIC CACHE)
#if(TELEPORT_UNIX)
#	target_compile_options(basisu PRIVATE -Wno-array-bounds -Wno-reorder-ctor)
#endif()
#set_target_properties( basisu PROPERTIES FOLDER thirdparty/basis_universal)

if(TELEPORT_SERVER)
	add_subdirectory(Utils)
endif()

if(TELEPORT_BUILD_DOCS)
	add_subdirectory ("docs")
endif()
if(TELEPORT_TEST)
add_subdirectory(test)
endif()

# Installer configuration. Must come last: include(CPack), which this performs, has to see
# the final value of every CPACK_* variable, and variables set inside a subdirectory are
# not visible here — so all of them are set in the module itself.
if(TELEPORT_CLIENT)
	include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/TeleportPackaging.cmake)
endif()
