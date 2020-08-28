cmake_minimum_required( VERSION 3.8 )
project( SimulCasterAudio )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set( src_root
	src/AudioCommon.h
	src/AudioCommon.cpp
	)
set( src_crossplatform
	src/crossplatform/AudioStreamTarget.cpp
	src/crossplatform/AudioStreamTarget.h
	src/crossplatform/AudioPlayer.cpp
	src/crossplatform/AudioPlayer.h
	src/crossplatform/AudioLog.cpp
	src/crossplatform/AudioLog.h
	)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)

set( src_public ${src_root} ${src_crossplatform} )
message ( "src_public ${src_public}" )
#source_group("src" src FILES ${src_root} )
#source_group("src\\crossplatform" src FILES ${src_crossplatform} )

#For MT runtime
add_static_library( SimulCasterAudio SOURCES ${src_public} )  
set_target_properties( SimulCasterAudio PROPERTIES FOLDER Client )

#For MD runtime
#add_library( SimulCasterAudio STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)