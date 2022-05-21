cmake_minimum_required( VERSION 3.8 )
project( ClientRender )

# Build options
set(DEBUG_CONFIGURATIONS Debug)

# Source
set( src_root
	Common.h
	Common.cpp
	DeviceContext.h
	Effect.h
	FrameBuffer.h
	IndexBuffer.h
	RenderPlatform.h
	Sampler.h
	Shader.h
	ShaderStorageBuffer.h
	Texture.cpp
	Texture.h
	UniformBuffer.cpp
	UniformBuffer.h
	VertexBuffer.h
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
	ShaderResource.cpp
	ShaderResource.h
	ShaderSystem.cpp
	ShaderSystem.h
	Skin.cpp
	Skin.h
	Transform.cpp
	Transform.h
	MemoryUtil.cpp
	MemoryUtil.h
	Tests.cpp
	Tests.h
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

	
source_group("Source Files\\Node Components" src FILES ${src_node_components})
source_group("Header Files\\Node Components" src FILES ${hdr_node_components})


set( src_public ${src_root} ${src_node_components} ${hdr_node_components} )
message ( "src_public ${src_public}" )

if(ANDROID)
	add_subdirectory(../thirdparty/draco
			${CMAKE_BINARY_DIR}/draco_build)
	include_directories("${CMAKE_BINARY_DIR}" ../thirdparty/draco)
endif()

#For MT runtime
add_static_library(ClientRender SOURCES ${src_public} )  
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
target_include_directories(ClientRender PRIVATE ../TeleportClient)
#Include draco
target_include_directories(ClientRender PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(ClientRender PUBLIC ../thirdparty/draco/src)
set_target_properties(ClientRender PROPERTIES FOLDER Client)
target_link_libraries(ClientRender draco TeleportClient)

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