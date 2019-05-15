cmake_minimum_required(VERSION 3.8)
project(pc_client)
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
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
include_directories(${SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include)
link_directories("${SIMUL}/lib/x64/v141/Static ${CONFIG_NAME}" "${SIMUL}/build/x64/v141/lib/${CONFIG_NAME}" ${CMAKE_SOURCE_DIR}/../libavstream/build_static/${CONFIG_NAME} ${CMAKE_SOURCE_DIR}/../thirdparty/enet/build_static/${CONFIG_NAME})

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

source_group("Shaders" src FILES ${shaders} )
add_static_executable( pc_client WIN32 ${srcs} ${shaders} )

set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
target_include_directories(pc_client PRIVATE ${SIMUL}/..) 
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include )
target_compile_features(pc_client PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client libavstream enet)
