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
	AvatarManager.cpp
	AvatarManager.h
	basic_linear_algebra.h
	ClientBootstrap.cpp
	ClientBootstrap.h
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
	GoogleDeviceIdentityProvider.cpp
	GoogleDeviceIdentityProvider.h
	GoogleIdentityProvider.cpp
	GoogleIdentityProvider.h
	GuestIdentityProvider.cpp
	GuestIdentityProvider.h
	Identity.h
	Identity.cpp
	IdentityProvider.h
	IdentityStore.cpp
	IdentityStore.h
	OAuthHttp.cpp
	OAuthHttp.h
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

# Google OAuth client credentials are compiled in rather than read from a config file the user
# could open. Supply them in the environment, or in a .env file (never committed) beside this
# repository or its parent workspace. Without them, Google sign-in is simply not offered.
#
# There are two clients: the desktop one for the GUI client's loopback flow, and a "TV and
# Limited Input device" one for the terminal client's device flow. They are separate
# registrations in the Google Cloud console and their tokens are not interchangeable.
set(TELEPORT_GOOGLE_CREDENTIAL_VARS
	TELEPORT_GOOGLE_CLIENT_ID
	TELEPORT_GOOGLE_CLIENT_SECRET
	TELEPORT_GOOGLE_DEVICE_CLIENT_ID
	TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET)
foreach(_var ${TELEPORT_GOOGLE_CREDENTIAL_VARS})
	set(${_var} "$ENV{${_var}}")
endforeach()
foreach(_dotenv "${CMAKE_SOURCE_DIR}/.env" "${CMAKE_SOURCE_DIR}/../.env")
	if(EXISTS "${_dotenv}")
		# Re-run cmake when the credentials change.
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_dotenv}")
		file(STRINGS "${_dotenv}" _dotenv_lines)
		foreach(_line ${_dotenv_lines})
			# KEY=VALUE, ignoring comments, blank lines, "export " and surrounding quotes.
			if(_line MATCHES "^[ \t]*(export[ \t]+)?([A-Za-z_][A-Za-z0-9_]*)[ \t]*=[ \t]*(.*)$")
				set(_key "${CMAKE_MATCH_2}")
				set(_value "${CMAKE_MATCH_3}")
				string(REGEX REPLACE "[ \t]+$" "" _value "${_value}")
				string(REGEX REPLACE "^[\"'](.*)[\"']$" "\\1" _value "${_value}")
				# The environment wins, so a one-off build can override the file.
				if(_key IN_LIST TELEPORT_GOOGLE_CREDENTIAL_VARS AND NOT ${_key})
					set(${_key} "${_value}")
				endif()
			endif()
		endforeach()
	endif()
endforeach()

if(TELEPORT_GOOGLE_CLIENT_ID)
	message(STATUS "TeleportClient: Google sign-in enabled for client id ${TELEPORT_GOOGLE_CLIENT_ID}")
else()
	message(STATUS "TeleportClient: no TELEPORT_GOOGLE_CLIENT_ID set (environment or .env); Google sign-in will be unavailable in the GUI client.")
endif()
if(TELEPORT_GOOGLE_DEVICE_CLIENT_ID)
	message(STATUS "TeleportClient: device sign-in enabled for client id ${TELEPORT_GOOGLE_DEVICE_CLIENT_ID}")
else()
	message(STATUS "TeleportClient: no TELEPORT_GOOGLE_DEVICE_CLIENT_ID set; Google sign-in will be unavailable in the terminal client.")
endif()
# PRIVATE: the credentials must not leak into the compile line of every dependent target.
target_compile_definitions(TeleportClient PRIVATE
	TELEPORT_GOOGLE_CLIENT_ID="${TELEPORT_GOOGLE_CLIENT_ID}"
	TELEPORT_GOOGLE_CLIENT_SECRET="${TELEPORT_GOOGLE_CLIENT_SECRET}"
	TELEPORT_GOOGLE_DEVICE_CLIENT_ID="${TELEPORT_GOOGLE_DEVICE_CLIENT_ID}"
	TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET="${TELEPORT_GOOGLE_DEVICE_CLIENT_SECRET}")

#Include cpp-httplib
target_include_directories(TeleportClient PUBLIC ${cpp-httplib_SOURCE_DIR})

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

if(APPLE)
	target_link_libraries(TeleportClient "-framework Security" "-framework CoreFoundation")
elseif(UNIX AND NOT ANDROID)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
	target_link_libraries(TeleportClient PkgConfig::LIBSECRET)
endif()
