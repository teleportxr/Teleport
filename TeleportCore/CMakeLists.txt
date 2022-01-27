cmake_minimum_required( VERSION 3.8 )
project( TeleportCore )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set(src_files TeleportCore.cpp)
file(GLOB header_files *.h)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)
#Include Basis Universal
include_directories(../thirdparty/basis_universal)
#Include enet
include_directories(../thirdparty/enet/include)

if(ANDROID)
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
endif()

#For MT runtime
add_library(TeleportCore STATIC ${src_files} ${header_files})
set_target_properties(TeleportCore PROPERTIES FOLDER Teleport)
