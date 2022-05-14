#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "libavstream/common_maths.h"		// for avs::Pose
#include "libavstream/common_networking.h"		// for avs::InputState
#include "TeleportCore/Input.h"
#include <openxr/openxr.h>

#define XR_CHECK(res) if (!XR_UNQUALIFIED_SUCCESS(res)){teleport::client::ReportError(xr_instance,(int)res);}
#include <openxr/openxr_reflection.h>

// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
// clang-format off
#define ENUM_CASE_STR(name, val) case name: return #name;

#define DECLARE_TO_STRING_FUNC(enumType) extern const char* to_string(enumType e);

#define MAKE_TO_STRING_FUNC(enumType)                  \
    const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }
// clang-format on


namespace teleport
{
	namespace client
	{
		DECLARE_TO_STRING_FUNC(XrReferenceSpaceType);
		DECLARE_TO_STRING_FUNC(XrViewConfigurationType);
		DECLARE_TO_STRING_FUNC(XrEnvironmentBlendMode);
		DECLARE_TO_STRING_FUNC(XrSessionState);
		DECLARE_TO_STRING_FUNC(XrResult);
		DECLARE_TO_STRING_FUNC(XrFormFactor);
		extern void ReportError(XrInstance xr_instance, int result);
		struct swapchain_surfdata_t
		{
			platform::crossplatform::Texture* depth_view;
			platform::crossplatform::Texture* target_view;
		};

		struct swapchain_t
		{
			XrSwapchain handle;
			int32_t     width=0;
			int32_t     height=0;
			uint32_t	last_img_id=0;
			std::vector<swapchain_surfdata_t>     surface_data;
		};
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
		//! State of an input. Note that we store *both* float and integer values,
		//! allowing for hysteresis in the integer interpretation of float input.
		struct InputState
		{
			float float32;
			uint32_t uint32;
		};
		struct NodePoseMapping
		{
			std::string regexPath;
			ActionId actionId;		// Which local action is bound to the node.
			avs::Pose poseOffset;	// In the XR pose's local space, the offset to the node's pose.
		};
		struct NodePoseState
		{
			avs::Pose pose;	// In global space, the offset to the node's current pose.
		};
		struct OpenXRServer
		{
			std::vector<InputMapping> inputMappings;
			std::vector<InputState> inputStates;
			std::map<avs::uid,NodePoseMapping> nodePoseMappings;
			std::map<avs::uid,NodePoseState> nodePoseStates;
			std::map<avs::uid,NodePoseMapping> unboundPoses;
			teleport::core::Input inputs;
			unsigned long long framenumber=0;
		};
		enum SubActionIndex:uint8_t
		{
			SA_NONE=0,
			SA_LEFT=1,
			SA_RIGHT=2,
			SA_GAMEPAD=4,
			SA_LEFT_AND_RIGHT=SA_LEFT|SA_RIGHT
		};

		struct ActionInitializer
		{
			ActionId actionId;
			const char* name;
			const char* localizedName;
			XrActionType xrActionType;
		};

		// struct to store the state of an XR action:
		struct ActionState
		{
			union
			{
				float f32;
				uint32_t u32;
				float vec2f[2];
				bool poseActive;
			};
			XrPosef	pose;
		};

		//
		struct XRInputDeviceState
		{
			XrBool32	renderThisDevice;
			XrBool32	handSelect;
			XrBool32	handMenu;
		};

		struct ActionDefinition
		{
			XrAction		xrAction;
			ActionId		actionId;
			XrActionType	xrActionType;
			XrSpace			space;
			std::string name;
		};

		struct XrInputSession
		{
			XrActionSet			actionSet;
			ActionDefinition    actionDefinitions[MAX_ACTIONS];
			ActionState			actionStates[MAX_ACTIONS];
			std::vector<XRInputDeviceState> inputDeviceStates;
			// Here we  can set all the actions to be supported.
			void SetActions(XrInstance& xr_instance, std::initializer_list<ActionInitializer> actions);
			void SessionInit(XrInstance xr_instance,XrSession &xr_session);
		};

