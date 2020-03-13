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
	src/SimulCasterServer/DiscoveryService.h
	src/SimulCasterServer/ExtractedTypes.h
	src/SimulCasterServer/GeometryEncoder.cpp
	src/SimulCasterServer/GeometryEncoder.h
	src/SimulCasterServer/GeometryStore.cpp
	src/SimulCasterServer/GeometryStore.h
	src/SimulCasterServer/GeometryStreamingService.cpp
	src/SimulCasterServer/GeometryStreamingService.h
	src/SimulCasterServer/NetworkPipeline.cpp
	src/SimulCasterServer/NetworkPipeline.h
	src/SimulCasterServer/VideoEncodePipeline.cpp
	src/SimulCasterServer/VideoEncodePipeline.h	
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

#Include its root directory
target_include_directories(SimulCasterServer PUBLIC src)
#Include libavstream
target_include_directories(SimulCasterServer PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/basis_universal)
#Include enet
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/enet/include)

#Include CUDA library location.
target_link_directories(SimulCasterServer PUBLIC "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v10.1/lib/x64")

target_link_libraries(SimulCasterServer libavstream enet basisu efp winmm cudart d3d12)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
else()
	set(def_platform PLATFORM_LINUX)
endif()

target_compile_definitions(SimulCasterServer PRIVATE ${def_platform})