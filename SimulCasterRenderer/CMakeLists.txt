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
	src/api/Texture.cpp
	src/api/Texture.h
	src/api/UniformBuffer.cpp
	src/api/UniformBuffer.h
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
	src/crossplatform/GeometryCache.cpp
	src/crossplatform/GeometryCache.h
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
	src/crossplatform/NodeManager.cpp
	src/crossplatform/NodeManager.h
	src/crossplatform/ResourceCreator.cpp
	src/crossplatform/ResourceCreator.h
	src/crossplatform/ResourceManager.h
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

set(src_node_components
	src/crossplatform/NodeComponents/AnimationComponent.cpp
	src/crossplatform/NodeComponents/AnimationState.cpp
	src/crossplatform/NodeComponents/VisibilityComponent.cpp
)

set(hdr_node_components
	src/crossplatform/NodeComponents/AnimationComponent.h
	src/crossplatform/NodeComponents/AnimationState.h
	src/crossplatform/NodeComponents/VisibilityComponent.h
)

set(src_testing
	src/crossplatform/Tests.cpp
	src/crossplatform/Tests.h
	)
	
source_group("Source Files\\Node Components" src FILES ${src_node_components})
source_group("Header Files\\Node Components" src FILES ${hdr_node_components})


set( src_public ${src_root} ${src_api} ${src_crossplatform} ${src_node_components} ${hdr_node_components} ${src_testing})
message ( "src_public ${src_public}" )

if(ANDROID)
	add_subdirectory(../thirdparty/draco
			${CMAKE_BINARY_DIR}/draco_build)
	include_directories("${CMAKE_BINARY_DIR}" ../thirdparty/draco)
endif()
#For MT runtime
add_static_library(SimulCasterRenderer SOURCES ${src_public} )  

#Include its root directory
target_include_directories(SimulCasterRenderer PRIVATE src)
#Include libavstream
target_include_directories(SimulCasterRenderer PRIVATE ../libavstream/include)
#Include Basis Universal
target_include_directories(SimulCasterRenderer PRIVATE ../thirdparty/basis_universal)
#Include stb
target_include_directories(SimulCasterRenderer PRIVATE ../firstparty/Platform/External/stb)
#Include enet
target_include_directories(SimulCasterRenderer PRIVATE ../thirdparty/enet/include)
target_include_directories(SimulCasterRenderer PRIVATE ../TeleportClient)
#Include draco
target_include_directories(SimulCasterRenderer PUBLIC ${CMAKE_BINARY_DIR})
target_include_directories(SimulCasterRenderer PUBLIC ../thirdparty/draco/src)
set_target_properties(SimulCasterRenderer PROPERTIES FOLDER Client)
target_link_libraries(SimulCasterRenderer draco)

if(WIN32)
# Removed circular dependency
elseif(ANDROID)
	#Include OpenGL ES 3.0
	target_include_directories(SimulCasterRenderer ../client/VrApi/Include) 
	target_include_directories(SimulCasterRenderer ../client/VrAppFramework/Include) 
	target_include_directories(SimulCasterRenderer ../client/LibOVRKernel/Src)
	target_link_libraries( SimulCasterRenderer draco)
			# Links the target library to the log library
			# included in the NDK.
			#${log-lib} )
endif()

#For MD runtime
#add_library( SimulCasterRenderer STATIC ${src_public} ) 

#target_compile_features(libavstream PRIVATE cxx_std_17)