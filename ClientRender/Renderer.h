// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include <libavstream/src/platform.hpp>
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/Text3DRenderer.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportClient/ClientDeviceState.h"
#include "ClientRender/GeometryDecoder.h"
#include "ClientRender/Gui.h"
#include "TeleportAudio/AudioStreamTarget.h"
#include "TeleportAudio/AudioCommon.h"
#ifdef _MSC_VER
#include "TeleportAudio/PC_AudioPlayer.h"
#endif
#include "TeleportAudio/NetworkPipeline.h"
#include "TeleportClient/Config.h"
#include "VideoDecoderBackend.h"
#include "InstanceRenderer.h"

namespace teleport
{
	namespace client
	{
		class ClientDeviceState;
	}
}

namespace clientrender
{
	struct RendererStats
	{
		uint64_t frameCounter;
		double lastFrameTime;
		double lastFPS;
	};

	enum class ShaderMode
	{
		DEFAULT,PBR, ALBEDO, NORMAL_UNSWIZZLED, DEBUG_ANIM, LIGHTMAPS, NORMAL_VERTEXNORMALS,NUM
	};
	//! Timestamp of when the system started.
	extern avs::Timestamp platformStartTimestamp;	
	//! Renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class Renderer:public teleport::client::SessionCommandInterface,public platform::crossplatform::RenderDelegaterInterface
	{
	public:
		Renderer(teleport::client::SessionClient *sessionClient
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
		void SetRenderPose(platform::crossplatform::GraphicsDeviceContext& deviceContext, const avs::Pose& originPose);
		virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
	

		float framerate = 0.0f;
		void Update(double timestamp_ms);

		bool OSDVisible() const
		{
			return show_osd;
		}
	protected:
	teleport::client::SessionClient *sessionClient=nullptr;
		std::map<avs::uid,std::shared_ptr<InstanceRenderer>> instanceRenderers;
		//std::map<avs::uid,std::shared_ptr<teleport::client::SessionClient> sessionClients;
		std::shared_ptr<InstanceRenderer> GetInstanceRenderer(avs::uid);
		void RemoveInstanceRenderer(avs::uid);
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
		platform::crossplatform::Camera			camera;
		platform::crossplatform::MouseCameraState	mouseCameraState;
		platform::crossplatform::MouseCameraInput	mouseCameraInput;
		void RenderLocalNodes(platform::crossplatform::GraphicsDeviceContext& deviceContext,avs::uid server_uid);
		//void RenderNode(platform::crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force=false,bool include_children=true);
		
		clientrender::RenderPlatform PcClientRenderPlatform;
		/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
		/// distributes numerical precision to where it is better used.
		static const bool reverseDepth = true;
		static constexpr int maxTagDataSize = 32;
		VideoTagDataCube videoTagDataCube[maxTagDataSize];

		std::vector<clientrender::SceneCaptureCubeTagData> videoTagDataCubeArray;

		// determined by the stream setup command:
		vec4 colourOffsetScale;
		vec4 depthOffsetScale;

		bool keydown[256] = {};
	
		/// A pointer to RenderPlatform, so that we can use the platform::crossplatform API.
		platform::crossplatform::RenderPlatform		*renderPlatform	=nullptr;
		/// A framebuffer to store the colour and depth textures for the view.
		platform::crossplatform::BaseFramebuffer	*hdrFramebuffer	=nullptr;
		/// An HDR Renderer to put the contents of hdrFramebuffer to the screen. In practice you will probably have your own method for this.
		platform::crossplatform::HdrRenderer		*hDRRenderer	=nullptr;

		RenderState renderState;

		platform::crossplatform::Text3DRenderer text3DRenderer;
		bool show_osd = false;
		double previousTimestamp=0.0;
		int32_t minimumPriority=0;
		bool using_vr = true;
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
		//bool show_textures				= false;
		bool show_cubemaps				=false;
	
		teleport::Gui &gui;
		teleport::client::Config &config;
		ShaderMode shaderMode=ShaderMode::PBR;
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
		
		GeometryDecoder geometryDecoder;
	
		void OnReceiveVideoTagData(const uint8_t* data, size_t dataSize);
		void UpdateTagDataBuffers(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		void RecomposeVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique);
		void RenderVideoTexture(platform::crossplatform::GraphicsDeviceContext& deviceContext, avs::uid server_uid,platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture);
		void RecomposeCubemap(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::Texture* srcTexture, platform::crossplatform::Texture* targetTexture, int mips, int2 sourceOffset);
		
		bool OnDeviceRemoved();
		void OnFrameMove(double fTime, float time_step, bool have_headset);
		void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
		void OnMouseMove(int xPos, int yPos,bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);
		
		void WriteHierarchy(int tab,std::shared_ptr<clientrender::Node> node);
		void WriteHierarchies(avs::uid server);
		
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
	
		void UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand& updateNodeStructureCommand) override;
		void UpdateNodeSubtype(const teleport::core::UpdateNodeSubtypeCommand &updateNodeSubtypeCommand,const std::string &regexPath) override;

		void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) override;
		void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList) override;
		void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList) override;
		void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
		void UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate) override;
		void UpdateNodeAnimationControl(const teleport::core::NodeUpdateAnimationControl& animationControlUpdate) override;
		void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) override;
		
		virtual avs::DecoderBackendInterface* CreateVideoDecoder();
		virtual avs::DecoderStatus GetVideoDecoderStatus() { return avs::DecoderStatus::DecoderUnavailable; }
		// Implement SessionCommandInterface
		bool OnSetupCommandReceived(const char* server_ip, const teleport::core::SetupCommand &setupCommand, teleport::core::Handshake& handshake) override;
		void OnVideoStreamClosed() override;
		void OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand& reconfigureVideoCommand) override;
		void OnLightingSetupChanged(const teleport::core::SetupLightingCommand &l) override;
		void OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition>& inputDefinitions) override;

		void HandleLocalInputs(const teleport::core::Input& local_inputs);
		void ShowHideGui();

		vec3 hit={0,0,0};
	};
}