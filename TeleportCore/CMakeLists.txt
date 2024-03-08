cmake_minimum_required( VERSION 3.8 )
project( TeleportCore )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set(src_files TeleportCore.cpp ErrorHandling.cpp FontAtlas.cpp Input.cpp URLParser.cpp)
file(GLOB header_files	AnimationInterface.h
						CommonNetworking.h
						ErrorHandling.h
						FontAtlas.h
						Input.h
						Protocol.h
						StringFunctions.h
						TeleportUtility.h
						TextCanvas.h
						Threads.h
						Time.h
						Time.cpp
						URLParser.h )

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
#Include fmt
target_include_directories(TeleportCore PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )
#Include Platform - only for CppSl.sl
target_include_directories(TeleportCore PRIVATE ../firstparty)
target_include_directories(TeleportCore PUBLIC ${CMAKE_BINARY_DIR}/safe/include)
 