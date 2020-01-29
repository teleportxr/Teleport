cmake_minimum_required(VERSION 3.8)
project(SimulCasterServer)

# Build options
option(USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" ON)

set(DEBUG_CONFIGURATIONS Debug)

#Source files
set(src
	src/SimulCasterServer/CaptureComponent.h
	src/SimulCasterServer/CasterContext.h
	src/SimulCasterServer/CasterSettings.h
	src/SimulCasterServer/ClientMessaging.cpp
	src/SimulCasterServer/ClientMessaging.h
	src/SimulCasterServer/DiscoveryService.h
	src/SimulCasterServer/GeometryEncoder.cpp
	src/SimulCasterServer/GeometryEncoder.h
	src/SimulCasterServer/GeometryStore.cpp
	src/SimulCasterServer/GeometryStore.h
	src/SimulCasterServer/GeometryStreamingService.cpp
	src/SimulCasterServer/GeometryStreamingService.h
	src/SimulCasterServer/NetworkPipeline.cpp
	src/SimulCasterServer/NetworkPipeline.h
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
		src/SimulCasterServer/UnityPlugin/PluginMain.cpp
	)

	add_library(SimulCasterServer SHARED ${src} ${src_plugin})
	set_target_properties( SimulCasterServer PROPERTIES 
		VS_DEBUGGER_COMMAND "${REMOTEPLAY_UNITY_EDITOR_PATH}"
		VS_DEBUGGER_COMMAND_ARGUMENTS "-projectPath \"${REMOTEPLAY_UNITY_PROJECT_DIR}\""
		VS_DEBUGGER_ENVIRONMENT ""
		)
else()
	add_library(SimulCasterServer STATIC ${src})
endif()

#Include its root directory
target_include_directories(SimulCasterServer PUBLIC src)
#Include libavstream
target_include_directories(SimulCasterServer PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/basis_universal)
#Include enet
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/enet/include)

target_link_libraries( SimulCasterServer libavstream enet basisu efp Winmm.lib)