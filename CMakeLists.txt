cmake_minimum_required(VERSION 3.8)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(TELEPORT_VERSION 1.0.0)
if(NOT DEFINED TELEPORT_SOLUTION_NAME OR "${TELEPORT_SOLUTION_NAME}" MATCHES "")
	if(TELEPORT_BUILD_DOCS)
		project(Teleport_Docs VERSION ${TELEPORT_VERSION})
		set(VS_STARTUP_PROJECT Sphinx)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sphinx)
	elseif(TELEPORT_UNITY)
		project(Teleport_Server_Unity VERSION ${TELEPORT_VERSION})
	else()
		if(TELEPORT_SERVER)
			project(Teleport_Server VERSION ${TELEPORT_VERSION})
		else()
			project(Teleport_Client VERSION ${TELEPORT_VERSION})
		endif()
	endif()
else()
	project("${TELEPORT_SOLUTION_NAME}" VERSION ${TELEPORT_VERSION})
endif()
set(PLATFORM_USE_FMT ON)
set(PLATFORM_BUILD_MD_LIBS OFF)
set(TELEPORT_SOLUTION_NAME "" CACHE STRING "")
option(TELEPORT_SERVER_SUPPORT_WEBRTC "Should the server support WebRTC connections from browsers?" OFF)
set(TELEPORT_LIBWEBRTC_BUILD_DIR "" CACHE PATH "WebRTC Build path")
set(TELEPORT_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/firstparty/Platform" )
set(SIMUL_PLATFORM_FOLDER_PREFIX "Platform" )
set(SIMUL_PLATFORM_DIR ${TELEPORT_PLATFORM_DIR})
set(PLATFORM_IMGUI ON)
option(TELEPORT_UNITY "Build for Unity?" ${TELEPORT_UNITY})
option(TELEPORT_BUILD_DOCS "Build documentation?" OFF)
option(TELEPORT_INTERNAL_CHECKS "Internal checks for development?" OFF)
if(TELEPORT_UNITY)
	set(TELEPORT_SERVER true)
	set(TELEPORT_CLIENT false)
else()
	option(TELEPORT_SERVER "Build for server? If true, client will not be built." ${TELEPORT_SERVER})
endif()
if(TELEPORT_SERVER OR TELEPORT_BUILD_DOCS)
	set(TELEPORT_CLIENT OFF)
else()
	set(TELEPORT_CLIENT ON)
	option(TELEPORT_CLIENT_USE_D3D12 "Use D3D12?" ON)
	if( ${TELEPORT_CLIENT_USE_D3D12})
		set(TELEPORT_CLIENT_USE_D3D11 OFF)
	else()
		set(TELEPORT_CLIENT_USE_D3D11 ON)
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
	set(TELEPORT_UNITY_EDITOR_DIR "C:/Program Files/Unity/Editor" CACHE PATH "")
	set(TELEPORT_UNITY_EDITOR_EXE "Unity.exe")
	find_program(TELEPORT_UNITY_EDITOR_EXE Unity.exe PATHS ${TELEPORT_UNITY_EDITOR_DIR})
endif()
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "")

#Override srt encryption to be off.
set(ENABLE_ENCRYPTION OFF CACHE BOOL "Enable encryption in SRT")

#Hide Basis Universal STATIC option, and set to opposite of USE_DYNAMIC_RUNTIME.
if(LIBAV_USE_DYNAMIC_RUNTIME)
	set(STATIC OFF CACHE INTERNAL "Static linking for Basis Universal" FORCE)
else()
	set(STATIC ON CACHE INTERNAL "Static linking for Basis Universal" FORCE)
endif()

