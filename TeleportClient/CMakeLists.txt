cmake_minimum_required( VERSION 3.8 )
project( TeleportClient )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set( src_files
	ClientDeviceState.cpp
	ClientDeviceState.h
	basic_linear_algebra.h
	)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)
#Include Basis Universal
include_directories(../thirdparty/basis_universal)
#Include enet
include_directories(../thirdparty/enet/include)

if(WIN32)
# Removed circular dependency
elseif(ANDROID)
	#Include OpenGL ES 3.0
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
	include_directories(../client/LibOVRKernel/Src)
endif()

#For MT runtime
add_static_library( TeleportClient SOURCES ${src_files} )  
set_target_properties( TeleportClient PROPERTIES FOLDER Client)

#For MD runtime
#add_library( SimulCasterRenderer STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)