cmake_minimum_required( VERSION 3.8 )
project( TeleportCore )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set(src_files			Animation.cpp DecodeMesh.cpp TeleportCore.cpp ErrorHandling.cpp FontAtlas.cpp Input.cpp 
						Time.cpp URLParser.cpp)
file(GLOB header_files	Animation.h
						DecodeMesh.h
						ErrorHandling.h
						FontAtlas.h
						Input.h
						Logging.h
						Protocol.h
						ResourceStreams.h
						StringFunctions.h
						TeleportUtility.h
						TextCanvas.h
						Threads.h
						Time.h
						URLParser.h )
set(export_includes export_include/TeleportCore/InputTypes.h 
						export_include/TeleportCore/CommonNetworking.h)

if(ANDROID)
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
endif()

#For MT runtime
add_library(TeleportCore STATIC ${src_files} ${header_files} ${export_includes})
SetTeleportDefaults( TeleportCore )
set_target_properties(TeleportCore PROPERTIES FOLDER Teleport)
target_include_directories(TeleportCore PUBLIC ..)
target_include_directories(TeleportCore PUBLIC export_include)
#Include its root directory
target_include_directories(TeleportCore PRIVATE src)
#Include libavstream
target_include_directories(TeleportCore PRIVATE ../libavstream/include)
target_include_directories(TeleportCore PRIVATE ../libavstream/export_include)
#Include fmt
target_include_directories(TeleportCore PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )
target_include_directories(TeleportCore PUBLIC ${SIMUL_PLATFORM_DIR}/External )
#Include Platform - only for CppSl.sl
target_include_directories(TeleportCore PRIVATE ../firstparty)
target_include_directories(TeleportCore PUBLIC ${CMAKE_BINARY_DIR}/safe/include)
#draco
#Include draco
target_include_directories(TeleportCore PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(TeleportCore PUBLIC ../thirdparty/draco/src)
target_include_directories(TeleportCore PUBLIC ../thirdparty/draco/third_party/eigen)
target_include_directories(TeleportCore PUBLIC ../thirdparty/draco/third_party/tinygltf)
 
#ktx
target_include_directories(TeleportCore PUBLIC ${CMAKE_BINARY_DIR}/ktx/include)
target_include_directories(TeleportCore PUBLIC ${CMAKE_BINARY_DIR}/ktx/lib)