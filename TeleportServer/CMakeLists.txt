cmake_minimum_required(VERSION 3.8)
project(TeleportServer)


set(DEBUG_CONFIGURATIONS Debug)

#Source files
set(src
	CasterTypes.h
	CaptureDelegates.h
	ClientNetworkContext.h
	ClientNetworkContext.cpp
	ServerSettings.h
	ClientData.cpp
	ClientData.h
	ClientMessaging.cpp
	ClientMessaging.h
	ClientManager.cpp
	ClientManager.h
	SignalingService.cpp
	SignalingService.h 
	Export.h
	ExtractedTypes.h
	Font.h
	Font.cpp
	GeometryEncoder.cpp
	GeometryEncoder.h
	GeometryStore.cpp
	GeometryStore.h
	GeometryStreamingService.cpp
	GeometryStreamingService.h
	NetworkPipeline.cpp
	NetworkPipeline.h
	SourceNetworkPipeline.cpp
	SourceNetworkPipeline.h
	VideoEncodePipeline.cpp
	VideoEncodePipeline.h	
	AudioEncoder.cpp
	AudioEncoder.h
	AudioEncodePipeline.cpp
	AudioEncodePipeline.h	
	HTTPService.cpp
	HTTPService.h 
	DefaultHTTPService.cpp
	DefaultHTTPService.h 
	Texture.h 
	Texture.cpp
	UnixDebugOutput.h 
)

#Whether to build dynamic runtime libraries.
if(TELEPORT_WINDOWS)
	if(${TELEPORT_USE_DYNAMIC_RUNTIME})
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /Zi")
	else()
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	endif()
endif()

#Create static library with source files from group.
if(TELEPORT_UNITY)
	set(src_plugin
		UnityPlugin/InteropStructures.h
		UnityPlugin/PluginGraphics.h
		UnityPlugin/PluginGraphics.cpp
		UnityPlugin/PluginClient.h
		UnityPlugin/PluginClient.cpp
		UnityPlugin/PluginMain.h
		UnityPlugin/PluginMain.cpp
	)
	add_library(TeleportServer SHARED ${src} ${src_plugin})
	set_target_properties( TeleportServer PROPERTIES 
		VS_DEBUGGER_COMMAND ${TELEPORT_UNITY_EDITOR_EXE}
		VS_DEBUGGER_COMMAND_ARGUMENTS "-projectPath \"${TELEPORT_UNITY_PROJECT_DIR}\""
		VS_DEBUGGER_ENVIRONMENT ""
		)
	#Include Unity's native plugin interface headers
	target_include_directories(TeleportServer PUBLIC ${TELEPORT_UNITY_EDITOR_DIR}/Data/PluginAPI)

	target_compile_definitions(TeleportServer PRIVATE TELEPORT_UNITY_SERVER=1)
	set_target_properties(TeleportServer PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY_RELEASE "${TELEPORT_SERVER_DLL_DIR}/"
			ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${TELEPORT_SERVER_DLL_DIR}/"
			RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TELEPORT_SERVER_DLL_DIR}/"
			)
else()
	add_library(TeleportServer STATIC ${src})
	#set_target_properties(TeleportServer PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/TeleportServer/Release"	)
	#add_library(TeleportServer SHARED ${src})
	#target_compile_definitions(TeleportServer PRIVATE TELEPORT_SERVER_DLL=1 TELEPORT_DYNAMIC_LINK=1)
endif()


SetTeleportDefaults( TeleportServer )

#Include its root directory
target_include_directories(TeleportServer PUBLIC .)
target_include_directories(TeleportServer PUBLIC .. )
#Include libavstream
target_include_directories(TeleportServer PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(TeleportServer PUBLIC ../thirdparty/basis_universal/encoder)
#Include draco
target_include_directories(TeleportServer PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(TeleportServer PUBLIC ../thirdparty/draco/src)
#include OpenSSL
target_include_directories(TeleportServer PUBLIC ../thirdparty/openssl/include)
#Include cpp-httplib
target_include_directories(TeleportServer PUBLIC ../thirdparty/cpp-httplib)
#Include Platform - for pure header functions only.sr
target_include_directories(TeleportServer PUBLIC ../firstparty)
# stb truetype
target_include_directories(TeleportServer PUBLIC ../firstparty/Platform/External/stb)
#include libdatachannel
target_include_directories(TeleportServer PUBLIC ../thirdparty/libdatachannel/include)
# safe
target_include_directories(TeleportServer PUBLIC ${CMAKE_BINARY_DIR}/safe/include)

if(${TELEPORT_SERVER_SUPPORT_WEBRTC})
	target_include_directories(TeleportServer PUBLIC ${TELEPORT_LIBWEBRTC_BUILD_DIR}/include)
endif()
target_include_directories(TeleportServer PUBLIC "${CMAKE_SOURCE_DIR}/thirdparty/libdatachannel/deps/json/single_include")

#Include CUDA library location.
if(UNIX)
	target_link_directories(TeleportServer PUBLIC ${LIBAV_CUDA_DIR}/lib64 /usr/lib/x86_64-linux-gnu)
	message("{LIBAV_CUDA_DIR}/libx64 ${LIBAV_CUDA_DIR}/libx64")
endif()
target_link_directories(TeleportServer PUBLIC ../thirdparty/openssl/x64/lib)
target_link_libraries(TeleportServer TeleportAudio TeleportCore libavstream basisu efp fmt LibDataChannel::LibDataChannelStatic pthreads4w::pthreadVC3)
if(WIN32)
	target_link_libraries(TeleportServer draco winmm d3d12 Secur32.lib)
else()
	target_link_libraries(TeleportServer draco_static ssl crypto cuda cudart atomic)
endif()
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