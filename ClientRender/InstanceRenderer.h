// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include <libavstream/surfaces/surface_interface.hpp>
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Node.h"
#include "GeometryCache.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "client/Shaders/cubemap_constants.sl"
#include "client/Shaders/pbr_constants.sl"
#include "client/Shaders/video_types.sl"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/ClientPipeline.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/GeometryDecoder.h"
#include "TeleportClient/Config.h"
#include "TeleportAudio/AudioStreamTarget.h"
#include "TeleportAudio/AudioCommon.h"
#if _MSC_VER
#include "TeleportAudio/PC_AudioPlayer.h"
#else
#include "TeleportAudio/AndroidAudioPlayer.h"
#endif
#include "TeleportAudio/NetworkPipeline.h"

namespace clientrender
{
	struct LocalGlobalPose
	{
		avs::Pose localPose;
		avs::Pose globalPose;
	};
	//! The generic state of the client hardware device e.g. headset, controllers etc.
	//! There exists one of these for each server, plus one for the null server (local state).
	
	struct DebugOptions
	{
		bool showAxes=false;
		bool showStageSpace = false;
		bool useDebugShader = false;
		std::string debugShader;
	};
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
		teleport::client::OpenXR *openXR = nullptr;
		DebugOptions debugOptions;
		avs::uid show_only=0;
		avs::uid selected_uid=0;
		bool show_node_overlays			=true;
		static constexpr int maxTagDataSize = 32;
		platform::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
		/// A framebuffer to store the colour and depth textures for the view.
		platform::crossplatform::Framebuffer	*hdrFramebuffer	=nullptr;
		/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
		platform::crossplatform::HdrRenderer		*hDRRenderer	=nullptr;
		// A simple example mesh to draw as transparent
		platform::crossplatform::Effect *pbrEffect					= nullptr;
		
		platform::crossplatform::EffectTechnique	*solid			=nullptr;
		platform::crossplatform::EffectTechnique	*transparent	=nullptr;

		uint64_t shaderValidity=1;
		platform::crossplatform::EffectVariantPass *solidVariantPass=nullptr;
		platform::crossplatform::EffectVariantPass *transparentVariantPass=nullptr;

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
		platform::crossplatform::ConstantBuffer<PerNodeConstants> perNodeConstants;
		platform::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
		platform::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
		platform::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;
	};
	//! API objects that are per-server.
	struct InstanceRenderState
	{
		AVSTextureHandle avsTexture;
		platform::crossplatform::Texture* diffuseCubemapTexture		= nullptr;
		platform::crossplatform::Texture* specularCubemapTexture	= nullptr;
		platform::crossplatform::Texture* lightingCubemapTexture	= nullptr;
		platform::crossplatform::Texture* videoTexture				= nullptr;
	};
	//! Renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class InstanceRenderer:public teleport::client::SessionCommandInterface
	{
	protected:
		avs::uid server_uid=0;
		clientrender::RenderPlatform *clientRenderPlatform=nullptr;
		platform::crossplatform::RenderPlatform* renderPlatform	= nullptr;
		teleport::client::SessionClient *sessionClient=nullptr;
		RenderState &renderState;
		InstanceRenderState instanceRenderState;
		teleport::client::Config &config;
		GeometryDecoder &geometryDecoder;
		// determined by the stream setup command:
		vec4 colourOffsetScale;
		vec4 depthOffsetScale;
#ifdef _MSC_VER
		teleport::audio::PC_AudioPlayer audioPlayer;
#else
		teleport::audio::AndroidAudioPlayer audioPlayer;
#endif
		std::unique_ptr<teleport::audio::AudioStreamTarget> audioStreamTarget;
		std::unique_ptr<teleport::audio::NetworkPipeline> audioInputNetworkPipeline;
		avs::Queue audioInputQueue;
		static constexpr uint32_t NominalJitterBufferLength = 0;
		static constexpr uint32_t MaxJitterBufferLength = 50;
		
		virtual avs::DecoderBackendInterface* CreateVideoDecoder()
		{
		return nullptr;
		}
	public:
		std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;
		VideoTagDataCube videoTagDataCube[RenderState::maxTagDataSize];
		bool videoPosDecoded=false;
		vec3 videoPos;
		unsigned long long receivedInitialPos = 0;
		InstanceRenderState &GetInstanceRenderState()
		{
			return instanceRenderState;
		}
	public:
		InstanceRenderer(avs::uid server,teleport::client::Config &config,GeometryDecoder &geometryDecoder,RenderState &renderState,teleport::client::SessionClient *sessionClient);
		virtual ~InstanceRenderer();
		void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
		void InvalidateDeviceObjects();
		
		void RenderVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext,platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture);
		void RecomposeVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique);
		void RecomposeCubemap(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);
		virtual void RenderView(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,avs::uid this_server_uid);
			
		void RenderGeometryCache(platform::crossplatform::GraphicsDeviceContext& deviceContext,std::shared_ptr<clientrender::GeometryCache> geometryCache);
		void RenderNode(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,const std::shared_ptr<clientrender::GeometryCache> &g
			,const std::shared_ptr<clientrender::Node> node
			,bool force
			,bool include_children
			,bool transparent_pass);

		void RenderTextCanvas(platform::crossplatform::GraphicsDeviceContext& deviceContext,const std::shared_ptr<TextCanvas> textCanvas);
		void RenderBone(platform::crossplatform::GraphicsDeviceContext& deviceContext,const mat4 &model_matrix,const std::shared_ptr<clientrender::Bone> bone);
		void RenderNodeOverlay(platform::crossplatform::GraphicsDeviceContext& deviceContext
			,const std::shared_ptr<clientrender::Node> node
			,bool force=false);
		
		std::shared_ptr<clientrender::GeometryCache> geometryCache;
		clientrender::ResourceCreator resourceCreator;
		
		virtual avs::DecoderStatus GetVideoDecoderStatus() { return avs::DecoderStatus::DecoderUnavailable; }
		
		void UpdateTagDataBuffers(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
		// Implement SessionCommandInterface
		std::vector<avs::uid> GetGeometryResources() override;
		void ClearGeometryResources() override;
		bool OnNodeEnteredBounds(avs::uid nodeID) override;
		bool OnNodeLeftBounds(avs::uid nodeID) override;
	
		void UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand& updateNodeStructureCommand) override;
		void AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &updateNodeSubtypeCommand,const std::string &regexPath) override;

		void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;
		void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList) override;
		void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList) override;
		void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
		void UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate) override;
	//	void UpdateNodeAnimationControl(const teleport::core::NodeUpdateAnimationControl& animationControlUpdate) override;
		void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;
		bool OnSetupCommandReceived(const char* server_ip, const teleport::core::SetupCommand &setupCommand, teleport::core::Handshake& handshake) override;
		void OnVideoStreamClosed() override;
		void OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand& reconfigureVideoCommand) override;
		void OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition>& inputDefinitions) override;
		void SetOrigin(unsigned long long ctr,avs::uid oorigin_uid) override;
		void OnStreamingControlMessage(const std::string& str) override;
	};
}