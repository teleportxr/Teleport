cmake_minimum_required( VERSION 3.8 )
project( TeleportAudio )

# Build options
option(SA_BUILD_SHARED_LIB "Build shared library" OFF)

set(DEBUG_CONFIGURATIONS Debug)

# Source
set( src_root
	AudioCommon.h
	AudioCommon.cpp
	)
set( src_crossplatform
	AudioStreamTarget.cpp
	AudioStreamTarget.h
	CustomAudioStreamTarget.cpp
	CustomAudioStreamTarget.h
	AudioPlayer.cpp
	AudioPlayer.h
	NetworkPipeline.cpp
	NetworkPipeline.h
	)
set( src_pc
	PC_AudioPlayer.cpp
	PC_AudioPlayer.h
	PC_AudioCapture.cpp
	PC_AudioCapture.h
	)

set( src_linux
	LinuxAudioPlayer.cpp
	LinuxAudioPlayer.h
	)

if(WIN32)
	set( src_public ${src_root} ${src_crossplatform} ${src_pc})
elseif(PLATFORM_LINUX)
	set( src_public ${src_root} ${src_crossplatform} ${src_linux})
else()
	set( src_public ${src_root} ${src_crossplatform})
endif()

#message ( "src_public ${src_public}" )
#source_group("src" src FILES ${src_root} )
#source_group("src\\crossplatform" src FILES ${src_crossplatform} )

if(SA_BUILD_SHARED_LIB)
	#For MD runtime
	add_library( TeleportAudio STATIC ${src_public} ) 
else()
	#For MT runtime
	add_static_library( TeleportAudio SOURCES ${src_public} )  
endif()
SetTeleportDefaults( TeleportAudio )
set_target_properties( TeleportAudio PROPERTIES FOLDER Teleport )

target_include_directories(TeleportAudio PUBLIC ../libavstream/include ../libavstream/export_include ../)
target_include_directories(TeleportAudio PUBLIC ${SIMUL_PLATFORM_DIR}/External/fmt/include )
target_include_directories(TeleportAudio PUBLIC ../firstparty )

if(PLATFORM_LINUX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)
	target_include_directories(TeleportAudio PRIVATE ${PULSEAUDIO_INCLUDE_DIRS})
	target_link_libraries(TeleportAudio ${PULSEAUDIO_LIBRARIES})
endif()

#target_compile_features(libavstream PRIVATE cxx_std_17)