// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include <libavstream/src/platform.hpp>
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/camera_constants.sl"
#include "TeleportClient/basic_linear_algebra.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportClient/OpenXR.h"
#include "TeleportClient/ClientDeviceState.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/ResourceCreator.h"
#include "ClientRender/GeometryDecoder.h"
#include "client/Shaders/cubemap_constants.sl"
#include "client/Shaders/pbr_constants.sl"
#include "client/Shaders/video_types.sl"
#include "ClientRender/Gui.h"
#include "TeleportAudio/AudioStreamTarget.h"
#include "TeleportAudio/AudioCommon.h"
#ifdef _MSC_VER
#include "TeleportAudio/PC_AudioPlayer.h"
#endif
#include "TeleportAudio/NetworkPipeline.h"
#include "TeleportClient/Config.h"
#include "VideoDecoderBackend.h"

namespace teleport
{
	namespace client
	{
		class ClientDeviceState;
	}
}
namespace clientrender
{
	/// <summary>
	/// A 
	/// </summary>
	struct AVSTexture
	{
		virtual ~AVSTexture() = default;
		virtual avs::SurfaceBackendInterface* createSurface() const = 0;
	};
	using AVSTextureHandle = std::shared_ptr<AVSTexture>;

	struct RendererStats
	{
		uint64_t frameCounter;
		double lastFrameTime;
		double lastFPS;
	};

	enum
	{
		NO_OSD=0,
		CAMERA_OSD,
		VIDEO_OSD,
		NETWORK_OSD,
		GEOMETRY_OSD,
		TEXTURES_OSD,
		TAG_OSD,
		CONTROLLER_OSD,
		NUM_OSDS
	};
	enum class ShaderMode
	{
		DEFAULT,PBR, ALBEDO, NORMAL_UNSWIZZLED, DEBUG_ANIM, LIGHTMAPS, NORMAL_VERTEXNORMALS,NUM
	};
	//! Timestamp of when the system started.
	extern avs::Timestamp platformStartTimestamp;	
	//! Base class for a renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class Renderer:public teleport::client::SessionCommandInterface,public platform::crossplatform::PlatformRendererInterface
	{
	public:
		Renderer(teleport::client::ClientDeviceState *c,clientrender::NodeManager *localNodeManager
				,clientrender::NodeManager *remoteNodeManager
				,teleport::client::SessionClient *sessionClient
				, teleport::Gui &g,teleport::client::Config &config);
		virtual ~Renderer();
		//! This allows live-recompile of shaders (desktop platforms only).
		void RecompileShaders();
		void SetMinimumPriority(int32_t p)
		{
			minimumPriority=p;
		}
		int32_t GetMinimumPriority() const
		{
			return minimumPriority;
		}

		void ChangePass(ShaderMode newShaderMode);
		void ConfigureVideo(const avs::VideoConfig &vc);
		virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		avs::SetupCommand lastSetupCommand;
		avs::SetupLightingCommand lastSetupLightingCommand;

		float framerate = 0.0f;
		void Update(double timestamp_ms);
	protected:
		void InvalidateDeviceObjects();
		void CreateTexture(clientrender::AVSTextureHandle &th,int width, int height);
		void FillInControllerPose(int index, float offset);
		struct ControllerSim
		{
			vec3 controller_dir;
			vec3 view_dir;
			float angle = 0.0f;
			vec3 pos_offset[2];
			avs::vec3 position[2];
			avs::vec4 orientation[2];
		};
		ControllerSim controllerSim;
	
		/// A camera instance to generate view and proj matrices and handle mouse control.
		/// In practice you will have your own solution for this.
		platform::crossplatform::Camera			camera;
		platform::crossplatform::MouseCameraState	mouseCameraState;
		platform::crossplatform::MouseCameraInput	mouseCameraInput;
		void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext& deviceContext,avs::uid server_uid,clientrender::GeometryCache &g);
		virtual void RenderNode(platform::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force=false);
		void RenderNodeOverlay(platform::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force=false);
		
