cmake_minimum_required(VERSION 3.8)
project(pc_client)

set(srcs
	ClientRenderer.cpp
	Main.cpp
	SessionClient.cpp
	stdafx.cpp
	ClientRenderer.h
	Config.h
	Input.h
	Resource.h
	SessionClient.h
	stdafx.h
	targetver.h
)
include_directories($ENV{SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include)
link_directories("$ENV{SIMUL}/lib/x64/v140/Static Release" ${CMAKE_SOURCE_DIR}/../libavstream/build_static/release ${CMAKE_SOURCE_DIR}/../thirdparty/enet/build_static/release)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

add_executable (pc_client ${srcs})

set_target_properties(pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties(pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_compile_features(pc_client PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client libavstream enet)
