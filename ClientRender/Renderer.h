// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "Common.h"
#include <libavstream/src/platform.hpp>
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/Text3DRenderer.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/ClientDeviceState.h"
#include "ClientRender/Gui.h"
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
		DEFAULT,PBR, ALBEDO, NORMALS, DEBUG_ANIM, LIGHTMAPS, NORMAL_VERTEXNORMALS, REZZING,NUM
	};
	//! Timestamp of when the system started.
	extern avs::Timestamp platformStartTimestamp;	
	//! Renderer that draws for a specific server.
	//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
	class Renderer:public platform::crossplatform::RenderDelegaterInterface
	{
		void UpdateShaderPasses();
	public:
		Renderer(teleport::Gui &g);
		virtual ~Renderer();
		//! This allows live-recompile of shaders (desktop platforms only).
		void RecompileShaders();
		void LoadShaders();
		void SetMinimumPriority(int32_t p)
		{
			minimumPriority=p;
		}
		int32_t GetMinimumPriority() const
		{
			return minimumPriority;
		}
		void ChangePass(ShaderMode newShaderMode);
		virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
		void RenderVRView(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		float framerate = 0.0f;
		void Update(double timestamp_ms);
		bool OSDVisible() const
		{
			return show_osd;
		}

	protected:
		bool reload_shaders=false;
		std::map<avs::uid,std::shared_ptr<InstanceRenderer>> instanceRenderers;
		virtual std::shared_ptr<InstanceRenderer> GetInstanceRenderer(avs::uid server_uid);
		void InitLocalGeometry();
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
			vec3 position[2];
			vec4 orientation[2];
		};
		ControllerSim controllerSim;
		platform::crossplatform::Camera			camera;
		platform::crossplatform::MouseCameraState	mouseCameraState;
		platform::crossplatform::MouseCameraInput	mouseCameraInput;
		
		bool start_xr_session=false,end_xr_session=false;
		/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
		/// distributes numerical precision to where it is better used.
		static const bool reverseDepth = true;


		bool keydown[256] = {};
	
		/// A pointer to RenderPlatform, so that we can use the platform::crossplatform API.
		platform::crossplatform::RenderPlatform		*renderPlatform	=nullptr;

		RenderState renderState;
		DebugOptions debugOptions;
		platform::crossplatform::Text3DRenderer text3DRenderer;
		bool show_osd = false;
		double previousTimestamp=0.0;
		int32_t minimumPriority=0;
		bool using_vr = true;

		struct LobbyGeometry
		{
			avs::uid left_root_node_uid = 0;
			avs::uid right_root_node_uid = 0;
			avs::uid local_left_hand_uid = 0;
			clientrender::Transform palm_to_hand_l;
			avs::uid local_right_hand_uid = 0;
			clientrender::Transform palm_to_hand_r;
			avs::uid hand_skin_uid = 0;
			vec3 index_finger_offset;
		};
		LobbyGeometry lobbyGeometry;

		static constexpr float HFOV = 90;
		float gamma=0.44f;

		bool have_vr_device = false;
		platform::crossplatform::Texture* externalTexture = nullptr;
		
		// TODO: temporary.
		avs::uid server_uid=1;
		const avs::uid local_server_uid=0;
		const avs::InputId local_menu_input_id=0;
		const avs::InputId local_cycle_osd_id=1;
		const avs::InputId local_cycle_shader_id=2;
		teleport::Gui &gui;
		teleport::client::Config &config;
		ShaderMode shaderMode=ShaderMode::PBR;
		avs::Pose GetOriginPose(avs::uid server_uid);
		std::queue<std::string> console;
		void ExecConsoleCommands();
		void ExecConsoleCommand(const std::string &str);
	public:
		
		GeometryDecoder geometryDecoder;
	
		bool OnDeviceRemoved();
		void OnFrameMove(double fTime, float time_step, bool have_headset);
		void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
		void OnMouseMove(int xPos, int yPos,bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
		void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);

		void ConsoleCommand(const std::string &str);
		
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
		void RemoveView(int) override;
		void DrawOSD(platform::crossplatform::GraphicsDeviceContext& deviceContext);
		
		// to render the vr view instead of re-rendering.
		void SetExternalTexture(platform::crossplatform::Texture* t);
		//void PrintHelpText(platform::crossplatform::GraphicsDeviceContext& deviceContext);

		void HandleLocalInputs(const teleport::core::Input& local_inputs);
		void ShowHideGui();

		vec3 hit={0,0,0};
	};
}