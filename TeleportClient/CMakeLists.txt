cmake_minimum_required( VERSION 3.8 )

cmake_policy(SET CMP0135 NEW)
project( TeleportClient )
# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)
# We would LIKE to use FetchContent to obtain an appropriate release of the openxr sdk, but it uses the MD runtime, and we don't.
#FetchContent_Declare(
#	OpenXR
#	URL_HASH SHA256=256e9b0af9e492e9836cda8ab5c21be35c372dd4cff4780ecde16d84d2350a13
#	URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.30.zip
#	SOURCE_DIR openxr
#)
#FetchContent_MakeAvailable(OpenXR)
# Build options
set(DEBUG_CONFIGURATIONS Debug)

# Source
set(src_files
	basic_linear_algebra.h
	ClientDeviceState.cpp
	ClientDeviceState.h
	ClientPipeline.cpp
	ClientPipeline.h
	ClientTime.cpp
	ClientTime.h
	Config.h
	Config.cpp
	DiscoveryService.h
	DiscoveryService.cpp
	GeometryCacheBackendInterface.h
	GoogleOAuth.cpp
	GoogleOAuth.h
	Identity.h
	Identity.cpp
	Log.cpp
	Log.h
	OpenXR.cpp
	OpenXR.h
	OpenXRRenderModel.cpp
	OpenXRRenderModel.h
	V8ProcessManager.h
	V8ProcessManager.cpp
	SessionClient.cpp
	SessionClient.h
	SignalingServer.cpp
	SignalingServer.h
	TabContext.cpp
	TabContext.h
	TokenStorage.cpp
	TokenStorage.h
	URLHandlers.h
	URLHandlers.cpp
)
file(GLOB rst_files *.rst )
if(TELEPORT_CLIENT_SUPPORT_IPSME)
	set(ipsme_files
		IPSME_MsgEnv.cpp
		IPSME_MsgEnv.h)
endif()
#For MT runtime
add_static_library( TeleportClient SOURCES ${src_files} ${ipsme_files} ${rst_files} DEPENDENCIES TeleportCore libavstream ) 
SetTeleportDefaults(TeleportClient)
set_target_properties( TeleportClient PROPERTIES FOLDER Client)

#Include its root directory
target_include_directories(TeleportClient PUBLIC src)
#Include libavstream
target_include_directories(TeleportClient PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(TeleportClient PUBLIC ../thirdparty/basis_universal)
#Include json
target_include_directories(TeleportClient PUBLIC ../thirdparty/json)

target_include_directories(TeleportClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include )

target_include_directories(TeleportClient PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )
target_include_directories(TeleportClient PUBLIC ${SIMUL_PLATFORM_DIR}/External )

#include libdatachannel
target_include_directories(TeleportClient PRIVATE ../thirdparty/libdatachannel/include)
target_include_directories(TeleportClient PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/deps/json/single_include")
        # From OpenXR repo
target_include_directories(TeleportClient PUBLIC "${openxr_SOURCE_DIR}/src/common" "${openxr_SOURCE_DIR}/external/include")

if(WIN32)
elseif(ANDROID)
	#Include OpenGL ES 3.0
	target_include_directories(TeleportClient PUBLIC ../client/VrApi/Include) 
	target_include_directories(TeleportClient PUBLIC ../client/VrAppFramework/Include) 
	target_include_directories(TeleportClient PUBLIC ../client/LibOVRKernel/Src)
endif()

if(TELEPORT_CLIENT_SUPPORT_IPSME)
	target_include_directories(TeleportClient PUBLIC "${TELEPORT_CLIENT_MOSQUITTO_INCLUDE_DIR}")
endif()

target_compile_definitions(TeleportClient PUBLIC TELEPORT_CLIENT_SUPPORT_IPSME=$<BOOL:${TELEPORT_CLIENT_SUPPORT_IPSME}> WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)

#Include cpp-httplib
target_include_directories(TeleportClient PUBLIC ../thirdparty/cpp-httplib)

if(APPLE)
	# This is a bug in CMake that causes it to prefer the system version over
	# the one in the specified ROOT folder
	if(EXISTS ${OPENSSL_ROOT_DIR})
        # Use static files when OPENSSL_USE_STATIC_LIBS is set.
        # OPENSSL_USE_STATIC_LIBS is what CMake's FindOpenSSL looks at
        # to decide whether to use static libraries.
        if(OPENSSL_USE_STATIC_LIBS)
            set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.a" CACHE FILEPATH "" FORCE)
            set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.a" CACHE FILEPATH "" FORCE)
        else()
            set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.dylib" CACHE FILEPATH "" FORCE)
            set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.dylib" CACHE FILEPATH "" FORCE)
        endif()
	endif()
endif()
find_package(OpenSSL REQUIRED)
target_link_libraries(TeleportClient OpenSSL::SSL)
