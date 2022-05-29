cmake_minimum_required( VERSION 3.8 )
project( TeleportClient )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set(src_files
	basic_linear_algebra.h
	ClientDeviceState.cpp
	ClientDeviceState.h
	ClientPipeline.cpp
	ClientPipeline.h
	Config.h
	DiscoveryService.h
	Log.cpp
	Log.h
	OpenXR.cpp
	OpenXR.h
	ServerTimestamp.cpp
	ServerTimestamp.h
	SessionClient.cpp
	SessionClient.h
)

#For MT runtime
add_static_library( TeleportClient SOURCES ${src_files} DEPENDENCIES TeleportCore)
set_target_properties( TeleportClient PROPERTIES FOLDER Client)

#Include its root directory
target_include_directories(TeleportClient PUBLIC src)
#Include libavstream
target_include_directories(TeleportClient PUBLIC ../libavstream/include)
#Include Basis Universal
target_include_directories(TeleportClient PUBLIC ../thirdparty/basis_universal)
#Include enet
target_include_directories(TeleportClient PUBLIC ../thirdparty/enet/include)

target_include_directories(TeleportClient PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include )

target_include_directories(TeleportClient PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )

if(WIN32)
# Removed circular dependency
elseif(ANDROID)
	#Include OpenGL ES 3.0
	target_include_directories(TeleportClient PUBLIC ../client/VrApi/Include) 
	target_include_directories(TeleportClient PUBLIC ../client/VrAppFramework/Include) 
	target_include_directories(TeleportClient PUBLIC ../client/LibOVRKernel/Src)
endif()

#For MD runtime
#add_library( ClientRender STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)