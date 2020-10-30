cmake_minimum_required(VERSION 3.8)
project(pc_client)
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
file(GLOB inifiles "*.ini")
set(srcs
	ClientRenderer.cpp
	Main.cpp
	PCDiscoveryService.cpp
	stdafx.cpp
	ClientRenderer.h
	Config.h
	Resource.h
	PCDiscoveryService.h
	stdafx.h
	targetver.h
	
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
	SCR_Class_PC_Impl/PC_AudioPlayer.cpp
	SCR_Class_PC_Impl/PC_AudioPlayer.h
)
if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DPLATFORM_DIR=${REMOTEPLAY_PLATFORM_DIR}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DPLATFORM_DIR=${REMOTEPLAY_PLATFORM_DIR}")
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

source_group("Shaders" src FILES ${shaders} )
add_static_executable( pc_client WIN32 SOURCES ${srcs} ${scr_pc_impl} ${shaders} ${inifiles} )

target_include_directories(pc_client PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR} ${REMOTEPLAY_SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../SimulCasterAudio/src ../SimulCasterRenderer/src ../thirdparty/basis_universal ${REMOTEPLAY_PLATFORM_DIR}/Windows)
target_link_directories(pc_client PRIVATE "${REMOTEPLAY_PLATFORM_DIR}/build/lib/${CONFIG_NAME}" )
set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/HLSL/HLSL11.json" )
add_sfx_shader_project(pc_client ${JSON_FILE} INCLUDES "${SIMUL_PLATFORM_DIR}/DirectX11/HLSL" "${SIMUL_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${shaders} ${SHADER_INCLUDES} ${CORE_SHADER_INCLUDES} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/Platform/DirectX11/shaderbin" DEFINES PLATFORM_D3D11_SFX=1)
set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
set_target_properties(pc_client PROPERTIES
		VS_DEBUGGER_ENVIRONMENT       "PATH=C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x86;$(PATH)"	 )		
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include )
target_compile_features(pc_client PRIVATE cxx_std_17)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client libavstream enet SimulCasterAudio SimulCasterRenderer basisu winmm)

target_link_libraries(pc_client Core_MT SimulCrossPlatform_MT SimulMath_MT SimulDirectX11_MT effects11_MT directxtex_MT )
