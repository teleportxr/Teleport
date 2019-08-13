cmake_minimum_required( VERSION 3.8 )
project( SimulCasterRenderer )

# Build options
set(DEBUG_CONFIGURATIONS Debug)
# Source
set( src_root
	src/Common.h
	)
set( src_api
	src/api/FrameBuffer.h
	src/api/IndexBuffer.h
	src/api/Pipeline.h
	src/api/RenderPlatform.h
	src/api/Sampler.h
	src/api/Shader.h
	src/api/Texture.h
	src/api/UniformBuffer.h
	src/api/VertexBuffer.h
	src/api/VertexBufferLayout.h
	)
set( src_crossplatform
	src/crossplatform/API.cpp
	src/crossplatform/API.h
	src/crossplatform/basic_linear_algebra.h
	src/crossplatform/Camera.cpp
	src/crossplatform/Camera.h
	src/crossplatform/DescriptorSet.cpp
	src/crossplatform/DescriptorSet.h
	src/crossplatform/GeometryDecoder.cpp
	src/crossplatform/GeometryDecoder.h
	src/crossplatform/Light.cpp
	src/crossplatform/Light.h
	src/crossplatform/Material.cpp
	src/crossplatform/Material.h
	src/crossplatform/Object.cpp
	src/crossplatform/Object.h
	src/crossplatform/Renderer.cpp
	src/crossplatform/Renderer.h
	src/crossplatform/ResourceCreator.cpp
	src/crossplatform/ResourceCreator.h
	src/crossplatform/ResourceManager.h
	)

#Include its root directory
include_directories(src)
#Include libavstream
include_directories(../libavstream/include)

if(WIN32)
	#Include Simul API
	include_directories(../pc_client/SCR_Class_PC_Impl) 
elseif(ANDROID)
	#Include OpenGL ES 3.0
	include_directories(../client/VrApi/Include) 
	include_directories(../client/VrAppFramework/Include) 
	include_directories(../client/LibOVRKernel/Src)
endif()

set( src_public ${src_root} ${src_api} ${src_crossplatform} )

#source_group("src" src FILES ${src_root} )
#source_group("src\\api" src FILES ${src_api} )
#source_group("src\\crossplatform" src FILES ${src_crossplatform} )

#For MT runtime
add_static_library( SimulCasterRenderer STATIC ${src_public} )  

#For MD runtime
#add_library( SimulCasterRenderer STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)