		struct InteractionProfileBinding
		{
			XrAction action;
			const char *complete_path;
		};

		struct InteractionProfile
		{
			XrPath profilePath;
			std::string name;
			std::vector<XrActionSuggestedBinding> xrActionSuggestedBindings;
			std::vector<std::string> bindingPaths;
			void Init(XrInstance &xr_instance,const char *pr,std::initializer_list<InteractionProfileBinding> bindings);
		};
		class OpenXR
		{
		public:
			OpenXR(){}
			bool InitInstance(const char* app_name);
			bool Init(platform::crossplatform::RenderPlatform* renderPlatform);
			virtual bool TryInitDevice()=0;
			void MakeActions();
			void PollActions();
			void RenderFrame( platform::crossplatform::RenderDelegate &, vec3 origin_pos, vec4 origin_orientation);
			void Shutdown();
			void PollEvents(bool& exit);
			bool HaveXRDevice() const;
			bool IsXRDeviceActive() const;

			// Getting mapped inputs specific to a given server, in-frame.
			void OnInputsSetupChanged(avs::uid server_uid,const std::vector<avs::InputDefinition> &inputDefinitions_);
			void MapNodeToPose(avs::uid server_uid,avs::uid uid,const std::string &regexPath);
			const teleport::core::Input& GetServerInputs(avs::uid server_uid,unsigned long long framenumber);

			const avs::Pose& GetHeadPose() const;
			const avs::Pose& GetControllerPose(int index) const;
			const std::map<avs::uid,NodePoseState> &GetNodePoseStates(avs::uid server_uid,unsigned long long framenumber);
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
			bool IsSessionActive() const
			{
				return xr_session_running;
			}
		protected:
			void BindUnboundPoses(avs::uid server_uid);
			std::map<avs::uid,OpenXRServer> openXRServers;
			platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
			bool haveXRDevice = false;
			void RenderLayerView(platform::crossplatform::GraphicsDeviceContext &deviceContext,XrCompositionLayerProjectionView& view, swapchain_surfdata_t& surface, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
			bool RenderLayer(XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation);
			avs::Pose headPose;
			std::vector<avs::Pose> controllerPoses;
			void openxr_poll_predicted(XrTime predicted_time);
			std::function<void()> menuButtonHandler;
			void RecordCurrentBindings();
			void UpdateServerState(avs::uid server_uid,unsigned long long framenumber);
			static bool CheckXrResult(XrInstance xr_instance,XrResult res);
			XrTime lastTime=0;
			std::vector<swapchain_t>				xr_swapchains;

			// virtuals for platform-specific
			virtual const char *GetOpenXRGraphicsAPIExtensionName() const=0;
			virtual std::vector<std::string> GetRequiredExtensions() const;
			virtual void HandleSessionStateChanges( XrSessionState state);
			virtual platform::crossplatform::GraphicsDeviceContext& GetDeviceContext(int uint32_t)=0;
			virtual void FinishDeviceContext(int i) {}

			XrFormFactor					app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			XrViewConfigurationType			app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

			XrInstance						xr_instance = {};

			XrSession						xr_session = {};
			XrSessionState					xr_session_state = XR_SESSION_STATE_UNKNOWN;
			bool							xr_session_running = false;
			XrSpace							xr_app_space = {};
			XrSpace							xr_head_space = {};
			XrSystemId						xr_system_id = XR_NULL_SYSTEM_ID;
			XrInputSession					xr_input_session = { };
			XrEnvironmentBlendMode			xr_blend = {};
			XrDebugUtilsMessengerEXT		xr_debug = {};
			std::vector<XrView>					xr_views;
			std::vector<XrViewConfigurationView>	xr_config_views;
		};
	}
}
