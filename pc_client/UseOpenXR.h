#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "common_maths.h"		// for avs::Pose
#include "common_networking.h"		// for avs::InputState
#include "TeleportClient/Input.h"
typedef int64_t XrTime;
struct XrCompositionLayerProjectionView;
struct XrCompositionLayerProjection;
struct swapchain_surfdata_t;
enum XrResult;

namespace teleport
{
	enum ActionId:uint16_t
	{
		INVALID=0,
		SELECT,
		SHOW_MENU,
		A,
		B,
		X,
		Y,
		LEFT_TRIGGER,
		RIGHT_TRIGGER,
		LEFT_SQUEEZE,
		RIGHT_SQUEEZE,
		LEFT_GRIP_POSE,
		RIGHT_GRIP_POSE,
		LEFT_AIM_POSE,
		RIGHT_AIM_POSE,
		LEFT_STICK_X,
		RIGHT_STICK_X,
		LEFT_STICK_Y,
		RIGHT_STICK_Y,
		LEFT_HAPTIC,
		RIGHT_HAPTIC,
		MAX_ACTIONS
	};
	//! Defines a mapping between an Input Definition from the server, 
	//! and an action on the client.
	struct InputMapping
	{
		avs::InputDefinition serverInputDefinition;
		ActionId clientActionId;
	};
	struct InputState
	{
		union
		{
			float float32;
			uint32_t uint32;
		};
	};
	struct OpenXRServer
	{
		std::vector<InputMapping> inputMappings;
		std::vector<InputState> inputStates;
		teleport::client::Input inputs;
		unsigned long long framenumber=0;
	};
	class UseOpenXR
	{
	public:
		bool Init(platform::crossplatform::RenderPlatform* renderPlatform, const char* app_name);
		bool TryInitDevice();
		void MakeActions();
		void PollActions();
		void RenderFrame(platform::crossplatform::GraphicsDeviceContext& deviceContext, platform::crossplatform::RenderDelegate &, vec3 origin_pos, vec4 origin_orientation);
		void Shutdown();
		void PollEvents(bool& exit);
		bool HaveXRDevice() const;

		// Getting mapped inputs specific to a given server, in-frame.
		void OnInputsSetupChanged(avs::uid server_uid,const std::vector<avs::InputDefinition> &inputDefinitions_);
		const teleport::client::Input& GetServerInputs(avs::uid server_uid,unsigned long long framenumber);

		const avs::Pose& GetHeadPose() const;
		const avs::Pose& GetControllerPose(int index) const;
		size_t GetNumControllers() const
		{
			return controllerPoses.size();
		};
		void SetMenuButtonHandler(std::function<void()> f)
		{
			menuButtonHandler = f;
		}
		const std::string &GetDebugString() const;
		platform::crossplatform::Texture* GetRenderTexture(int index=0);
	protected:
		std::map<avs::uid,OpenXRServer> openXRServers;
		platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
		bool haveXRDevice = false;
		void RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext, XrCompositionLayerProjectionView& view, swapchain_surfdata_t& surface, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
		bool RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext,XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
		avs::Pose headPose;
		std::vector<avs::Pose> controllerPoses;
		void openxr_poll_predicted(XrTime predicted_time);
		std::function<void()> menuButtonHandler;
		void RecordCurrentBindings();
	};
}
