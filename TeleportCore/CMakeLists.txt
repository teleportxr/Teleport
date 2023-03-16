cmake_minimum_required( VERSION 3.8 )
project( TeleportCore )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set(src_files TeleportCore.cpp ErrorHandling.cpp FontAtlas.cpp Input.cpp )
file(GLOB header_files *.h )

if(ANDROID)
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
endif()

#For MT runtime
add_library(TeleportCore STATIC ${src_files} ${header_files} )
SetTeleportDefaults( TeleportCore )
set_target_properties(TeleportCore PROPERTIES FOLDER Teleport)
target_include_directories(TeleportCore PUBLIC ..)
#Include its root directory
target_include_directories(TeleportCore PRIVATE src)
#Include libavstream
target_include_directories(TeleportCore PRIVATE ../libavstream/include)
#Include Basis Universal
target_include_directories(TeleportCore PRIVATE ../thirdparty/basis_universal)
#Include enet
target_include_directories(TeleportCore PRIVATE ../thirdparty/enet/include)
target_include_directories(TeleportCore PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )
 