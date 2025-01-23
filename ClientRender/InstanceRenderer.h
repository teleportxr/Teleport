// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/GeometryCache.h"
#include "ClientRender/GeometryDecoder.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/LinkRenderer.h"
#include "ClientRender/CanvasTextRenderer.h"
#include "Common.h"
#include "GeometryCache.h"
#include "Node.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "TeleportAudio/AudioCommon.h"
#include "TeleportAudio/AudioStreamTarget.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportClient/Config.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/SessionClient.h"
#include "client/Shaders/cubemap_constants.sl"
#include "client/Shaders/video_types.sl"
#include <libavstream/surfaces/surface_interface.hpp>
#if _MSC_VER
#include "TeleportAudio/PC_AudioPlayer.h"
#else
#include "TeleportAudio/AndroidAudioPlayer.h"
#endif
#include "TeleportAudio/NetworkPipeline.h"
#include <chrono>

namespace teleport
{
	namespace clientrender
	{
		struct AVSTexture
		{
			virtual ~AVSTexture() = default;
			virtual avs::SurfaceBackendInterface *createSurface() const = 0;
		};
		using AVSTextureHandle = std::shared_ptr<AVSTexture>;

		struct AVSTextureImpl : public clientrender::AVSTexture
		{
			AVSTextureImpl(platform::crossplatform::Texture *t)
				: texture(t)
			{
			}
			platform::crossplatform::Texture *texture = nullptr;
			avs::SurfaceBackendInterface *createSurface() const override;
		};
		//! The generic state of the renderer.
		struct RenderState
		{
			teleport::client::OpenXR *openXR = nullptr;
			teleport::clientrender::LinkRenderer linkRenderer;
			teleport::clientrender::CanvasTextRenderer canvasTextRenderer;
			std::shared_ptr<clientrender::FontAtlas> commonFontAtlas;
#ifdef __ANDROID__
			bool multiview = true;
#else
			bool multiview = false;
#endif
			avs::uid show_only = 0;
			avs::uid selected_uid = 0;
			avs::uid selected_cache = 0;
			static constexpr int maxTagDataSize = 32;
			platform::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
			/// A framebuffer to store the colour and depth textures for the view.
			platform::crossplatform::Framebuffer *hdrFramebuffer = nullptr;
			/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
			platform::crossplatform::HdrRenderer *hDRRenderer = nullptr;
			// A simple example mesh to draw as transparent
			std::shared_ptr<platform::crossplatform::Effect> pbrEffect = nullptr;

			platform::crossplatform::EffectTechnique *solid = nullptr;
			platform::crossplatform::EffectTechnique *transparent = nullptr;

			uint64_t shaderValidity = 1;
			platform::crossplatform::EffectVariantPass *solidVariantPass = nullptr;
			platform::crossplatform::EffectVariantPass *transparentVariantPass = nullptr;

			std::shared_ptr<platform::crossplatform::Effect> cubemapClearEffect = nullptr;
			platform::crossplatform::ShaderResource _RWTagDataIDBuffer;
			platform::crossplatform::ShaderResource _lights;
			platform::crossplatform::ShaderResource plainTexture;
			platform::crossplatform::ShaderResource RWTextureTargetArray;
			platform::crossplatform::ShaderResource cubemapClearEffect_TagDataIDBuffer;
			platform::crossplatform::ShaderResource pbrEffect_TagDataIDBuffer;
			platform::crossplatform::ShaderResource pbrEffect_specularCubemap, pbrEffect_diffuseCubemap;
			platform::crossplatform::ShaderResource pbrEffect_diffuseTexture;
			platform::crossplatform::ShaderResource pbrEffect_normalTexture;
			platform::crossplatform::ShaderResource pbrEffect_combinedTexture;
			platform::crossplatform::ShaderResource pbrEffect_emissiveTexture;
			platform::crossplatform::ShaderResource pbrEffect_globalIlluminationTexture;
			platform::crossplatform::ShaderResource cubemapClearEffect_TagDataCubeBuffer;

			platform::crossplatform::ConstantBuffer<CameraConstants, platform::crossplatform::ResourceUsageFrequency::FEW_PER_FRAME> cameraConstants;
			platform::crossplatform::ConstantBuffer<StereoCameraConstants, platform::crossplatform::ResourceUsageFrequency::FEW_PER_FRAME> stereoCameraConstants;
			platform::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
			platform::crossplatform::ConstantBuffer<TeleportSceneConstants, platform::crossplatform::ResourceUsageFrequency::FEW_PER_FRAME> teleportSceneConstants;
			platform::crossplatform::ConstantBuffer<PerNodeConstants> perNodeConstants;
			platform::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
			platform::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;

