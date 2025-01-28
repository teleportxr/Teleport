// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "ClientRender/Gui.h"
#include "ClientRender/Camera2D.h"
#include "Common.h"
#include "InstanceRenderer.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Platform/CrossPlatform/MeshRenderer.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/CrossPlatform/Text3DRenderer.h"
#include "Platform/CrossPlatform/Camera.h"
#include "TeleportClient/ClientDeviceState.h"
#include "TeleportClient/SessionClient.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "VideoDecoderBackend.h"
#include <libavstream/src/platform.hpp>

namespace teleport
{
	namespace client
	{
		class ClientDeviceState;
	}
}

namespace teleport
{
	namespace clientrender
	{
		struct RendererStats
		{
			uint64_t frameCounter;
			double lastFrameTime;
			double lastFPS;
		};
		//! Timestamp of when the system started.
		extern avs::Timestamp platformStartTimestamp;
		//! Renderer that draws for a specific server.
		//! There will be one instance of a derived class of clientrender::Renderer for each attached server.
		class Renderer : public platform::crossplatform::RenderDelegatorInterface
		{
			void UpdateShaderPasses();

		public:
			Renderer(Gui &g);
			virtual ~Renderer();
			static Renderer *GetRenderer();

			//! This allows live-recompile of shaders (desktop platforms only).
			void RecompileShaders();
			void LoadShaders();
			void SetMinimumPriority(int32_t p)
			{
				minimumPriority = p;
			}
			int32_t GetMinimumPriority() const
			{
				return minimumPriority;
			}
			void ChangePass(ShaderMode newShaderMode);
			virtual void RenderView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void RenderVRView(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			float framerate = 0.0f;
			void Update(std::chrono::microseconds unix_time_us);

			//! e.g. for debug shaders or shader recompilation.
			void UpdateAllNodeRenders();
			// Called by nodemanager:
			void AddNodeToRender(avs::uid cache_uid, avs::uid node_uid);
			void RemoveNodeFromRender(avs::uid cache_uid, avs::uid node_uid);
			void UpdateNodeInRender(avs::uid cache_uid, avs::uid node_uid);

		protected:
			bool reload_shaders = false;
			std::map<avs::uid, std::shared_ptr<InstanceRenderer>> instanceRenderers;
			virtual std::shared_ptr<InstanceRenderer> GetInstanceRenderer(avs::uid server_uid);
			void InitLocalHandGeometry();
			void InitLocalGeometry();
			void RemoveInstanceRenderer(avs::uid);
			void InvalidateDeviceObjects();
			void CreateTexture(clientrender::AVSTextureHandle &th, int width, int height);
			void FillInControllerPose(int index, float offset);
			void SetRenderPose(platform::crossplatform::GraphicsDeviceContext& deviceContext, const teleport::core::Pose& originPose);
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
			platform::crossplatform::Camera camera;
			Camera2D camera2D;
			platform::crossplatform::MouseCameraState mouseCameraState;
			platform::crossplatform::MouseCameraInput mouseCameraInput;

			bool start_xr_session = false, end_xr_session = false;
			/// It is better to use a reversed depth buffer format, i.e. the near plane is z=1 and the far plane is z=0. This
			/// distributes numerical precision to where it is better used.
			static const bool reverseDepth = true;

			bool keydown[256] = {};

			/// A pointer to RenderPlatform, so that we can use the platform::crossplatform API.
			platform::crossplatform::RenderPlatform *renderPlatform = nullptr;

			RenderState renderState;
			platform::crossplatform::Text3DRenderer text3DRenderer;
			std::chrono::microseconds previousTimestampUs;
			int32_t minimumPriority = 0;
			struct ControllerModel
			{
				avs::uid controller_node_uid = 0;
				vec3 index_finger_offset = {0, 0, 0};
				avs::uid model_uid = 0;
			};
			struct HandModel
			{
				bool visible = false;

				avs::uid hand_node_uid = 0;			 // The parent node in the local scene:
				avs::uid hand_skeleton_node_uid = 0; // The skeleton root node, child of hand_node_uid.
				avs::uid hand_mesh_node_uid = 0;	 // The mesh root node, child of hand_node_uid.

				avs::uid hand_skeleton_uid = 0;		// The skeleton asset
				avs::uid model_uid = 0;				// The mesh asset
				clientrender::Transform palm_to_hand;
				vec3 index_finger_offset = {0, 0, 0};
			};

			struct LobbyGeometry
			{
				avs::uid self_node_uid;
				ControllerModel leftController;
				ControllerModel rightController;
				HandModel hands[2];
			};
			LobbyGeometry lobbyGeometry;

			static constexpr float HFOV = 90;
			float gamma = 0.44f;

			bool have_vr_device = false;
			platform::crossplatform::Texture *externalTexture = nullptr;

			// TODO: temporary.
			const avs::uid local_server_uid = 0;
			const teleport::core::InputId local_menu_input_id = 1;
			const teleport::core::InputId local_cycle_osd_id = 2;
			const teleport::core::InputId local_cycle_shader_id = 3;
			Gui &gui;
			client::Config &config;
			ShaderMode shaderMode = ShaderMode::DEFAULT;
			teleport::core::Pose GetOriginPose(avs::uid server_uid);
			std::queue<std::string> console;
			void ExecConsoleCommands();
			void ExecConsoleCommand(const std::string &str);

		public:
			GeometryDecoder geometryDecoder;

			bool OnDeviceRemoved();
			void OnFrameMove(double fTime, float time_step);
			void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
			void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
			void OnMouseMove(int xPos, int yPos, bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
			void OnKeyboard(unsigned wParam, bool bKeyDown, bool gui_shown);

			void ConsoleCommand(const std::string &str);
			void SelectionChanged();

			void WriteHierarchy(int tab, std::shared_ptr<clientrender::Node> node);
			void WriteHierarchies(avs::uid server);

			int AddView() override;
			void ResizeView(int view_id, int W, int H) override;
			void Render(int view_id, void *pContext, void *renderTexture, int w, int h, long long frame, void *context_allocator = nullptr) override
			{
				RenderDesktopView(view_id, pContext, renderTexture, w, h, frame, context_allocator);
			}
			void RenderDesktopView(int view_id, void *pContext, void *renderTexture, int w, int h, long long frame, void *context_allocator = nullptr);
			void Init(platform::crossplatform::RenderPlatform *r, teleport::client::OpenXR *u, PlatformWindow *active_window);

			// callbacks
			void HandTrackingChanged(int left_right, bool on_off);
			void XrBindingsChanged(std::string user_path, std::string profile);
			void XrSessionChanged(bool active);

			void RemoveView(int) override;
			void DrawOSD(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void DrawGUI(platform::crossplatform::GraphicsDeviceContext &deviceContext, bool mode_3d);
			//! Callback fn for OpenXR Overlay.
			void RenderVROverlay(platform::crossplatform::GraphicsDeviceContext &deviceContext);
			void UpdateVRGuiMouse();

			// to render the vr view instead of re-rendering.
			void SetExternalTexture(platform::crossplatform::Texture *t);
			// void PrintHelpText(platform::crossplatform::GraphicsDeviceContext& deviceContext);

			void HandleLocalInputs(const teleport::core::Input &local_inputs);
			void ShowHideGui();

			vec3 hit = {0, 0, 0};
			std::map<std::string, std::string> xr_profile_to_controller_model_name;
			vec3 camera_local_pos;
			platform::crossplatform::CameraInterface *cameraInterface=nullptr;
		};
	}
}