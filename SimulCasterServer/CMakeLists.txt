cmake_minimum_required(VERSION 3.8)
project(SimulCasterServer)

# Build options
option(USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" ON)

set(DEBUG_CONFIGURATIONS Debug)

#Source files
set(src
	src/SimulCasterServer/CasterTypes.h
	src/SimulCasterServer/CaptureDelegates.h
	src/SimulCasterServer/CasterContext.h
	src/SimulCasterServer/CasterSettings.h
	src/SimulCasterServer/ClientData.cpp
	src/SimulCasterServer/ClientData.h
	src/SimulCasterServer/ClientMessaging.cpp
	src/SimulCasterServer/ClientMessaging.h
	src/SimulCasterServer/DiscoveryService.cpp
	src/SimulCasterServer/DiscoveryService.h 
	src/SimulCasterServer/ErrorHandling.h
	src/SimulCasterServer/ErrorHandling.cpp
	src/SimulCasterServer/ExtractedTypes.h
	src/SimulCasterServer/GeometryEncoder.cpp
	src/SimulCasterServer/GeometryEncoder.h
	src/SimulCasterServer/GeometryStore.cpp
	src/SimulCasterServer/GeometryStore.h
	src/SimulCasterServer/GeometryStreamingService.cpp
	src/SimulCasterServer/GeometryStreamingService.h
	src/SimulCasterServer/NetworkPipeline.cpp
	src/SimulCasterServer/NetworkPipeline.h
	src/SimulCasterServer/SourceNetworkPipeline.cpp
	src/SimulCasterServer/SourceNetworkPipeline.h
	src/SimulCasterServer/DefaultDiscoveryService.cpp
	src/SimulCasterServer/DefaultDiscoveryService.h
	src/SimulCasterServer/VideoEncodePipeline.cpp
	src/SimulCasterServer/VideoEncodePipeline.h	
	src/SimulCasterServer/AudioEncoder.cpp
	src/SimulCasterServer/AudioEncoder.h
	src/SimulCasterServer/AudioEncodePipeline.cpp
	src/SimulCasterServer/AudioEncodePipeline.h	
	src/SimulCasterServer/HTTPService.cpp
	src/SimulCasterServer/HTTPService.h 
	src/SimulCasterServer/DefaultHTTPService.cpp
	src/SimulCasterServer/DefaultHTTPService.h 
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
if(REMOTEPLAY_UNITY)
	set(src_plugin
		src/SimulCasterServer/UnityPlugin/Export.h
		src/SimulCasterServer/UnityPlugin/InteropStructures.h
		src/SimulCasterServer/UnityPlugin/PluginGraphics.h
		src/SimulCasterServer/UnityPlugin/PluginGraphics.cpp
		src/SimulCasterServer/UnityPlugin/PluginMain.cpp
	)

	add_library(SimulCasterServer SHARED ${src} ${src_plugin})
	set_target_properties( SimulCasterServer PROPERTIES 
		VS_DEBUGGER_COMMAND ${REMOTEPLAY_UNITY_EDITOR_DIR}/${REMOTEPLAY_UNITY_EDITOR_EXE}
		VS_DEBUGGER_COMMAND_ARGUMENTS "-projectPath \"${REMOTEPLAY_UNITY_PROJECT_DIR}\""
		VS_DEBUGGER_ENVIRONMENT ""
		)
	#Include Unity's native plugin interface headers
	target_include_directories(SimulCasterServer PUBLIC ${REMOTEPLAY_UNITY_EDITOR_DIR}/Data/PluginAPI)
else()
	add_library(SimulCasterServer STATIC ${src})
endif()
SetTeleportDefaults( SimulCasterServer )

#Include its root directory
target_include_directories(SimulCasterServer PUBLIC src)
#Include libavstream
target_include_directories(SimulCasterServer PUBLIC ../libavstream/include)
#Include libavstream
target_include_directories(SimulCasterServer PUBLIC ../SimulCasterAudio/src)
#Include Basis Universal
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/basis_universal/encoder)
#Include enet
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/enet/include)
#Include draco
target_include_directories(SimulCasterServer PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/draco/src)
#include OpenSSL
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/openssl/include)
#Include cpp-httplib
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/cpp-httplib)


#Include CUDA library location.
target_link_directories(SimulCasterServer PUBLIC "${LIBAV_CUDA_DIR}/lib/x64")

target_link_directories(SimulCasterServer PUBLIC ../thirdparty/openssl/x64/Release/lib)

target_link_libraries(SimulCasterServer SimulCasterAudio libavstream enet basisu efp winmm cudart d3d12 draco libssl_static libcrypto_static TeleportCore)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
else()
	set(def_platform PLATFORM_LINUX)
endif()

target_compile_definitions(SimulCasterServer PRIVATE ${def_platform})

if(${USE_ASYNC_NETWORK_PROCESSING})
	target_compile_definitions(SimulCasterServer PUBLIC ASYNC_NETWORK_PROCESSING)
endif()
set_target_properties( SimulCasterServer PROPERTIES FOLDER Teleport )