			vec3 controller_dir;
			vec3 view_dir;
			// In local frame for an instance:
			vec3 current_controller_dir;
			vec3 current_view_dir;
			avs::uid nearest_link_cache_uid=0;
			avs::uid nearest_link_uid=0;
			float nearest_link_distance=1e10f;
			
			avs::uid next_nearest_link_cache_uid=0;
			avs::uid next_nearest_link_uid=0;
			float next_nearest_link_distance=1e10f;
			platform::crossplatform::SamplerState *cubeSamplerState=nullptr;
			platform::crossplatform::SamplerState *wrapSamplerState = nullptr;
			platform::crossplatform::SamplerState *clampSamplerState = nullptr;
			platform::crossplatform::SamplerState *samplerStateNearest = nullptr;
		};
		//! API objects that are per-server.
		//! There exists one of these for each server, plus one for the null server (local state).
		struct InstanceRenderState
		{
			AVSTextureHandle avsTexture;
			platform::crossplatform::Texture *diffuseCubemapTexture = nullptr;
			platform::crossplatform::Texture *specularCubemapTexture = nullptr;
			platform::crossplatform::Texture *lightingCubemapTexture = nullptr;
			platform::crossplatform::Texture *videoTexture = nullptr;
		};
		//! Renderer that draws for a specific server.
		//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
		class InstanceRenderer : public teleport::client::SessionCommandInterface
		{
		protected:
			avs::uid server_uid = 0;
			clientrender::RenderPlatform *clientRenderPlatform = nullptr;
			platform::crossplatform::RenderPlatform *renderPlatform = nullptr;
			teleport::client::SessionClient *sessionClient = nullptr;
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

			virtual avs::DecoderBackendInterface *CreateVideoDecoder()
			{
				return nullptr;
			}
			void ApplyModelMatrix(platform::crossplatform::GraphicsDeviceContext &deviceContext, const mat4 &model);

		public:
			std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;
			VideoTagDataCube videoTagDataCube[RenderState::maxTagDataSize];
			bool videoPosDecoded = false;
			vec3 videoPos;
			unsigned long long receivedInitialPos = 0;
			InstanceRenderState &GetInstanceRenderState()
			{
				return instanceRenderState;
			}
			struct SkeletonRender
			{
				platform::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
				~SkeletonRender()
				{
					boneMatrices.InvalidateDeviceObjects();
				}
			};
			phmap::flat_hash_map<avs::uid, std::shared_ptr<SkeletonRender>> skeletonRenders;
			struct MeshRender
			{
				const mat4 *model;
				avs::uid cache_uid;
				std::shared_ptr<clientrender::Material> material;
				std::shared_ptr<clientrender::Mesh> mesh;
				std::shared_ptr<clientrender::Node> node;
				avs::uid gi_texture_id;
				bool setBoneConstantBuffer;
				bool clockwise;
				uint16_t element;
				std::weak_ptr<clientrender::SkeletonInstance> skeletonInstance;
			};
			struct MeshInstanceRender
			{
				const mat4 *model;
			};
			struct InstancedRender
			{
				mutable std::shared_ptr<platform::crossplatform::Buffer> instanceBuffer;
				phmap::flat_hash_map<clientrender::Node*,MeshInstanceRender> meshInstanceRenders;
				mutable bool valid=false;
				avs::uid cache_uid;
				std::shared_ptr<clientrender::Material> material;
				std::shared_ptr<clientrender::Mesh> mesh;
				avs::uid gi_texture_id;
				bool clockwise;
				uint16_t element;
			};
			struct MaterialRender
			{
				std::shared_ptr<clientrender::Material> material;
				phmap::flat_hash_map<uint64_t, std::shared_ptr<MeshRender>> meshRenders;
				// key is combo of cache_uid,gi_texture_id, element
				phmap::flat_hash_map<uint64_t, std::shared_ptr<InstancedRender>> instancedRenders;
			};
			// Group everything that uses a given pass together.
			struct PassRender
			{
				// Materials within the pass.
				phmap::flat_hash_map<avs::uid, std::shared_ptr<MaterialRender>> materialRenders;
				// Each pass is associated with a specific layout.
				std::shared_ptr<platform::crossplatform::Layout> layout;
			};
			phmap::flat_hash_map<uint64_t, std::shared_ptr<LinkRender>> linkRenders;
			phmap::flat_hash_map<uint64_t, std::shared_ptr<CanvasRender>> canvasRenders;
			
			phmap::flat_hash_map<platform::crossplatform::EffectPass *, std::shared_ptr<PassRender>> passRenders;
			mutable std::mutex passRenders_mutex;

