cmake_minimum_required(VERSION 3.8)
project(SimulCasterServer)

# Build options
option(USE_DYNAMIC_RUNTIME "Use dynamic (MD) runtime?" ON)

set(DEBUG_CONFIGURATIONS Debug)

#Source files
set(src
	src/SimulCasterServer/CasterContext.h
	src/SimulCasterServer/CasterSettings.h
	src/SimulCasterServer/ClientMessaging.cpp
	src/SimulCasterServer/ClientMessaging.h
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
add_library(SimulCasterServer STATIC ${src})

#Include its root directory
target_include_directories(SimulCasterServer PUBLIC src)
#Include libavstream
target_include_directories(SimulCasterServer PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/basis_universal)
#Include enet
target_include_directories(SimulCasterServer PUBLIC ../thirdparty/enet/include)