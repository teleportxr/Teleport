cmake_minimum_required( VERSION 3.8 )
project( ClientRender )

# Build options
set(DEBUG_CONFIGURATIONS Debug)

# Source
set( src_root
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
	Bone.cpp
	Bone.h
	Camera.cpp
	Camera.h
	GeometryCache.cpp
	GeometryCache.h
	GeometryDecoder.cpp
	GeometryDecoder.h
	InstanceRenderer.cpp
	InstanceRenderer.h
	Gui.cpp
	Gui.h
	Light.cpp
	Light.h
	Material.cpp
	Material.h
	Mesh.cpp
	Mesh.h
	Node.cpp
	Node.h
	NodeManager.cpp
	NodeManager.h
	ResourceCreator.cpp
	ResourceCreator.h
	ResourceManager.h
	Renderer.h
	Renderer.cpp
	RenderPlatform.h
	RenderPlatform.cpp
	ShaderResource.cpp
	ShaderResource.h
	Skin.cpp
	Skin.h
	SkinInstance.cpp
	SkinInstance.h
	Transform.cpp
	Transform.h
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
	NodeComponents/VisibilityComponent.cpp
)

set(hdr_node_components
	NodeComponents/AnimationComponent.h
	NodeComponents/AnimationState.h
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
	
source_group("Source Files\\Node Components" src FILES ${src_node_components})
source_group("Header Files\\Node Components" src FILES ${hdr_node_components})
source_group("AVParser" src FILES ${av_parser} )

set( src_public ${src_root} ${src_node_components} ${hdr_node_components} )
message ( "src_public ${src_public}" )

if(ANDROID)
	add_subdirectory(../thirdparty/draco
			${CMAKE_BINARY_DIR}/draco_build)
	include_directories("${CMAKE_BINARY_DIR}" ../thirdparty/draco)
endif()

#For MT runtime
add_static_library(ClientRender SOURCES ${src_public} ${av_parser} )  
SetTeleportDefaults(ClientRender)
#Include its root directory
target_include_directories(ClientRender PRIVATE src)
#Include libavstream
target_include_directories(ClientRender PRIVATE ../libavstream/include)
#Include Basis Universal
target_include_directories(ClientRender PRIVATE ../thirdparty/basis_universal)
#Include stb
target_include_directories(ClientRender PRIVATE ../firstparty/Platform/External/stb)
#Include enet
target_include_directories(ClientRender PRIVATE ../thirdparty/enet/include)
target_include_directories(ClientRender PRIVATE ${SIMUL_PLATFORM_DIR}/External/imgui)
#Include draco
target_include_directories(ClientRender PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(ClientRender PUBLIC ../thirdparty/draco/src)
set_target_properties(ClientRender PROPERTIES FOLDER Client)
target_link_libraries(ClientRender draco TeleportClient)
if(TELEPORT_CLIENT_USE_D3D12)
	target_link_libraries(ClientRender SimulDirectX12_MT)
else()
	target_link_libraries(ClientRender SimulDirectX11_MT)
	target_link_libraries(ClientRender d3d12)
endif()
target_compile_definitions(ClientRender PUBLIC TELEPORT_CLIENT_USE_D3D12=$<BOOL:${TELEPORT_CLIENT_USE_D3D12}> TELEPORT_CLIENT_USE_D3D11=$<BOOL:${TELEPORT_CLIENT_USE_D3D11}> TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER=$<BOOL:${TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER}>)

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

#For MD runtime
#add_library( ClientRender STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)