		clientrender::RenderPlatform PcClientRenderPlatform;
		/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
		/// distributes numerical precision to where it is better used.
		static const bool reverseDepth = true;
		platform::crossplatform::ConstantBuffer<CubemapConstants> cubemapConstants;
		platform::crossplatform::ConstantBuffer<PbrConstants> pbrConstants;
		platform::crossplatform::ConstantBuffer<BoneMatrices> boneMatrices;
		platform::crossplatform::StructuredBuffer<VideoTagDataCube> tagDataCubeBuffer;
		platform::crossplatform::StructuredBuffer<PbrLight> lightsBuffer;
		static constexpr int maxTagDataSize = 32;
		VideoTagDataCube videoTagDataCube[maxTagDataSize];

		std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;

		// determined by the stream setup command:
		vec4 colourOffsetScale;
		vec4 depthOffsetScale;

		bool keydown[256] = {};
		teleport::client::SessionClient *sessionClient=nullptr;
	
		/// A pointer to RenderPlatform, so that we can use the platform::crossplatform API.
		platform::crossplatform::RenderPlatform *renderPlatform	=nullptr;
		/// A framebuffer to store the colour and depth textures for the view.
		platform::crossplatform::BaseFramebuffer	*hdrFramebuffer	=nullptr;
		/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
		platform::crossplatform::HdrRenderer		*hDRRenderer	=nullptr;

		// A simple example mesh to draw as transparent
		platform::crossplatform::Effect *pbrEffect				= nullptr;
		platform::crossplatform::Effect *cubemapClearEffect	= nullptr;
		platform::crossplatform::ShaderResource _RWTagDataIDBuffer;
		platform::crossplatform::ShaderResource _lights;
		platform::crossplatform::ConstantBuffer<CameraConstants> cameraConstants;
		platform::crossplatform::StructuredBuffer<uint4> tagDataIDBuffer;
		platform::crossplatform::Texture* diffuseCubemapTexture	= nullptr;
		platform::crossplatform::Texture* specularCubemapTexture	= nullptr;
		platform::crossplatform::Texture* lightingCubemapTexture	= nullptr;
		platform::crossplatform::Texture* videoTexture				= nullptr;
		int show_osd = NO_OSD;
		double previousTimestamp=0.0;
		int32_t minimumPriority=0;
		bool using_vr = true;
		clientrender::GeometryCache localGeometryCache;
		clientrender::ResourceCreator localResourceCreator;
		clientrender::GeometryCache geometryCache;
		clientrender::ResourceCreator resourceCreator;
		avs::uid local_left_hand_uid=0;
		clientrender::Transform palm_to_hand_l;
		avs::uid local_right_hand_uid=0;
		clientrender::Transform palm_to_hand_r;
		avs::uid hand_skin_uid=0;
		vec3 index_finger_offset;

		int RenderMode=0;
		std::shared_ptr<clientrender::Material> mFlatColourMaterial;
		unsigned long long receivedInitialPos = 0;
		unsigned long long receivedRelativePos = 0;
		bool videoPosDecoded=false;
		vec3 videoPos;

		avs::vec3 bodyOffsetFromHead; //Offset of player body from head pose.

		static constexpr float HFOV = 90;
		float gamma=0.44f;

		bool have_vr_device = false;
		platform::crossplatform::Texture* externalTexture = nullptr;
		teleport::client::OpenXR *openXR=nullptr;
		avs::uid show_only=0;
		
		std::string server_ip;
		int server_discovery_port=0;
		// TODO: temporary.
		avs::uid server_uid=1;
		const avs::uid local_server_uid=0;
		const avs::InputId local_menu_input_id=0;
		const avs::InputId local_cycle_osd_id=1;
		const avs::InputId local_cycle_shader_id=2;
	
		bool show_video = false;

		bool render_from_video_centre	= false;
		bool show_textures				= false;
		bool show_cubemaps				=false;
		bool show_node_overlays			=false;

