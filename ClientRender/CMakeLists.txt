cmake_minimum_required( VERSION 3.8 )
project( ClientRender )

# Build options
set(DEBUG_CONFIGURATIONS Debug)

configure_file(${CMAKE_SOURCE_DIR}/thirdparty/flecs/flecs.c ${CMAKE_CURRENT_BINARY_DIR}/flecs.c @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/thirdparty/flecs/flecs.h ${CMAKE_CURRENT_BINARY_DIR}/flecs.h @ONLY)

# Source
set( src_root
	CanvasTextRenderer.cpp
	CanvasTextRenderer.h
	Common.h
	Common.cpp
	IndexBuffer.h
	IndexBuffer.cpp
	Texture.cpp
	Texture.h
	UniformBuffer.cpp
	UniformBuffer.h
	VertexBuffer.h
	VertexBuffer.cpp
	VertexBufferLayout.h
	Animation.cpp
	Animation.h
	API.cpp
	API.h
	Camera.cpp
	Camera.h
	FontAtlas.cpp
	FontAtlas.h
	GeometryCache.cpp
	GeometryCache.h
	GeometryDecoder.cpp
	GeometryDecoder.h
	Gui.cpp
	Gui.h
	InstanceRenderer.cpp
	InstanceRenderer.h
	Light.cpp
	Light.h
	LinkRenderer.cpp
	LinkRenderer.h
	Material.cpp
	Material.h
	Mesh.cpp
	Mesh.h
	Node.cpp
	Node.h
	NodeManager.cpp
	NodeManager.h
	Resource.h
	Resource.cpp 
	ResourceCreator.cpp
	ResourceCreator.h
	ResourceManager.h
	Renderer.h
	Renderer.cpp
	ShaderResource.cpp
	ShaderResource.h
	Skeleton.cpp
	Skeleton.h
	SkeletonInstance.cpp
	SkeletonInstance.h
	Transform.cpp
	Transform.h
	TextCanvas.cpp
	TextCanvas.h
	MemoryUtil.cpp
	MemoryUtil.h
	Tests.cpp
	Tests.h
	VideoDecoderBackend.cpp
	VideoDecoderBackend.h
	)

set(src_node_components
	NodeComponents/AnimationComponent.cpp
	NodeComponents/AnimationState.cpp
	NodeComponents/Component.cpp
	NodeComponents/SubSceneComponent.cpp
	NodeComponents/VisibilityComponent.cpp
)

set(hdr_node_components
	NodeComponents/AnimationComponent.h
	NodeComponents/AnimationState.h
	NodeComponents/Component.h
	NodeComponents/SubSceneComponent.h
	NodeComponents/VisibilityComponent.h
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
set(shaders "${CMAKE_SOURCE_DIR}/client/Shaders/link.sfx" "${CMAKE_SOURCE_DIR}/client/Shaders/pbr.sfx")
file(GLOB shader_includes "${CMAKE_SOURCE_DIR}/client/Shaders/*.sl" )
file(GLOB global_shader_includes "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders/*.sl")
source_group("Shaders" src FILES ${shaders} )
source_group("Shader Includes" src FILES ${shader_includes} )
source_group("Global Shader Includes" src FILES ${global_shader_includes} )

set(flecs_files ${CMAKE_CURRENT_BINARY_DIR}/flecs.c ${CMAKE_CURRENT_BINARY_DIR}/flecs.h
	)
source_group("Source Files\\Node Components" src FILES ${src_node_components})
source_group("Header Files\\Node Components" src FILES ${hdr_node_components})
source_group("AVParser" src FILES ${av_parser} )

set( src_public ${src_root} ${src_node_components} ${hdr_node_components} )
#message ( "src_public ${src_public}" )

if(ANDROID)
	add_subdirectory(../thirdparty/draco
			${CMAKE_BINARY_DIR}/draco_build)
	include_directories("${CMAKE_BINARY_DIR}" ../thirdparty/draco)
endif()

#For MT runtime
add_static_library(ClientRender SOURCES ${src_public} ${av_parser} ${flecs_files} ${shaders} ${shader_includes} ${global_shader_includes})  
SetTeleportDefaults(ClientRender)
#Include its root directory
target_include_directories(ClientRender PRIVATE src)
#include flecs
target_include_directories(ClientRender PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
#Include libavstream
target_include_directories(ClientRender PRIVATE ../libavstream/include)
#Include Basis Universal
target_include_directories(ClientRender PRIVATE ../thirdparty/basis_universal)
#Include stb
target_include_directories(ClientRender PRIVATE ../firstparty/Platform/External/stb)
#Include imgui
target_include_directories(ClientRender PRIVATE ${SIMUL_PLATFORM_DIR}/External/imgui)
#Include draco
target_include_directories(ClientRender PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(ClientRender PUBLIC ../thirdparty/draco/src)
target_include_directories(ClientRender PUBLIC ../thirdparty/draco/third_party/eigen)
target_include_directories(ClientRender PUBLIC ../thirdparty/draco/third_party/tinygltf)
set_target_properties(ClientRender PROPERTIES FOLDER Client)
target_link_libraries(ClientRender draco TeleportClient)
if(TELEPORT_CLIENT_USE_D3D12)
	target_link_libraries(ClientRender SimulDirectX12_MT)
endif()
if(TELEPORT_CLIENT_USE_D3D11)
message("TELEPORT_CLIENT_USE_D3D11")
	target_link_libraries(ClientRender SimulDirectX11_MT)
	target_link_libraries(ClientRender d3d12)
endif()
if(TELEPORT_CLIENT_USE_VULKAN)
	target_link_libraries(ClientRender SimulVulkan_MT)
endif()
target_compile_definitions(ClientRender PUBLIC TELEPORT_CLIENT_USE_VULKAN=$<BOOL:${TELEPORT_CLIENT_USE_VULKAN}> TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_D3D11=$<BOOL:${TELEPORT_CLIENT_USE_D3D11}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}> TINYGLTF_NO_STB_IMAGE=1)

if(WIN32)
# Removed circular dependency
elseif(ANDROID)
	#Include OpenGL ES 3.0
	target_include_directories(ClientRender ../client/VrApi/Include) 
	target_include_directories(ClientRender ../client/VrAppFramework/Include) 
	target_include_directories(ClientRender ../client/LibOVRKernel/Src)
	target_link_libraries( ClientRender draco)
			# Links the target library to the log library
			# included in the NDK.
			#${log-lib} )
endif()

add_multiplatform_sfx_shader_project( ClientRender INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../client/Shaders" "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders" "${CMAKE_SOURCE_DIR}/.." "${SIMUL_PLATFORM_DIR}/.." 
							SOURCES ${shaders} DEFINES VULKAN_SDK=${VULKAN_SDK_DIR} CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} 
							OPTIONS -GSIMUL_TRUESKY=$<BOOL:${SIMUL_TRUESKY}>
					 		OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME" )