cmake_minimum_required(VERSION 3.8)
project(pc_client)
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
set( SIMUL "C:/Simul/4.2/Simul" CACHE STRING "Set the location of the Simul root directory. Usually the main source folder." )
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
set(scr_pc_impl
	SCR_Class_PC_Impl/PC_FrameBuffer.cpp
	SCR_Class_PC_Impl/PC_FrameBuffer.h
	SCR_Class_PC_Impl/PC_IndexBuffer.cpp
	SCR_Class_PC_Impl/PC_IndexBuffer.h
	SCR_Class_PC_Impl/PC_Pipeline.cpp
	SCR_Class_PC_Impl/PC_Pipeline.h
	SCR_Class_PC_Impl/PC_RenderPlatform.h
	SCR_Class_PC_Impl/PC_Sampler.cpp
	SCR_Class_PC_Impl/PC_Sampler.h
	SCR_Class_PC_Impl/PC_Shader.cpp
	SCR_Class_PC_Impl/PC_Shader.h
	SCR_Class_PC_Impl/PC_Texture.cpp
	SCR_Class_PC_Impl/PC_Texture.h
	SCR_Class_PC_Impl/PC_UniformBuffer.cpp
	SCR_Class_PC_Impl/PC_UniformBuffer.h
	SCR_Class_PC_Impl/PC_VertexBuffer.cpp
	SCR_Class_PC_Impl/PC_VertexBuffer.h
)
include_directories(${SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../SimulCasterRenderer/src)
link_directories("${SIMUL}/build/x64/v141/Static ${CONFIG_NAME}" "${SIMUL}/build/lib/${CONFIG_NAME}" "${SIMUL}/lib/x64/v141/Static ${CONFIG_NAME}" "${SIMUL}/lib/x64/v140/Static ${CONFIG_NAME}" ${CMAKE_SOURCE_DIR}/../libavstream/build_static/${CONFIG_NAME} ${CMAKE_SOURCE_DIR}/../thirdparty/enet/build_static/${CONFIG_NAME} ${CMAKE_SOURCE_DIR}/build/x64/lib ${CMAKE_SOURCE_DIR}/build/x64/SimulCasterRenderer/Release )

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DSIMUL=${SIMUL}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DSIMUL=${SIMUL}")
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

source_group("Shaders" src FILES ${shaders} )
add_static_executable( pc_client WIN32 ${srcs} ${scr_pc_impl} ${shaders} )

set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
target_include_directories(pc_client PRIVATE ${SIMUL}/..) 
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include )
target_compile_features(pc_client PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client libavstream enet SimulCasterRenderer)
#target_link_libraries(pc_client debug effects11_MTd directxtex_MTd )
target_link_libraries(pc_client optimized effects11_MT directxtex_MT)
