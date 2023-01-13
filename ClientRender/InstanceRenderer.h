// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include <libavstream/surfaces/surface_interface.hpp>
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Node.h"
#include "GeometryCache.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/camera_constants.sl"
#include "client/Shaders/cubemap_constants.sl"
#include "client/Shaders/pbr_constants.sl"
#include "client/Shaders/video_types.sl"
#include "TeleportClient/OpenXR.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/ResourceCreator.h"

namespace clientrender
{
	struct AVSTexture
	{
		virtual ~AVSTexture() = default;
		virtual avs::SurfaceBackendInterface* createSurface() const = 0;
	};
	using AVSTextureHandle = std::shared_ptr<AVSTexture>;

	struct AVSTextureImpl:public clientrender::AVSTexture
	{
		AVSTextureImpl(platform::crossplatform::Texture *t)
			:texture(t)
		{
		}
		platform::crossplatform::Texture *texture = nullptr;
		avs::SurfaceBackendInterface* createSurface() const override;
	};
	struct RenderState
	{
		teleport::client::OpenXR *openXR=nullptr;
		AVSTextureHandle avsTexture;
		avs::uid show_only=0;
		avs::uid selected_uid=0;
		bool show_node_overlays			=false;
		teleport::core::SetupCommand lastSetupCommand;
		teleport::core::SetupLightingCommand lastSetupLightingCommand;
		std::string overridePassName;

		platform::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
		platform::crossplatform::Texture* diffuseCubemapTexture	= nullptr;
		platform::crossplatform::Texture* specularCubemapTexture	= nullptr;
		platform::crossplatform::Texture* lightingCubemapTexture	= nullptr;
		platform::crossplatform::Texture* videoTexture				= nullptr;
		// A simple example mesh to draw as transparent
		platform::crossplatform::Effect *pbrEffect					= nullptr;
		
		platform::crossplatform::EffectTechnique	*pbrEffect_transparentTechnique					=nullptr;
		platform::crossplatform::EffectTechnique	*pbrEffect_transparentMultiviewTechnique		=nullptr;
		platform::crossplatform::EffectTechnique	*pbrEffect_solidTechnique						=nullptr;
		platform::crossplatform::EffectPass			*pbrEffect_solidTechnique_localPass				=nullptr;
		platform::crossplatform::EffectTechnique	*pbrEffect_solidAnimTechnique					=nullptr;
		platform::crossplatform::EffectTechnique	*pbrEffect_solidMultiviewTechnique				=nullptr;
		platform::crossplatform::EffectTechnique	*pbrEffect_solidAnimMultiviewTechnique			=nullptr;
		platform::crossplatform::EffectPass			*pbrEffect_solidMultiviewTechnique_localPass	=nullptr;

		platform::crossplatform::Effect *cubemapClearEffect	= nullptr;
		platform::crossplatform::ShaderResource _RWTagDataIDBuffer;
		platform::crossplatform::ShaderResource _lights;
		platform::crossplatform::ShaderResource plainTexture;
		platform::crossplatform::ShaderResource RWTextureTargetArray;
		platform::crossplatform::ShaderResource cubemapClearEffect_TagDataIDBuffer;
		platform::crossplatform::ShaderResource pbrEffect_TagDataIDBuffer;
		platform::crossplatform::ShaderResource pbrEffect_specularCubemap,pbrEffect_diffuseCubemap;
		platform::crossplatform::ShaderResource pbrEffect_diffuseTexture;
		platform::crossplatform::ShaderResource pbrEffect_normalTexture;
		platform::crossplatform::ShaderResource pbrEffect_combinedTexture;
		platform::crossplatform::ShaderResource pbrEffect_emissiveTexture;
		platform::crossplatform::ShaderResource pbrEffect_globalIlluminationTexture;
		platform::crossplatform::ShaderResource cubemapClearEffect_TagDataCubeBuffer; 
		
		platform::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
		platform::crossplatform::ConstantBuffer<StereoCameraConstants> stereoCameraConstants;
		platform::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
		platform::crossplatform::ConstantBuffer<PbrConstants> pbrConstants;
		platform::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
		platform::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
		platform::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;
	};
	//! Renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class InstanceRenderer
	{
		avs::uid server_uid=0;
		clientrender::RenderPlatform *clientRenderPlatform=nullptr;
	public:
		InstanceRenderer(avs::uid server);
		virtual ~InstanceRenderer();
		void RestoreDeviceObjects(clientrender::RenderPlatform *r);
		void InvalidateDeviceObjects();

		void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,RenderState &renderState
			,avs::uid this_server_uid);

		void RenderNode(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,RenderState &renderState
			,const std::shared_ptr<clientrender::Node>& node
			,bool force=false
			,bool include_children=true);
		void RenderNodeOverlay(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,RenderState &renderState
			,const std::shared_ptr<clientrender::Node>& node
			,bool force=false);
		clientrender::GeometryCache geometryCache;
		clientrender::ResourceCreator resourceCreator;
	};
}