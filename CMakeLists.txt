cmake_minimum_required(VERSION 3.11)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(TELEPORT_VERSION 1.0.0)
if(NOT DEFINED TELEPORT_SOLUTION_NAME OR "${TELEPORT_SOLUTION_NAME}" MATCHES "")
	if(TELEPORT_UNITY)
		project(Teleport_Server_Unity VERSION ${TELEPORT_VERSION})
	elseif(TELEPORT_SERVER)
		project(Teleport_Server VERSION ${TELEPORT_VERSION})
	elseif(TELEPORT_CLIENT)
		project(Teleport_Client VERSION ${TELEPORT_VERSION})
	elseif(TELEPORT_BUILD_DOCS)
		project(Teleport_Docs VERSION ${TELEPORT_VERSION})
		set(VS_STARTUP_PROJECT Sphinx)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sphinx)
	endif()
else()
	project("${TELEPORT_SOLUTION_NAME}" VERSION ${TELEPORT_VERSION})
endif()

# FetchContent download timestamp policy:
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)

set(PLATFORM_USE_FMT ON)
set(PLATFORM_SUPPORT_D3D12 ON)
set(PLATFORM_SUPPORT_D3D11 OFF)
set(PLATFORM_SUPPORT_OPENGL OFF)
set(PLATFORM_BUILD_MD_LIBS OFF)
set(TELEPORT_SOLUTION_NAME "" CACHE STRING "")
option(TELEPORT_SERVER_SUPPORT_WEBRTC "Should the server support WebRTC connections?" ON)
set(TELEPORT_LIBWEBRTC_BUILD_DIR "" CACHE PATH "WebRTC Build path")
set(TELEPORT_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/firstparty/Platform" )
set(SIMUL_PLATFORM_FOLDER_PREFIX "Platform" )
set(SIMUL_PLATFORM_DIR ${TELEPORT_PLATFORM_DIR})
set(PLATFORM_IMGUI ON)
option(TELEPORT_UNITY "Build for Unity?" ${TELEPORT_UNITY})
option(TELEPORT_BUILD_DOCS "Build documentation?" OFF)
option(TELEPORT_INTERNAL_CHECKS "Internal checks for development?" OFF)
option(TELEPORT_TRACY_PROFILING "Use Tracy to profile Teleport SDK?" OFF)
option(TELEPORT_USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime? Only for server builds." OFF)
if(TELEPORT_USE_DYNAMIC_RUNTIME)
	set(TELEPORT_RUNTIME dynamic)
else()
	set(TELEPORT_RUNTIME static)
endif()
Include(FetchContent)

# Get the "safe" header library for lockable resources
FetchContent_Declare(
  safe
  GIT_REPOSITORY https://github.com/LouisCharlesC/safe.git
  GIT_TAG        v1.1.0
  SOURCE_DIR safe
)
FetchContent_MakeAvailable(safe)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

if(TELEPORT_TRACY_PROFILING)
	FetchContent_Declare (
		tracy
		GIT_REPOSITORY https://github.com/wolfpld/tracy.git
		GIT_TAG master
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
	)
	FetchContent_MakeAvailable(tracy)
endif()
if(TELEPORT_UNITY)
	set(TELEPORT_SERVER true)
else()
	option(TELEPORT_SERVER "Build for server? If true, client will not be built." ${TELEPORT_SERVER})
endif()
if(TELEPORT_SERVER)
	set(TELEPORT_CLIENT OFF)
else()
	if(TELEPORT_BUILD_DOCS)
		option(TELEPORT_CLIENT "Build for client?" OFF)
	else()
		option(TELEPORT_CLIENT "Build for client?" ON)
	endif()
endif()
if(TELEPORT_CLIENT)
	option(TELEPORT_CLIENT_USE_D3D12 "Use D3D12?" OFF)
	option(TELEPORT_CLIENT_USE_VULKAN "Use Vulkan?" OFF)
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

set(TELEPORT_UNIX OFF)
set(TELEPORT_WINDOWS OFF)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(TELEPORT_WINDOWS ON)
endif()

set(CMAKE_CXX_STANDARD 17 )
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
	# fix linkage for lots of libraries:
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -fPIC)
	include_directories(${CMAKE_SOURCE_DIR}/firstparty/Platform/Linux)
endif()

if(TELEPORT_UNITY)
	get_filename_component(PROJECT_DIR ${CMAKE_BINARY_DIR} DIRECTORY)
	set(TELEPORT_UNITY_PLUGINS_DIR "${PROJECT_DIR}/Assets/Teleport/Plugins/x86_64" CACHE PATH "Location to build TeleportServer.dll for use by Unity")
	set(TELEPORT_UNITY_PROJECT_DIR "${PROJECT_DIR}" CACHE PATH "")
	set(TELEPORT_UNITY_EDITOR_DIR "" CACHE PATH "")
	set(TELEPORT_UNITY_EDITOR_EXE "${TELEPORT_UNITY_EDITOR_DIR}/Unity.exe" CACHE PATH "Unity Editor or game to run when debugging")
endif()

set(CONFIG_NAME $<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>)


#defaults for srt:
if(TELEPORT_WINDOWS)
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
	SET(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
	set(PTHREAD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/srt/submodules/pthread-win32)
	set(PTHREAD_LIBRARY ${CMAKE_BINARY_DIR}/thirdparty/srt/submodules/pthread-win32/Release/pthreadVC3.lib)
endif()

option(ENABLE_APPS "" OFF)
option(ENABLE_SHARED "" OFF) 
option(ENABLE_STATIC "" ON)

if(TELEPORT_WINDOWS)
	if(TELEPORT_USE_DYNAMIC_RUNTIME)
	else()
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /Zi /DNDEBUG /DNOMINMAX /MT")
		set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /DNOMINMAX /MTd")
		set(CMAKE_C_FLAGS_RELEASE "/MT  /O2 /Ob2 /DNDEBUG /DNOMINMAX")
		set(CMAKE_C_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /DNOMINMAX")
	endif()
	#someday we will add /fsanitize=address - when MS have fixed all the address bugs in their core libs.)
endif()

#openssl
if(TELEPORT_WINDOWS)
	set(OPENSSL_APPLINK_SOURCE "${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include/openssl/applink.c" CACHE STRING "" )
	set(OPENSSL_ROOT_DIR ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64 CACHE STRING "" )
	set(OPENSSL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/include CACHE STRING "" )
	set(OPENSSL_LIBRARIES ${CMAKE_SOURCE_DIR}/thirdparty/openssl/x64/lib CACHE STRING "" )
	set(OPENSSL_CRYPTO_LIBRARY libcrypto.lib CACHE STRING "" )
	set(SSL_EAY_DEBUG ${OPENSSL_LIBRARIES}/libssl.lib CACHE STRING "" )
	set(SSL_EAY_RELEASE ${OPENSSL_LIBRARIES}/libssl.lib CACHE STRING "" )
	set(SSL_EAY_LIBRARY_DEBUG ${OPENSSL_LIBRARIES}/libssl.lib CACHE STRING "" )
	set(SSL_EAY_LIBRARY_RELEASE ${OPENSSL_LIBRARIES}/libssl.lib CACHE STRING "" )
	set(LIB_EAY_DEBUG ${OPENSSL_LIBRARIES}/libcrypto.lib CACHE STRING "" )
	set(LIB_EAY_RELEASE ${OPENSSL_LIBRARIES}/libcrypto.lib CACHE STRING "" )
	message("OPENSSL_CRYPTO_LIBRARY ${OPENSSL_CRYPTO_LIBRARY}")
	include_directories(${CMAKE_SOURCE_DIR}/firstparty/Platform/Windows)
	add_subdirectory(thirdparty/srt/submodules/pthread-win32)

	option(TELEPORT_CLIENT_SUPPORT_IPSME "Should IPSME be included?" OFF)
	set(TELEPORT_CLIENT_MOSQUITTO_LIB_DIR "E:/Code/Teleport/IPSME/mosquitto-2.0.18/build/lib" CACHE PATH "Mosquitto install path for IPSME support")
	set(TELEPORT_CLIENT_MOSQUITTO_INCLUDE_DIR "E:/Code/Teleport/IPSME/mosquitto-2.0.18/include" CACHE PATH "Mosquitto install path for IPSME support")
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

	add_subdirectory(thirdparty/libdatachannel)
	set_target_properties(datachannel PROPERTIES EXCLUDE_FROM_ALL ON)
	set_target_properties(datachannel PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
 
	set(sctp_werror OFF)
	set_target_properties( datachannel PROPERTIES FOLDER thirdparty/datachannel)
	set_target_properties( datachannel-static PROPERTIES FOLDER thirdparty/datachannel)
	set_target_properties( juice-static PROPERTIES FOLDER thirdparty/datachannel)
	set_target_properties( srtp2 PROPERTIES FOLDER thirdparty/datachannel)
set_target_properties( usrsctp PROPERTIES FOLDER thirdparty/datachannel)

if(TELEPORT_UNITY)
else()
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
endif()

if(TELEPORT_UNIX)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value")
	# Either use everywhere or not at all: add_compile_definitions(PRIVATE _GLIBCXX_DEBUG)
endif()

#defaults for basisu
if(TELEPORT_SERVER)
	option(STATIC "" OFF)
	set(TELEPORT_DATACHANNEL_WEBRTC ON)
	add_compile_definitions(PRIVATE TELEPORT_DATACHANNEL_WEBRTC=1) 
else()
	option(STATIC "" ON)
	set(TELEPORT_DATACHANNEL_WEBRTC ON)
	add_compile_definitions(PRIVATE TELEPORT_CLIENT=1 TELEPORT_DATACHANNEL_WEBRTC=1) 
endif()

option(BUILD_AS_LIBRARY "" ON) 

function(set_target_runtime targname rt)
	# Default to statically-linked runtime.
	if("${rt}" STREQUAL "")
		set(rt "static")
	endif()
	#message(STATUS "rt ${rt}")
	if(${rt} STREQUAL "static")
		SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MT$<$<CONFIG:Debug>:d>")
	else()	
		SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MD$<$<CONFIG:Debug>:d>")
	endif()
endfunction()

function(SetTeleportDefaults targname)
	set_target_properties( ${targname} PROPERTIES PREFIX "" )
	set_property(TARGET ${targname} PROPERTY COMPILE_WARNING_AS_ERROR ON)
	add_definitions(-DUNICODE -D_UNICODE)
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
		target_compile_options(${targname} PRIVATE -Wall -Wextra -pedantic -Werror -Wno-gnu-zero-variadic-macro-arguments -Wno-address-of-packed-member -Wno-sign-compare -Wno-parentheses -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-unused-parameter -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-value)
		# these options are intended to minimize the necessary changes to build on Linux, and should be removed when possible:
		target_compile_options(${targname} PRIVATE -Wno-missing-field-initializers -Wno-unused-variable -Wno-deprecated-copy)

		# TODO: fix basisu compile on Linux, remove when possible.
		target_compile_options(${targname} PRIVATE -Wno-array-bounds -Wno-reorder-ctor)
		# TODO: This is a fix for errors of the form
		# relocation R_X86_64_32S against `.rodata' can not be used when making a shared object; recompile with -fPIC
		target_compile_options(${targname} PRIVATE -fPIC)
		# dummy for stdcall, PLUS make Clang show debug info for strings and other stl stuff:
		target_compile_definitions(${targname} PRIVATE TELEPORT_STDCALL= )
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
	# Server doesn't include Platform, so load fmt separately.
	add_subdirectory(firstparty/Platform/External/fmt)
	set(LIBAV_USE_DYNAMIC_RUNTIME ${TELEPORT_USE_DYNAMIC_RUNTIME} CACHE BOOL "" FORCE)
	set(LIBAV_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
	set(SA_BUILD_SHARED_LIB ON CACHE BOOL "" FORCE)
	add_subdirectory(thirdparty/cpp-httplib)
endif()

if(TELEPORT_SERVER OR TELEPORT_BUILD_DOCS)
	add_subdirectory(TeleportServer)
endif()
 
if(TELEPORT_SERVER)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TeleportServer)
endif()

if(TELEPORT_CLIENT OR TELEPORT_BUILD_DOCS)
	set(OPENSSL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openssl/x64)
	set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
	set(OPENSSL_MSVC_STATIC_RT ON CACHE BOOL "" FORCE)
	set(PLATFORM_IMPLEMENT_STB_IMAGE OFF CACHE BOOL "" FORCE)
	set(PLATFORM_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
	set(PLATFORM_SUPPORT_D3D12_VIEWINSTANCING ON CACHE BOOL "" FORCE)
	add_subdirectory(firstparty/Platform)
	#message("1 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	set(PLATFORM_BUILD_SAMPLES OFF)
	add_subdirectory(pc_client)
	add_subdirectory(ClientRender)
	#message("2 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	add_subdirectory(TeleportClient)
	#message("3 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	set(LIBAV_USE_DYNAMIC_RUNTIME OFF CACHE BOOL "")
	set(TELEPORT_USE_DYNAMIC_RUNTIME OFF CACHE BOOL "")
	set(LIBAV_BUILD_SHARED_LIBS OFF CACHE BOOL "")
	set(SA_BUILD_SHARED_LIB OFF CACHE BOOL "")
endif()

set(SA_BUILD_SHARED_LIB ${TELEPORT_SERVER})

add_subdirectory(TeleportAudio)
add_subdirectory(libavstream)

if(TELEPORT_CLIENT_USE_D3D12)
	target_compile_definitions(libavstream PRIVATE LIBAV_USE_D3D12=1)
else()
	target_compile_definitions(libavstream PRIVATE LIBAV_USE_D3D12=0)
endif()

add_subdirectory(thirdparty/basis_universal)
set_target_properties( basisu PROPERTIES FOLDER thirdparty/basis_universal)

if(TELEPORT_UNIX)
	target_compile_options(basisu PRIVATE -Wno-array-bounds -Wno-reorder-ctor)
endif()

set(DRACO_TRANSCODER_SUPPORTED ON CACHE BOOL "" FORCE)
set(TINYGLTF_NO_STB_IMAGE ON CACHE BOOL "" FORCE)
set(TINYGLTF_NO_STB_IMAGE_WRITE ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/draco)

	# Here we will put all the draco targets into the thirdparty/draco solution folder:
	macro(get_all_targets_recursive targets dir)
		get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
		foreach(subdir ${subdirectories})
			get_all_targets_recursive(${targets} ${subdir})
		endforeach()
		get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
		list(APPEND ${targets} ${current_targets})
	endmacro()
	set(targets)
	get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/draco)
	foreach(targ ${targets})
		set_target_properties( ${targ} PROPERTIES FOLDER thirdparty/draco)
		#message(${targ})
	endforeach()


if(LIBAV_USE_SRT)
	add_subdirectory(thirdparty/srt)
	if(TELEPORT_UNIX)
		target_compile_options(srt_virtual PRIVATE -fPIC)
		target_compile_options(srt_static PRIVATE -fPIC)
	endif()

	if(TELEPORT_CLIENT OR TELEPORT_BUILD_DOCS)
		set_target_properties( srt_static PROPERTIES FOLDER thirdparty/srt)
	endif()
	set_target_properties( srt_virtual PROPERTIES FOLDER thirdparty/srt)

endif()

if(TELEPORT_BUILD_DOCS)
	add_subdirectory ("docs")
endif()
add_subdirectory(test)

set(KTX_FEATURE_GL_UPLOAD Off)
set(KTX_FEATURE_STATIC_LIBRARY On)
set(KTX_FEATURE_TESTS Off)
set(KTX_FEATURE_TOOLS Off)
set(KTX_FEATURE_Vulkan On)