		teleport::client::ClientDeviceState *clientDeviceState = nullptr;
		std::string overridePassName = ""; //Pass used for rendering geometry.

		teleport::Gui &gui;
		teleport::client::Config &config;
		ShaderMode shaderMode=ShaderMode::PBR;
		void GeometryOSD(const clientrender::GeometryCache &geometryCache);
	public:
		teleport::client::ClientPipeline clientPipeline;
		std::unique_ptr<sca::AudioStreamTarget> audioStreamTarget;
#ifdef _MSC_VER
		sca::PC_AudioPlayer audioPlayer;
#endif
		std::unique_ptr<sca::NetworkPipeline> audioInputNetworkPipeline;
		avs::Queue audioInputQueue;
		static constexpr bool AudioStream	= true;
		static constexpr bool GeoStream		= true;
		static constexpr uint32_t NominalJitterBufferLength = 0;
		static constexpr uint32_t MaxJitterBufferLength = 50;
		//static constexpr avs::SurfaceFormat SurfaceFormat = avs::SurfaceFormat::ARGB;
		AVSTextureHandle avsTexture;

		GeometryDecoder geometryDecoder;
	
		void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
		void UpdateTagDataBuffers(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void RecomposeVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique);
		void RenderVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture, const platform::math::Matrix4x4& invCamMatrix);
		void RecomposeCubemap(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);
		
		bool OnDeviceRemoved();
		void OnFrameMove(double fTime, float time_step, bool have_headset);
		void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
		void OnMouseMove(int xPos, int yPos,bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);
		
		void WriteHierarchy(int tab,std::shared_ptr<clientrender::Node> node);
		void WriteHierarchies();
		
		int AddView() override;
		void ResizeView(int view_id, int W, int H) override;
		void Render(int view_id,void* pContext,void* renderTexture,int w,int h,long long frame,void* context_allocator=nullptr) override
		{
			RenderDesktopView(view_id,pContext,renderTexture,w,h,frame,context_allocator);
		}
		void RenderDesktopView(int view_id,void* pContext,void* renderTexture,int w,int h, long long frame, void* context_allocator = nullptr);
		void Init(platform::crossplatform::RenderPlatform *r,teleport::client::OpenXR *u,teleport::PlatformWindow* active_window);
		void SetServer(const char* ip_port);
		void RemoveView(int) override;
		void DrawOSD(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		// handler for the UI to tell us to connect.
		void ConnectButtonHandler(const std::string& url);
		void CancelConnectButtonHandler();
		
		std::vector<avs::uid> GetGeometryResources() override;
		void ClearGeometryResources() override;
		// to render the vr view instead of re-rendering.
		void SetExternalTexture(platform::crossplatform::Texture* t);
		void PrintHelpText(platform::crossplatform::GraphicsDeviceContext& deviceContext);

		bool OnNodeEnteredBounds(avs::uid nodeID) override;
		bool OnNodeLeftBounds(avs::uid nodeID) override;
	
		void UpdateNodeStructure(const avs::UpdateNodeStructureCommand& updateNodeStructureCommand) override;
		void UpdateNodeSubtype(const avs::UpdateNodeSubtypeCommand &updateNodeSubtypeCommand,const std::string &regexPath) override;

		void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;
		void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList) override;
		void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList) override;
		void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
		void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate) override;
		void UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate) override;
		void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;
		
		virtual avs::DecoderBackendInterface* CreateVideoDecoder();
		// Implement SessionCommandInterface
		bool OnSetupCommandReceived(const char* server_ip, const avs::SetupCommand &setupCommand, avs::Handshake& handshake) override;
		void OnVideoStreamClosed() override;
		void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) override;
		void OnLightingSetupChanged(const avs::SetupLightingCommand &l) override;
		void OnInputsSetupChanged(const std::vector<avs::InputDefinition>& inputDefinitions) override;

		void HandleLocalInputs(const teleport::core::Input& local_inputs);
		void ShowHideGui();
	};
}