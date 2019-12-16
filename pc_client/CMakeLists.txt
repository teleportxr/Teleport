cmake_minimum_required(VERSION 3.8)
project(pc_client)
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
set(srcs
	ClientRenderer.cpp
	Main.cpp
	SessionClient.cpp
	stdafx.cpp
	Log.cpp
	ClientRenderer.h
	Config.h
	Input.h
	Resource.h
	SessionClient.h
	stdafx.h
	targetver.h
	Log.h
)
set(scr_pc_impl
	SCR_Class_PC_Impl/PC_DeviceContext.cpp
	SCR_Class_PC_Impl/PC_DeviceContext.h
	SCR_Class_PC_Impl/PC_Effect.cpp
	SCR_Class_PC_Impl/PC_Effect.h
	SCR_Class_PC_Impl/PC_FrameBuffer.cpp
	SCR_Class_PC_Impl/PC_FrameBuffer.h
	SCR_Class_PC_Impl/PC_IndexBuffer.cpp 
	SCR_Class_PC_Impl/PC_IndexBuffer.h
	SCR_Class_PC_Impl/PC_RenderPlatform.h
	SCR_Class_PC_Impl/PC_RenderPlatform.cpp
	SCR_Class_PC_Impl/PC_Sampler.cpp
	SCR_Class_PC_Impl/PC_Sampler.h
	SCR_Class_PC_Impl/PC_Shader.cpp
	SCR_Class_PC_Impl/PC_Shader.h
	SCR_Class_PC_Impl/PC_ShaderStorageBuffer.cpp
	SCR_Class_PC_Impl/PC_ShaderStorageBuffer.h
	SCR_Class_PC_Impl/PC_Texture.cpp
	SCR_Class_PC_Impl/PC_Texture.h
	SCR_Class_PC_Impl/PC_UniformBuffer.cpp
	SCR_Class_PC_Impl/PC_UniformBuffer.h
	SCR_Class_PC_Impl/PC_VertexBuffer.cpp
	SCR_Class_PC_Impl/PC_VertexBuffer.h
)
if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DSIMUL=${REMOTEPLAY_SIMUL}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DSIMUL=${REMOTEPLAY_SIMUL}")
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

source_group("Shaders" src FILES ${shaders} )
add_static_executable( pc_client WIN32 ${srcs} ${scr_pc_impl} ${shaders} )

target_include_directories(pc_client PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR} ${REMOTEPLAY_SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../SimulCasterRenderer/src ../thirdparty/basis_universal)
target_link_directories(pc_client PRIVATE "${REMOTEPLAY_SIMUL}/build/x64/v141/Static ${CONFIG_NAME}" "${REMOTEPLAY_SIMUL}/build/lib/${CONFIG_NAME}"
	"${REMOTEPLAY_SIMUL}/lib/x64/v141/Static ${CONFIG_NAME}" "${REMOTEPLAY_SIMUL}/lib/x64/v140/Static ${CONFIG_NAME}" )

set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
target_include_directories(pc_client PRIVATE ${REMOTEPLAY_SIMUL}/..) 
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include )
target_compile_features(pc_client PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client libavstream enet SimulCasterRenderer basisu)

target_link_libraries(pc_client debug effects11_MTd optimized effects11_MT debug directxtex_MTd optimized directxtex_MT)
