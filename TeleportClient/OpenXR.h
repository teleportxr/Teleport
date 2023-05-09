#pragma once
#include "Platform/CrossPlatform/RenderPlatform.h"
#include <vector>
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Texture.h"
#include "libavstream/common_maths.h"		// for avs::Pose
#include "TeleportCore/CommonNetworking.h"		// for avs::InputState
#include "TeleportCore/Input.h"
#include <openxr/openxr.h>
#include <thread>

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
		enum class ThreadState
		{
			INACTIVE,
			STARTING,
			RUNNING,
			FINISHED,
			FAILED
		};
		DECLARE_TO_STRING_FUNC(XrReferenceSpaceType);
		DECLARE_TO_STRING_FUNC(XrViewConfigurationType);
		DECLARE_TO_STRING_FUNC(XrEnvironmentBlendMode);
		DECLARE_TO_STRING_FUNC(XrSessionState);
		DECLARE_TO_STRING_FUNC(XrResult);
		DECLARE_TO_STRING_FUNC(XrFormFactor);
		extern void ReportError(XrInstance xr_instance, int result);
		struct swapchain_surfdata_t
		{
			platform::crossplatform::Texture* depth_view=nullptr;
			platform::crossplatform::Texture* target_view=nullptr;
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
			SYSTEM,
			A,
			B,
			X,
			Y,
			HEAD_POSE,
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
			MOUSE_LEFT_BUTTON,
			MOUSE_RIGHT_BUTTON,
			MAX_ACTIONS
		};
		const char* stringof(ActionId a);
		//! Defines a mapping between an Input Definition from the server, 
		//! and an action on the client.
		struct InputMapping
		{
			teleport::core::InputDefinition serverInputDefinition;
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
			avs::PoseDynamic pose_footSpace;	// In the current XR space, the offset to the node's current pose.
		};
		//! Each OpenXRServer contains the currently bound inputs and poses for the connection. Both mappings (initialized on connection)
		//! and states (updated in real time).
		struct OpenXRServer
		{
			//! Definitions used on startup
			std::vector<teleport::core::InputDefinition> inputDefinitions;
			//! Mappings initialized on startup.
			std::vector<InputMapping> inputMappings;
			std::map<avs::uid,NodePoseMapping> nodePoseMappings;
			avs::uid rootNode=0;
			//! Current states
			std::vector<InputState> inputStates;
			std::map<avs::uid,NodePoseState> nodePoseStates;
			// temp, filled from nodePoseStates:
			std::map<avs::uid,avs::PoseDynamic> nodePoses;
			//! Temporary - these poses will be bound on the next update.
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
			XrPosef		pose_stageSpace;
			XrVector3f  velocity_stageSpace;
			XrVector3f  angularVelocity_stageSpace;
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
			std::string localizedName;
		};

		struct InputSession
		{
			XrActionSet			actionSet;
			std::vector<ActionDefinition>    actionDefinitions;
			std::vector<ActionState>		actionStates;
			std::vector<XRInputDeviceState> inputDeviceStates;
			std::map<int32_t,int32_t> boundKeys;
			// Here we  can set all the actions to be supported.
			void SetActions( std::initializer_list<ActionInitializer> actions);
			ActionId AddAction( const char* name,const char* localizedName,XrActionType xrActionType);
			void InstanceInit(XrInstance& xr_instance);
			void SessionInit(XrInstance xr_instance,XrSession &xr_session);
		};

		struct InteractionProfileBinding
		{
			XrAction action;
			const char *complete_path=nullptr;
			bool virtual_action=false;
			};

		struct InteractionProfile
		{
			XrPath profilePath;
			std::string name;
			std::vector<XrActionSuggestedBinding> xrActionSuggestedBindings;
			std::vector<std::string> bindingPaths;
			void Init(XrInstance &xr_instance,const char *pr,std::initializer_list<InteractionProfileBinding> bindings);
			//! virtual_binding  means that the binding is not a real OpenXR path, but e.g. mouse/keyboard.
			void Add(XrInstance &xr_instance,XrAction action,const char *complete_path,bool virtual_binding);
		};

		struct FallbackBinding
		{
			std::string path;
		};
		struct FallbackState
		{
			avs::Pose pose_worldSpace;
			bool buttonDown=false;
		};
		struct MouseState
		{
			bool leftButtonDown=false;
			bool rightButtonDown=false;
			bool middleButtonDown=false; 
		};
		struct Overlay
		{
			XrPosef pose={{0,0,0,1.0f},{2.0f, 1.8f,0.0f}};
			XrExtent2Df size = {4.0f, 2.0f};
		};
		enum class OpenXRState
		{
			Uninitialized,
			Initialized,
			Running,
			Stopped
		};
		class OpenXR
		{
		public:
			OpenXR(const char *app_name);
			virtual ~OpenXR();
			bool InitInstance();
			void SetRenderPlatform(platform::crossplatform::RenderPlatform* renderPlatform);
			void Shutdown();
			virtual bool StartSession()=0;
			void EndSession();
			void CreateMouseAndKeyboardProfile();
			void MakeActions();
			void Tick();
			void PollActions();
			void RenderFrame( platform::crossplatform::RenderDelegate &, platform::crossplatform::RenderDelegate &);
			void PollEvents();
			bool HaveXRDevice() const;
			const char * GetSystemName() const;
			bool IsXRDeviceRendering() const;
			bool CanStartSession() ;
			bool RequestsQuit() const
			{
				return quit;
			}
			bool ActivatePassthrough(bool on_off);
			bool IsPassthroughActive() const;
			
			//! Set a "virtual" pose binding - e.g. mouse emulation.
			void SetFallbackBinding(ActionId actionId,std::string path);
			void SetFallbackPoseState(ActionId actionId,const avs::Pose &pose);
			void SetFallbackButtonState(ActionId actionId,bool btn_down);

			//! Process mouse and keyboard
			void OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta);
			void OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta);
			void OnMouseMove(int xPos, int yPos );
			void OnKeyboard(unsigned wParam, bool bKeyDown);

			// Getting mapped inputs specific to a given server, in-frame.
			void OnInputsSetupChanged(avs::uid server_uid,const std::vector<teleport::core::InputDefinition> &inputDefinitions_);
			void MapNodeToPose(avs::uid server_uid,avs::uid uid,const std::string &regexPath);
			//! Remove the mapping of the specified node - it has perhaps been unmapped or destroyed.
			void RemoveNodePoseMapping(avs::uid server_uid,avs::uid uid);
			const teleport::core::Input& GetServerInputs(avs::uid server_uid,unsigned long long framenumber);
			//! Get the currently bound input mappings for the server.
			const std::vector<InputMapping> &GetServerInputMappings(avs::uid server_uid);
			//! Get the currently bound pose mappings for the server.
			const std::map<avs::uid, NodePoseMapping >& GetServerNodePoseMappings(avs::uid server_uid);
			// Force input mapping to a particular setting - normally for local controls.
			void SetHardInputMapping(avs::uid server_uid,avs::InputId inputId,avs::InputType inputType,ActionId clientActionId);

			//! Get the head pose in the device's stage space (axes adapted to the Engineering standard, Z=up).
			const avs::Pose& GetHeadPose_StageSpace() const;
			avs::Pose GetActionPose(ActionId id) const;
			float GetActionFloatState(ActionId actionId) const;
			 avs::uid GetRootNode(avs::uid server_uid);
			const std::map<avs::uid,avs::PoseDynamic> &GetNodePoses(avs::uid server_uid,unsigned long long framenumber);
			const std::map<avs::uid,NodePoseState> &GetNodePoseStates(avs::uid server_uid,unsigned long long framenumber);
			
			const std::string &GetDebugString() const;
			platform::crossplatform::Texture* GetRenderTexture(int index=0);
			bool IsSessionActive() const
			{
				return xr_session_running;
			}

			Overlay overlay;
			static avs::Pose ConvertGLStageSpacePoseToWorldSpacePose(const avs::Pose &stagePose_worldSpace,const XrPosef &pose) ;
			static avs::Pose ConvertGLStageSpacePoseToLocalSpacePose(const XrPosef &pose) ;
			vec3 ConvertGLStageSpaceDirectionToLocalSpace(const XrVector3f &d) const;
			platform::crossplatform::ViewStruct CreateViewStructFromXrCompositionLayerProjectionView(XrCompositionLayerProjectionView view, int id, platform::crossplatform::DepthTextureStyle depthTextureStyle);
			static platform::math::Matrix4x4 CreateViewMatrixFromPose(const avs::Pose& pose);
			static platform::math::Matrix4x4 CreateTransformMatrixFromPose(const avs::Pose& pose);

		protected:
			bool threadedInitInstance();
			bool internalInitInstance();
			bool quit=false;
			std::string applicationName;
			MouseState mouseState;
			std::string GetBoundPath(const ActionDefinition &def) const;
			std::map<avs::uid,FallbackBinding> fallbackBindings;
			std::map<avs::uid,FallbackState> fallbackStates;
			void BindUnboundPoses(avs::uid server_uid);
			std::map<avs::uid,OpenXRServer> openXRServers;
			platform::crossplatform::RenderPlatform* renderPlatform = nullptr;
			bool haveXRDevice = false;
			void RenderLayerView(platform::crossplatform::GraphicsDeviceContext &deviceContext, XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& projection_views, swapchain_surfdata_t& surface, platform::crossplatform::RenderDelegate& renderDelegate);
			bool RenderLayer(XrTime predictedTime, std::vector<XrCompositionLayerProjectionView>& projection_views,std::vector<XrCompositionLayerSpaceWarpInfoFB>& spacewarp_views
						, XrCompositionLayerProjection& layer, platform::crossplatform::RenderDelegate& renderDelegate);
			void DoSpaceWarp(XrCompositionLayerProjectionView &projection_view,XrCompositionLayerSpaceWarpInfoFB &spacewarp_view,int i);
			bool RenderOverlayLayer(XrTime predictedTime,platform::crossplatform::RenderDelegate &overlayDelegate);
			bool AddOverlayLayer(XrTime predictedTime,XrCompositionLayerQuad &layer,int i);

			avs::Pose headPose_stageSpace={};
			struct XrState
			{
				XrPosef XrSpacePoseInWorld={0};
			};
			XrState state;
			XrState previousState;
			void openxr_poll_predicted(XrTime predicted_time);
			void RecordCurrentBindings();
			void UpdateServerState(avs::uid server_uid,unsigned long long framenumber);
			static bool CheckXrResult(XrInstance xr_instance,XrResult res);
			XrTime lastTime=0;
			std::vector<swapchain_t>				xr_swapchains;
			// virtuals for platform-specific
			virtual const char *GetOpenXRGraphicsAPIExtensionName() const=0;
			virtual std::set<std::string> GetRequiredExtensions() const;
			virtual std::set<std::string> GetOptionalExtensions() const;
			virtual void HandleSessionStateChanges( XrSessionState state);
			virtual platform::crossplatform::GraphicsDeviceContext& GetDeviceContext(size_t swapchainIndex, size_t imageIndex)=0;
			virtual void FinishDeviceContext(size_t swapchainIndex, size_t imageIndex) {}
			virtual void EndFrame() {}
			
			virtual bool SystemSupportsPassthrough( XrFormFactor formFactor);
			XrFormFactor							app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			XrViewConfigurationType					app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

			XrInstance								xr_instance = {};
			XrSystemProperties						xr_system_properties = {};	
			XrSession								xr_session = {};
			XrSessionState							xr_session_state = XR_SESSION_STATE_UNKNOWN;
			bool									xr_session_running = false;
			XrSpace									xr_app_space = {};
			XrSpace									xr_head_space = {};
			// Mutable because we store it for speed.
			XrSystemId								xr_system_id = XR_NULL_SYSTEM_ID;
			InputSession							xr_input_session = { };
			XrEnvironmentBlendMode					xr_environment_blend = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
			XrDebugUtilsMessengerEXT				xr_debug = {};
			std::vector<XrView>						xr_views;
			std::vector<XrViewConfigurationView>	xr_config_views;
			int MAIN_SWAPCHAIN = 0;
			int MOTION_VECTOR_SWAPCHAIN=0;
			int MOTION_DEPTH_SWAPCHAIN=0;
			int OVERLAY_SWAPCHAIN=0;
			XrPath userHandLeftActiveProfile=0;
			XrPath userHandRightActiveProfile=0;
			
			std::vector<XrPath> activeInteractionProfilePaths;
			std::map<uint16_t, uint16_t> mapActionIndexToInputId;
			std::vector<InteractionProfile> interactionProfiles;
			size_t MOUSE_KEYBOARD_PROFILE_INDEX=0;
			const InteractionProfile *GetActiveBinding(XrPath p) const;
			// InitInstance runs on a thread, because it takes a long time and blocks.
			std::thread initInstanceThread;
			std::atomic<ThreadState> initInstanceThreadState=ThreadState::INACTIVE;
			std::mutex instanceMutex;
			std::set<std::string> got_extensions;
			XrPassthroughFB passthroughFeature = XR_NULL_HANDLE;
			XrPassthroughLayerFB passthroughLayer = XR_NULL_HANDLE;
		};
	}
}
