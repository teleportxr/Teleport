cmake_minimum_required( VERSION 3.8 )
project( SimulCasterRenderer )

# Build options
set(DEBUG_CONFIGURATIONS Debug)

# Source
set( src_root
	src/Common.h
	src/Common.cpp
	)
set( src_api
	src/api/DeviceContext.h
	src/api/Effect.h
	src/api/FrameBuffer.h
	src/api/IndexBuffer.h
	src/api/RenderPlatform.h
	src/api/Sampler.h
	src/api/Shader.h
	src/api/ShaderStorageBuffer.h
	src/api/Texture.h
	src/api/UniformBuffer.h
	src/api/UniformBuffer.cpp
	src/api/VertexBuffer.h
	src/api/VertexBufferLayout.h
	)
set( src_crossplatform
	src/crossplatform/Animation.cpp
	src/crossplatform/Animation.h
	src/crossplatform/API.cpp
	src/crossplatform/API.h
	src/crossplatform/Bone.cpp
	src/crossplatform/Bone.h
	src/crossplatform/Camera.cpp
	src/crossplatform/Camera.h
	src/crossplatform/GeometryDecoder.cpp
	src/crossplatform/GeometryDecoder.h
	src/crossplatform/Light.cpp
	src/crossplatform/Light.h
	src/crossplatform/Log.cpp
	src/crossplatform/Log.h
	src/crossplatform/Material.cpp
	src/crossplatform/Material.h
	src/crossplatform/Mesh.cpp
	src/crossplatform/Mesh.h
	src/crossplatform/Node.cpp
	src/crossplatform/Node.h
	src/crossplatform/NodeComponents.cpp
	src/crossplatform/NodeComponents.h
	src/crossplatform/NodeManager.cpp
	src/crossplatform/NodeManager.h
	src/crossplatform/ResourceCreator.cpp
	src/crossplatform/ResourceCreator.h
	src/crossplatform/ResourceManager.h
	src/crossplatform/ServerTimestamp.cpp
	src/crossplatform/ServerTimestamp.h
	src/crossplatform/SessionClient.cpp
	src/crossplatform/SessionClient.h
	src/crossplatform/ShaderResource.cpp
	src/crossplatform/ShaderResource.h
	src/crossplatform/ShaderSystem.cpp
	src/crossplatform/ShaderSystem.h
	src/crossplatform/Skin.cpp
	src/crossplatform/Skin.h
	src/crossplatform/Transform.cpp
	src/crossplatform/Transform.h
	src/crossplatform/MemoryUtil.cpp
	src/crossplatform/MemoryUtil.h
	)

set(src_testing
	src/crossplatform/Tests.cpp
	src/crossplatform/Tests.h
	)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)
#Include Basis Universal
include_directories(../thirdparty/basis_universal)
#Include enet
include_directories(../thirdparty/enet/include)
include_directories(../TeleportClient)

if(WIN32)
# Removed circular dependency
elseif(ANDROID)
	#Include OpenGL ES 3.0
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
	include_directories(../client/LibOVRKernel/Src)
endif()

set( src_public ${src_root} ${src_api} ${src_crossplatform} ${src_testing})
message ( "src_public ${src_public}" )
#source_group("src" src FILES ${src_root} )
#source_group("src\\api" src FILES ${src_api} )
#source_group("src\\crossplatform" src FILES ${src_crossplatform} )

#For MT runtime
add_static_library( SimulCasterRenderer SOURCES ${src_public} )  
set_target_properties( SimulCasterRenderer PROPERTIES FOLDER Client)

#For MD runtime
#add_library( SimulCasterRenderer STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)