set(CONFIG_NAME $<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>)
#message(STATUS "CONFIG_NAME ${CONFIG_NAME}")

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
	set(CMAKE_CXX_FLAGS_RELEASE "/Od /Ob0 /Zi /DNDEBUG /DNOMINMAX /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /DNOMINMAX /MTd")
	set(CMAKE_C_FLAGS_RELEASE "/MT  /O2 /Ob2 /DNDEBUG /DNOMINMAX")
	set(CMAKE_C_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1 /DNOMINMAX")
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
	set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TELEPORT_UNITY_PLUGINS_DIR} )
	message("Outputting to ${TELEPORT_UNITY_PLUGINS_DIR}")
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
	if(MSVC)
	# Treat angle-brackets as representing external headers. Minimal warnings for external headers.
		target_compile_options(${targname} PRIVATE /W3 /wd4011 /wd4514 /external:anglebrackets /external:W0)
		target_compile_definitions(${targname} PRIVATE TELEPORT_STDCALL=__stdcall)
	# attempting to get WebRTC not to crash:
	#target_compile_definitions(${targname} PRIVATE   
	#					  _WEBSOCKETPP_CPP11_INTERNAL_
    #                        #USE_AURA=1
	#						_HAS_EXCEPTIONS=0 __STD_C _CRT_RAND_S
	#						_CRT_SECURE_NO_DEPRECATE
    #                        _SCL_SECURE_NO_DEPRECATE
	#						_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	#						#_ATL_NO_OPENGL _WINDOWS CERT_CHAIN_PARA_HAS_EXTRA_FIELDS
    #                        #PSAPI_VERSION=2 _SECURE_ATL  __WRL_NO_DEFAULT_LIB__
    #                        #WINAPI_FAMILY=WINAPI_FAMILY_PC_APP WIN10=_WIN32_WINNT_WIN10 WIN32_LEAN_AND_MEAN
    #                        #NOMINMAX NTDDI_VERSION=NTDDI_WIN10_RS2 _WIN32_WINNT=0x0A00 WINVER=0x0A00
    #                        #NDEBUG NVALGRIND DYNAMIC_ANNOTATIONS_ENABLED=0
	#						WEBRTC_ENABLE_PROTOBUF=0
    #                        WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE RTC_ENABLE_VP9 HAVE_SCTP
    #                        WEBRTC_LIBRARY_IMPL WEBRTC_NON_STATIC_TRACE_EVENT_HANDLERS=0
    #                        WEBRTC_WIN
	#						ABSL_ALLOCATOR_NOTHROW=1
	#						ABSL_USES_STD_OPTIONAL=0
	#						HAVE_SCTP WEBRTC_VIDEO_CAPTURE_WINRT
	#)
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
  set_target_runtime(${targname} static)
  SetTeleportDefaults( ${targname} )
endfunction()

function(add_teleport_static_executable targname)
	add_executable(${ARGV})
	#message( STATUS set_target_runtime ${targname})
	set_target_runtime(${targname} static)
	SetTeleportDefaults( ${targname} )
endfunction()

#set_target_properties(zlib PROPERTIES FOLDER thirdparty/zlib)

add_subdirectory(TeleportCore)

if(TELEPORT_SERVER)
	# Server doesn't include Platform, so load fmt separately.
		add_subdirectory(firstparty/Platform/External/fmt)
		set(LIBAV_USE_DYNAMIC_RUNTIME ON CACHE BOOL "")
		set(LIBAV_BUILD_SHARED_LIBS ON CACHE BOOL "")
		set(SA_BUILD_SHARED_LIB ON CACHE BOOL "")
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
	set(PLATFORM_SUPPORT_D3D12_VIEWINSTANCING ON CACHE BOOL "" FORCE)
	add_subdirectory(firstparty/Platform)
	#message("1 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	set(PLATFORM_BUILD_SAMPLES OFF)
	add_subdirectory(pc_client)
	add_subdirectory(ClientRender)
	#message("2 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
	add_subdirectory(TeleportClient)
	#message("3 PLATFORM_GRAPHICS_APIS ${PLATFORM_GRAPHICS_APIS}")
		add_subdirectory(client/Shaders/Vulkan)
	set(LIBAV_USE_DYNAMIC_RUNTIME OFF CACHE BOOL "")
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

	add_subdirectory(thirdparty/enet)
	set_target_properties( enet PROPERTIES FOLDER thirdparty/enet)
	add_subdirectory(thirdparty/basis_universal)
	set_target_properties( basisu PROPERTIES FOLDER thirdparty/basis_universal)

	if(TELEPORT_UNIX)
		target_compile_options(enet PRIVATE -fPIC)
		target_compile_options(basisu PRIVATE -Wno-array-bounds -Wno-reorder-ctor)
	endif()

	option(DRACO_TRANSCODER_SUPPORTED  "" ON)
	option(TINYGLTF_NO_STB_IMAGE  "" ON)
	option(TINYGLTF_NO_STB_IMAGE_WRITE  "" ON)
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

	add_subdirectory(thirdparty/srt)
	if(TELEPORT_UNIX)
		target_compile_options(srt_virtual PRIVATE -fPIC)
		target_compile_options(srt_static PRIVATE -fPIC)
	endif()

	if(TELEPORT_CLIENT OR TELEPORT_BUILD_DOCS)
		set_target_properties( srt_static PROPERTIES FOLDER thirdparty/srt)
	endif()
	set_target_properties( srt_virtual PROPERTIES FOLDER thirdparty/srt)


if(TELEPORT_BUILD_DOCS)
	add_subdirectory ("docs")
endif()

set(KTX_FEATURE_GL_UPLOAD Off)
set(KTX_FEATURE_STATIC_LIBRARY On)
set(KTX_FEATURE_TESTS Off)
set(KTX_FEATURE_TOOLS Off)
set(KTX_FEATURE_Vulkan On)