			struct NodeRender
			{
				std::set<platform::crossplatform::EffectPass *> storedPasses;
				std::set<avs::uid> materials;
				std::set <uint64_t> node_element_hashes;
			};
			// Given any node, we can find where it is in the passRenders.
			phmap::flat_hash_map<Node *, std::shared_ptr<NodeRender>> nodeRenders;
		public:
			InstanceRenderer(avs::uid server, teleport::client::Config &config, GeometryDecoder &geometryDecoder, RenderState &renderState, teleport::client::SessionClient *sessionClient);
			virtual ~InstanceRenderer();
			void RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();

			void UpdateMouse(vec3 orig, vec3 dir,float &distance,std::string &url);

			void RenderBackgroundTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void RenderVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture *srcTexture, platform::crossplatform::Texture *targetTexture, const char *technique, const char *shaderTexture);
			void RecomposeVideoTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture *srcTexture, platform::crossplatform::Texture *targetTexture, const char *technique);
			void RecomposeCubemap(platform::crossplatform::GraphicsDeviceContext &deviceContext, platform::crossplatform::Texture *srcTexture, platform::crossplatform::Texture *targetTexture, int mips, int2 sourceOffset);
			virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			static void ApplyCameraMatrices(platform::crossplatform::GraphicsDeviceContext &deviceContext,RenderState &renderState);
			void ApplySceneMatrices(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void ApplyMaterialConstants(platform::crossplatform::GraphicsDeviceContext &deviceContext, std::shared_ptr<clientrender::Material> material);

			void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext &deviceContext);

			// Updates prior to rendering.
			void UpdateGeometryCacheForRendering(platform::crossplatform::GraphicsDeviceContext &deviceContext, std::shared_ptr<clientrender::GeometryCache> geometryCache);
			void UpdateNodeForRendering(platform::crossplatform::GraphicsDeviceContext &deviceContext, const std::shared_ptr<clientrender::GeometryCache> &g, const std::shared_ptr<clientrender::Node> node, bool include_children, bool transparent_pass);
			
			// Render everything that uses a given pass:
			void RenderPass(platform::crossplatform::GraphicsDeviceContext &deviceContext, PassRender &p, platform::crossplatform::EffectPass *pass);
			void RenderLink(platform::crossplatform::GraphicsDeviceContext &deviceContext,  LinkRender &l);
			void RenderMaterial(platform::crossplatform::GraphicsDeviceContext &deviceContext, const MaterialRender &materialRender);
			void RenderMesh(platform::crossplatform::GraphicsDeviceContext &deviceContext, const MeshRender &meshRender);
			void RenderInstancedMeshes(platform::crossplatform::GraphicsDeviceContext &deviceContext, const InstancedRender &instancedRender);
			void RenderTextCanvas(platform::crossplatform::GraphicsDeviceContext &deviceContext,  const CanvasRender *canvasRender);
			void RenderNodeOverlay(platform::crossplatform::GraphicsDeviceContext &deviceContext, const std::shared_ptr<clientrender::GeometryCache> &g, const std::shared_ptr<clientrender::Node> node, bool include_children);

			std::shared_ptr<clientrender::GeometryCache> geometryCache;

			virtual avs::DecoderStatus GetVideoDecoderStatus() { return avs::DecoderStatus::DecoderUnavailable; }

			void UpdateTagDataBuffers(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void OnReceiveVideoTagData(const uint8_t *data, size_t dataSize);
			// Implement SessionCommandInterface
			std::vector<avs::uid> GetGeometryResources() override;
			void ClearGeometryResources() override;
			bool OnNodeEnteredBounds(avs::uid nodeID) override;
			bool OnNodeLeftBounds(avs::uid nodeID) override;

			void UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &updateNodeStructureCommand) override;
			void AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &updateNodeSubtypeCommand, const std::string &regexPath) override;

			void SetVisibleNodes(const std::vector<avs::uid> &visibleNodes) override;
			void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate> &updateList) override;
			void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState> &updateList) override;
			void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
			void UpdateNodeAnimation(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate) override;
			bool OnSetupCommandReceived(const char *server_ip, const teleport::core::SetupCommand &setupCommand) override;
			bool GetHandshake( teleport::core::Handshake& handshake) override;
			void OnVideoStreamClosed() override;
			void OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand &reconfigureVideoCommand) override;
			void OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions) override;
			void SetOrigin(unsigned long long ctr, avs::uid origin_uid) override;
			void OnStreamingControlMessage(const std::string &str) override;

			void AddNodeToInstanceRender(avs::uid cache_uid, avs::uid node_uid);
			void RemoveNodeFromInstanceRender(avs::uid cache_uid, avs::uid node_uid);
			void UpdateNodeInInstanceRender(avs::uid cache_uid, avs::uid node_uid);
			void AddNodeMeshToInstanceRender(avs::uid cache_uid, std::shared_ptr<Node> node, const std::shared_ptr<clientrender::Mesh> mesh);
			void UpdateNodeRenders();
		};
	}
}