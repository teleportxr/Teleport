cmake_minimum_required(VERSION 3.8)
project(pc_client)

option(TELEPORT_CLIENT_USE_D3D12 "Use D3D12?" OFF)

option(TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER "Use Platform video decoder? Currently requires TELEPORT_CLIENT_USE_D3D12" OFF)

if(${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER})
	if(NOT ${TELEPORT_CLIENT_USE_D3D12})
		message("Warning: Setting TELEPORT_CLIENT_USE_D3D12 true because TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER is true.")
		set(TELEPORT_CLIENT_USE_D3D12 ON)
	endif()
endif()

file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")
file(GLOB inifiles "*.ini")
set(srcs
	ClientRenderer.cpp
	Main.cpp
	UseOpenXR.cpp
	PCDiscoveryService.cpp
	stdafx.cpp
	VideoDecoder.cpp
	Gui.cpp
	Gui.h
	ClientRenderer.h
	Config.h
	UseOpenXR.h
	Resource.h
	PCDiscoveryService.h
	stdafx.h
	targetver.h
	VideoDecoder.h
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
	SCR_Class_PC_Impl/PC_MemoryUtil.cpp
	SCR_Class_PC_Impl/PC_MemoryUtil.h
)

set(av_parser
	AVParser/BitReader.cpp
	AVParser/BitReader.h
	AVParser/H264Types.h
	AVParser/HevcTypes.h
	AVParser/HevcParser.cpp
	AVParser/HevcParser.h
	AVParser/Parser.cpp
	AVParser/Parser.h
)

if(WIN32)
	set(def_platform PLATFORM_WINDOWS UNICODE _WIN32_WINNT=0x0601)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DPLATFORM_DIR=${REMOTEPLAY_PLATFORM_DIR}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DPLATFORM_DIR=${REMOTEPLAY_PLATFORM_DIR}")
else()
	message(FATAL_ERROR "Unsupported platform!")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4711")
source_group("Shaders" src FILES ${shaders} )
source_group("SCR_Class_PC_Impl" src FILES ${scr_pc_impl} )
source_group("AVParser" src FILES ${av_parser} )

add_static_executable( pc_client WIN32 SOURCES ${srcs} ${scr_pc_impl} ${av_parser} ${shaders} ${inifiles} )


target_include_directories(pc_client PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR} ${REMOTEPLAY_SIMUL}/.. ${CMAKE_SOURCE_DIR}/../thirdparty/enet/include ../libavstream/include ../TeleportAudio/src ../ClientRender/src ../thirdparty/basis_universal ${REMOTEPLAY_PLATFORM_DIR}/Windows)

if(TELEPORT_CLIENT_USE_D3D12)
	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx/DirectX12.json" )
	add_sfx_shader_project(pc_client ${JSON_FILE} INCLUDES "${SIMUL_PLATFORM_DIR}/DirectX12/Sfx" "${SIMUL_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${shaders} ${SHADER_INCLUDES} ${CORE_SHADER_INCLUDES} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/Platform/DirectX12/shaderbin")
	target_link_libraries(pc_client SimulDirectX12_MT)
else()
	set( JSON_FILE "${SIMUL_PLATFORM_DIR}/DirectX11/Sfx/DirectX11.json" )
	add_sfx_shader_project(pc_client ${JSON_FILE} INCLUDES "${SIMUL_PLATFORM_DIR}/DirectX11/Sfx" "${SIMUL_DIR}/Shaders/SL" "${SIMUL_PLATFORM_DIR}/Shaders/SL" SOURCES ${shaders} ${SHADER_INCLUDES} ${CORE_SHADER_INCLUDES} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/Platform/DirectX11/shaderbin" DEFINES PLATFORM_D3D11_SFX=1)
	target_link_libraries(pc_client SimulDirectX11_MT)
	target_link_libraries(pc_client d3d12)
endif()
target_compile_definitions(pc_client PUBLIC TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}>) 	

SetTeleportDefaults(pc_client)
set_target_properties( pc_client PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows")
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties( pc_client PROPERTIES FOLDER Client)
set_target_properties( pc_client PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x86;$(PATH)"	 )		
target_include_directories(pc_client PUBLIC ../thirdparty/enet/Include ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/include ${SIMUL_PLATFORM_DIR}/External/fmt/include ${SIMUL_PLATFORM_DIR}/External/imgui)
target_compile_features(pc_client PRIVATE cxx_std_17)
target_link_directories(pc_client PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/openxr-sdk/lib)
#target_link_directories(pc_client PUBLIC "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	target_compile_definitions(pc_client PRIVATE PLATFORM_64BIT)
endif()

target_link_libraries(pc_client debug openxr_loaderd optimized openxr_loader) 
target_link_libraries(pc_client libavstream enet TeleportAudio ClientRender TeleportClient TeleportCore basisu winmm)
target_link_libraries(pc_client Core_MT SimulCrossPlatform_MT SimulMath_MT directxtex_MT ImGui_MT fmt)
target_link_libraries(pc_client -ignore:4711)

target_link_options(pc_client PUBLIC /ignore:4711)
target_compile_options(pc_client PRIVATE /wd4711)

set(CPACK_PACKAGE_INSTALL_DIRECTORY "cpack_install")

install(TARGETS pc_client
		RUNTIME
		DESTINATION build_client/bin/Release
		COMPONENT client)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}\\\\client.ini" DESTINATION pc_client COMPONENT client)
message("FILES ${shaders} DESTINATION pc_client\\\\Shaders COMPONENT client")

#install(FILES ${shaders} DESTINATION pc_client\\\\Shaders COMPONENT client)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}\\\\Shaders" DESTINATION pc_client COMPONENT client)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}\\\\textures" DESTINATION pc_client COMPONENT client)