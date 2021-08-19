cmake_minimum_required( VERSION 3.8 )
project( SimulCasterAudio )

# Build options
option(SA_BUILD_SHARED_LIB "Build shared library" OFF)

set(DEBUG_CONFIGURATIONS Debug)

# Source
set( src_root
	src/AudioCommon.h
	src/AudioCommon.cpp
	)
set( src_crossplatform
	src/crossplatform/AudioStreamTarget.cpp
	src/crossplatform/AudioStreamTarget.h
	src/crossplatform/CustomAudioStreamTarget.cpp
	src/crossplatform/CustomAudioStreamTarget.h
	src/crossplatform/AudioPlayer.cpp
	src/crossplatform/AudioPlayer.h
	src/crossplatform/AudioLog.cpp
	src/crossplatform/AudioLog.h
	src/crossplatform/NetworkPipeline.cpp
	src/crossplatform/NetworkPipeline.h
	)
set( src_pc
	src/pc/PC_AudioPlayer.cpp
	src/pc/PC_AudioPlayer.h
	)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)

if(WIN32)
	set( src_public ${src_root} ${src_crossplatform} ${src_pc})
else()
	set( src_public ${src_root} ${src_crossplatform})
endif()

message ( "src_public ${src_public}" )
#source_group("src" src FILES ${src_root} )
#source_group("src\\crossplatform" src FILES ${src_crossplatform} )

if(SA_BUILD_SHARED_LIB)
	#For MD runtime
	add_library( SimulCasterAudio STATIC ${src_public} ) 
else()
	#For MT runtime
	add_static_library( SimulCasterAudio SOURCES ${src_public} )  
	set_target_properties( SimulCasterAudio PROPERTIES FOLDER Client )
endif()
SetDefaults( SimulCasterAudio )




#target_compile_features(libavstream PRIVATE cxx_std_17)