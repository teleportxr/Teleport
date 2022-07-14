cmake_minimum_required(VERSION 3.8)
project(TeleportServer)

# Build options
option(USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" ON)

set(DEBUG_CONFIGURATIONS Debug)

#Source files
set(src
	src/TeleportServer/CasterTypes.h
	src/TeleportServer/CaptureDelegates.h
	src/TeleportServer/CasterContext.h
	src/TeleportServer/ServerSettings.h
	src/TeleportServer/ClientData.cpp
	src/TeleportServer/ClientData.h
	src/TeleportServer/ClientMessaging.cpp
	src/TeleportServer/ClientMessaging.h
	src/TeleportServer/ClientManager.cpp
	src/TeleportServer/ClientManager.h
	src/TeleportServer/DiscoveryService.cpp
	src/TeleportServer/DiscoveryService.h 
	src/TeleportServer/ExtractedTypes.h
	src/TeleportServer/GeometryEncoder.cpp
	src/TeleportServer/GeometryEncoder.h
	src/TeleportServer/GeometryStore.cpp
	src/TeleportServer/GeometryStore.h
	src/TeleportServer/GeometryStreamingService.cpp
	src/TeleportServer/GeometryStreamingService.h
	src/TeleportServer/NetworkPipeline.cpp
	src/TeleportServer/NetworkPipeline.h
	src/TeleportServer/SourceNetworkPipeline.cpp
	src/TeleportServer/SourceNetworkPipeline.h
	src/TeleportServer/DefaultDiscoveryService.cpp
	src/TeleportServer/DefaultDiscoveryService.h
	src/TeleportServer/VideoEncodePipeline.cpp
	src/TeleportServer/VideoEncodePipeline.h	
	src/TeleportServer/AudioEncoder.cpp
	src/TeleportServer/AudioEncoder.h
	src/TeleportServer/AudioEncodePipeline.cpp
	src/TeleportServer/AudioEncodePipeline.h	
	src/TeleportServer/HTTPService.cpp
	src/TeleportServer/HTTPService.h 
	src/TeleportServer/DefaultHTTPService.cpp
	src/TeleportServer/DefaultHTTPService.h 
)

#Whether to build dynamic runtime libraries.
if(${USE_DYNAMIC_RUNTIME})
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /Zi")
else()
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()

#Create static library with source files from group.
if(TELEPORT_UNITY)
	set(src_plugin
		src/TeleportServer/UnityPlugin/Export.h
		src/TeleportServer/UnityPlugin/InteropStructures.h
		src/TeleportServer/UnityPlugin/PluginGraphics.h
		src/TeleportServer/UnityPlugin/PluginGraphics.cpp
		src/TeleportServer/UnityPlugin/PluginMain.cpp
	)

	add_library(TeleportServer SHARED ${src} ${src_plugin})
	set_target_properties( TeleportServer PROPERTIES 
		VS_DEBUGGER_COMMAND ${REMOTEPLAY_UNITY_EDITOR_DIR}/${REMOTEPLAY_UNITY_EDITOR_EXE}
		VS_DEBUGGER_COMMAND_ARGUMENTS "-projectPath \"${REMOTEPLAY_UNITY_PROJECT_DIR}\""
		VS_DEBUGGER_ENVIRONMENT ""
		)
	#Include Unity's native plugin interface headers
	target_include_directories(TeleportServer PUBLIC ${REMOTEPLAY_UNITY_EDITOR_DIR}/Data/PluginAPI)
else()
	add_library(TeleportServer STATIC ${src})
endif()
SetTeleportDefaults( TeleportServer )

#Include its root directory
target_include_directories(TeleportServer PUBLIC src)
#Include libavstream
target_include_directories(TeleportServer PUBLIC ../libavstream/include)
#Include TeleportCore
target_include_directories(TeleportServer PUBLIC ../TeleportCore)
#Include TeleportAudio
target_include_directories(TeleportServer PUBLIC ../TeleportAudio)
#Include Basis Universal
target_include_directories(TeleportServer PUBLIC ../thirdparty/basis_universal/encoder)
#Include enet
target_include_directories(TeleportServer PUBLIC ../thirdparty/enet/include)
#Include draco
target_include_directories(TeleportServer PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(TeleportServer PUBLIC ../thirdparty/draco/src)
#include OpenSSL
target_include_directories(TeleportServer PUBLIC ../thirdparty/openssl/include)
#Include cpp-httplib
target_include_directories(TeleportServer PUBLIC ../thirdparty/cpp-httplib)


#Include CUDA library location.
#target_link_directories(TeleportServer PUBLIC "${LIBAV_CUDA_DIR}/lib/x64")

target_link_directories(TeleportServer PUBLIC ../thirdparty/openssl/lib/MD)

target_link_libraries(TeleportServer TeleportAudio TeleportCore libavstream enet basisu efp winmm d3d12 draco libssl libcrypto )

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
else()
	set(def_platform PLATFORM_LINUX)
endif()

target_compile_definitions(TeleportServer PRIVATE ${def_platform})

if(${USE_ASYNC_NETWORK_PROCESSING})
	target_compile_definitions(TeleportServer PUBLIC ASYNC_NETWORK_PROCESSING)
endif()
set_target_properties( TeleportServer PROPERTIES FOLDER Teleport )