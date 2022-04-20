#include "UseOpenXR.h"
#include <vector>
#if TELEPORT_CLIENT_USE_D3D12
#include <d3d12.h>
#define XR_USE_GRAPHICS_API_D3D12
#else
#include <d3d11.h>
#define XR_USE_GRAPHICS_API_D3D11
#endif
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "fmt/core.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "Platform/CrossPlatform/AxesStandard.h"
#include "TeleportCore/ErrorHandling.h"

#include <regex>

XrInstance		xr_instance = {};

const char *GetXRErrorString(XrResult res)
{
	static char str[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(xr_instance, res, str);
	return str;
}

static void ReportError(XrInstance xr_instance, int result)
{
	XrResult res = (XrResult)result;
	std::cerr << "Error: " << GetXRErrorString(res) << std::endl;
}

#define XR_CHECK(res) if (!XR_UNQUALIFIED_SUCCESS(res)){TELEPORT_CERR<<"";ReportError(xr_instance,(int)res);}

using namespace std;
using namespace platform;
using namespace teleport;
const XrPosef	xr_pose_identity = { {0,0,0,1}, {0,0,0} };

XrPath MakeXrPath(const XrInstance & xr_instance,const char* str)
{
	XrPath path;
	xrStringToPath(xr_instance, str, &path);
	return path;
}

std::string FromXrPath(const XrInstance & xr_instance,XrPath path)
{
	uint32_t strl;
	char text[XR_MAX_PATH_LENGTH];
	XrResult res;
	res=xrPathToString(xr_instance, path, XR_MAX_PATH_LENGTH, &strl, text);
	std::string str;
	if(res==XR_SUCCESS)
	{
		str=text;
	}
	else
	{
		XR_CHECK(res);
	}
	return str;
}
struct XrGraphicsBindingPlatform
{
	XrStructureType             type;
	const void* XR_MAY_ALIAS    next;
	crossplatform::RenderPlatform* renderPlatform;
} ;

struct XrSwapchainImagePlatform
{
	XrStructureType      type;
	void* XR_MAY_ALIAS    next;
	crossplatform::Texture* texture;
} ;

struct swapchain_surfdata_t
{
	crossplatform::Texture* depth_view;
	crossplatform::Texture* target_view;
};
struct swapchain_t
{
	XrSwapchain handle;
	int32_t     width=0;
	int32_t     height=0;
	uint32_t	last_img_id=0;
#if TELEPORT_CLIENT_USE_D3D12
	vector<XrSwapchainImageD3D12KHR> surface_images;
#else
	vector<XrSwapchainImageD3D11KHR> surface_images;
#endif
	vector<swapchain_surfdata_t>     surface_data;
};

void swapchain_destroy(swapchain_t& swapchain)
{
	for (uint32_t i = 0; i < swapchain.surface_data.size(); i++)
	{
		delete swapchain.surface_data[i].depth_view;
		delete swapchain.surface_data[i].target_view;
	}
}

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
	void SetActions(XrInstance& xr_instance, std::initializer_list<ActionInitializer> actions)
	{
		inputDeviceStates.resize(2);
		for (auto& a : actions)
		{
			XrActionCreateInfo action_info = { XR_TYPE_ACTION_CREATE_INFO };
			action_info.actionType = a.xrActionType;
			strcpy_s(action_info.actionName, a.name);
			strcpy_s(action_info.localizedActionName, a.localizedName);
			auto &def=actionDefinitions[a.actionId];
			XR_CHECK(xrCreateAction(actionSet, &action_info, &def.xrAction));
			def.actionId=a.actionId;
			def.xrActionType=a.xrActionType;
			def.name=a.name;
		}
	}
	void SessionInit(XrSession &xr_session)
	{
		for (int i=0;i<MAX_ACTIONS;i++)
		{
			auto& def = actionDefinitions[i];

			if(def.xrActionType==XR_ACTION_TYPE_POSE_INPUT)
			{
				// Create frames of reference for the pose actions
					XrActionSpaceCreateInfo action_space_info = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
					action_space_info.action			= def.xrAction;
					action_space_info.poseInActionSpace	= xr_pose_identity;
					XR_CHECK(xrCreateActionSpace(xr_session, &action_space_info, &actionDefinitions[def.actionId].space));
			}
		}
	}
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
	void Init(XrInstance &xr_instance,const char *pr,std::initializer_list<InteractionProfileBinding> bindings)
	{
		name=pr;
		profilePath= MakeXrPath(xr_instance, pr);
		xrActionSuggestedBindings.reserve(bindings.size());
		bindingPaths.reserve(bindings.size());
		size_t i = 0;
		for (auto elem : bindings)
		{
			XrPath p;
			p= MakeXrPath(xr_instance, elem.complete_path);
			if(elem.action&&p)
			{
				xrActionSuggestedBindings.push_back( {elem.action, p});
				bindingPaths.push_back(elem.complete_path);
				i++;
			}
			else
			{
				if(!p)
				{
					TELEPORT_CERR<<"InteractionProfile: "<<pr<<": Failed to create suggested binding as path "<<elem.complete_path<<" was invalid."<<std::endl;
				}
				if(!elem.action)
				{
					TELEPORT_CERR<<"InteractionProfile: "<<pr<<": Failed to create suggested binding "<<elem.complete_path<<" as action is null."<<std::endl;
				}
			}
		}
	}
};

std::vector<InteractionProfile> interactionProfiles;
std::vector<XrPath> activeInteractionProfilePaths;
std::map<uint16_t, uint16_t> mapActionIndexToInputId;

XrSession						xr_session = {};
XrSessionState					xr_session_state = XR_SESSION_STATE_UNKNOWN;
bool							xr_running = false;
XrSpace							xr_app_space = {};
XrSpace							xr_head_space = {};
XrSystemId						xr_system_id = XR_NULL_SYSTEM_ID;
XrInputSession					xr_input_session = { };
XrEnvironmentBlendMode			xr_blend = {};
XrDebugUtilsMessengerEXT		xr_debug = {};
vector<XrView>					xr_views;
vector<XrViewConfigurationView>	xr_config_views;
vector<swapchain_t>				xr_swapchains;

// Function pointers for some OpenXR extension methods we'll use.
#if TELEPORT_CLIENT_USE_D3D12
PFN_xrGetD3D12GraphicsRequirementsKHR ext_xrGetD3D12GraphicsRequirementsKHR = nullptr;
#else
PFN_xrGetD3D11GraphicsRequirementsKHR ext_xrGetD3D11GraphicsRequirementsKHR = nullptr;
#endif
PFN_xrCreateDebugUtilsMessengerEXT    ext_xrCreateDebugUtilsMessengerEXT = nullptr;
PFN_xrDestroyDebugUtilsMessengerEXT   ext_xrDestroyDebugUtilsMessengerEXT = nullptr;

struct app_transform_buffer_t
{
	float world[16];
	float viewproj[16];
};

XrFormFactor            app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
XrViewConfigurationType app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

swapchain_surfdata_t CreateSurfaceData(crossplatform::RenderPlatform *renderPlatform,XrBaseInStructure& swapchain_img,int64_t d3d_swapchain_fmt)
{
	swapchain_surfdata_t result = {};

	// Get information about the swapchain image that OpenXR made for us!
#if TELEPORT_CLIENT_USE_D3D12
	XrSwapchainImageD3D12KHR& d3d_swapchain_img = (XrSwapchainImageD3D12KHR&)swapchain_img;
#else
	XrSwapchainImageD3D11KHR& d3d_swapchain_img = (XrSwapchainImageD3D11KHR&)swapchain_img;
#endif
	result.target_view				= renderPlatform->CreateTexture("swapchain target");
	result.target_view->InitFromExternalTexture2D(renderPlatform, d3d_swapchain_img.texture, nullptr,0,0,platform::crossplatform::UNKNOWN,true);
	result.depth_view				= renderPlatform->CreateTexture("swapchain depth");
	platform::crossplatform::TextureCreate textureCreate = {};
	textureCreate.numOfSamples		= std::max(1,result.target_view->GetSampleCount());
	textureCreate.mips				= 1;
	textureCreate.w					= result.target_view->width;
	textureCreate.l					= result.target_view->length;
	textureCreate.arraysize			= 1;
	textureCreate.f					= platform::crossplatform::PixelFormat::D_32_FLOAT;
	textureCreate.setDepthStencil	= true;
	textureCreate.computable		= false;
	result.depth_view->EnsureTexture(renderPlatform, &textureCreate);

	return result;
}
static bool CheckXrResult(XrResult res)
{
	if(res == XR_SUCCESS)
		return true;
	char str[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(xr_instance, res, str);
	std::cerr << "CheckXrResult: " << str << std::endl;
	return false;
}

bool UseOpenXR::Init(crossplatform::RenderPlatform *r,const char* app_name)
{
	// OpenXR will fail to initialize if we ask for an extension that OpenXR
	// can't provide! So we need to check our all extensions before 
	// initializing OpenXR with them. Note that even if the extension is 
	// present, it's still possible you may not be able to use it. For 
	// example: the hand tracking extension may be present, but the hand
	// sensor might not be plugged in or turned on. There are often 
	// additional checks that should be made before using certain features!
	vector<const char*> use_extensions;
	const char* ask_extensions[] = {
#if TELEPORT_CLIENT_USE_D3D12
		XR_KHR_D3D12_ENABLE_EXTENSION_NAME, // Use Direct3D12 for rendering
#else
		XR_KHR_D3D11_ENABLE_EXTENSION_NAME, // Use Direct3D11 for rendering
#endif
		XR_EXT_DEBUG_UTILS_EXTENSION_NAME,  // Debug utils for extra info
	};
	renderPlatform = r;
	// We'll get a list of extensions that OpenXR provides using this 
	// enumerate pattern. OpenXR often uses a two-call enumeration pattern 
	// where the first call will tell you how much memory to allocate, and
	// the second call will provide you with the actual data!
	uint32_t ext_count = 0;
	xrEnumerateInstanceExtensionProperties(nullptr, 0, &ext_count, nullptr);
	vector<XrExtensionProperties> xr_exts(ext_count, { XR_TYPE_EXTENSION_PROPERTIES });
	xrEnumerateInstanceExtensionProperties(nullptr, ext_count, &ext_count, xr_exts.data());

	std::cout<<"OpenXR extensions available:\n";
	for (size_t i = 0; i < xr_exts.size(); i++)
	{
		std::cout<<fmt::format("- {}\n", xr_exts[i].extensionName).c_str();

		// Check if we're asking for this extensions, and add it to our use 
		// list!
		for (int32_t ask = 0; ask < _countof(ask_extensions); ask++) {
			if (strcmp(ask_extensions[ask], xr_exts[i].extensionName) == 0) {
				use_extensions.push_back(ask_extensions[ask]);
				break;
			}
		}
	}
	// If a required extension isn't present, you want to ditch out here!
	// It's possible something like your rendering API might not be provided
	// by the active runtime. APIs like OpenGL don't have universal support.
	if (!std::any_of(use_extensions.begin(), use_extensions.end(),
		[](const char* ext)
			{
#if TELEPORT_CLIENT_USE_D3D12
				return strcmp(ext, XR_KHR_D3D12_ENABLE_EXTENSION_NAME) == 0;
#else
				return strcmp(ext, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0;
#endif
			}))
		return false;

	// Initialize OpenXR with the extensions we've found!
	XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.enabledExtensionCount =(uint32_t) use_extensions.size();
	createInfo.enabledExtensionNames = use_extensions.data();
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	strcpy_s(createInfo.applicationInfo.applicationName, app_name);
	xrCreateInstance(&createInfo, &xr_instance);

	// Check if OpenXR is on this system, if this is null here, the user 
	// needs to install an OpenXR runtime and ensure it's active!
	if (xr_instance == nullptr)
		return false;

	// Load extension methods that we'll need for this application! There's a
	// couple ways to do this, and this is a fairly manual one. Chek out this
	// file for another way to do it:
	// https://github.com/maluoi/StereoKit/blob/master/StereoKitC/systems/platform/openxr_extensions.h
	xrGetInstanceProcAddr(xr_instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrCreateDebugUtilsMessengerEXT));
	xrGetInstanceProcAddr(xr_instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrDestroyDebugUtilsMessengerEXT));
#if TELEPORT_CLIENT_USE_D3D12
	xrGetInstanceProcAddr(xr_instance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D12GraphicsRequirementsKHR));
#else
	xrGetInstanceProcAddr(xr_instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D11GraphicsRequirementsKHR));
#endif
	// Set up a really verbose debug log! Great for dev, but turn this off or
	// down for final builds. WMR doesn't produce much output here, but it
	// may be more useful for other runtimes?
	// Here's some extra information about the message types and severities:
	// https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#debug-message-categorization
#if TELEPORT_INTERNAL
	XrDebugUtilsMessengerCreateInfoEXT debug_info = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debug_info.messageTypes =
		XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
	debug_info.messageSeverities =
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug_info.userCallback = [](XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT types, const XrDebugUtilsMessengerCallbackDataEXT* msg, void* user_data) {
		// Print the debug message we got! There's a bunch more info we could
		// add here too, but this is a pretty good start, and you can always
		// add a breakpoint this line!
		std::cout<<fmt::format("{}: {}\n", msg->functionName, msg->message).c_str()<<std::endl;

		// Output to debug window
		std::cout<<fmt::format( "{}: {}", msg->functionName, msg->message).c_str() << std::endl;

		// Returning XR_TRUE here will force the calling function to fail
		return (XrBool32)XR_FALSE;
	};
	// Start up the debug utils!
	if (ext_xrCreateDebugUtilsMessengerEXT)
		ext_xrCreateDebugUtilsMessengerEXT(xr_instance, &debug_info, &xr_debug);
#endif
	return true;
}

bool UseOpenXR::TryInitDevice()
{
	// Request a form factor from the device (HMD, Handheld, etc.)
	// If the device is not on, not connected, or its app is not running, this may fail here:
	XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = app_config_form;
	if (!CheckXrResult(xrGetSystem(xr_instance, &systemInfo, &xr_system_id)))
	{
		std::cerr << fmt::format("Failed to Get XR System\n").c_str() << std::endl;
		return false;
	}

	// Check what blend mode is valid for this device (opaque vs transparent displays)
	// We'll just take the first one available!
	uint32_t blend_count = 0;
	xrEnumerateEnvironmentBlendModes(xr_instance, xr_system_id, app_config_view, 1, &blend_count, &xr_blend);

	// OpenXR wants to ensure apps are using the correct graphics card, so this MUST be called 
	// before xrCreateSession. This is crucial on devices that have multiple graphics cards, 
	// like laptops with integrated graphics chips in addition to dedicated graphics cards.
#if TELEPORT_CLIENT_USE_D3D12
	XrGraphicsRequirementsD3D12KHR requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
	ext_xrGetD3D12GraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBindingD3D12KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
	binding.device = renderPlatform->AsD3D12Device();
#else
	XrGraphicsRequirementsD3D11KHR requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	ext_xrGetD3D11GraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBindingD3D11KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	binding.device = renderPlatform->AsD3D11Device();
#endif
	XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.next = &binding;
	sessionInfo.systemId = xr_system_id;
	if (!CheckXrResult(xrCreateSession(xr_instance, &sessionInfo, &xr_session)))
	{
		std::cerr<<fmt::format("Failed to create XR Session\n").c_str() << std::endl;
		return false;
	}

	// Unable to start a session, may not have an MR device attached or ready
	if (xr_session == nullptr)
		return false;

	// OpenXR uses a couple different types of reference frames for positioning content, we need to choose one for
	// displaying our content! STAGE would be relative to the center of your guardian system's bounds, and LOCAL
	// would be relative to your device's starting location. HoloLens doesn't have a STAGE, so we'll use LOCAL.
	XrReferenceSpaceCreateInfo ref_space = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	ref_space.poseInReferenceSpace = xr_pose_identity;
	ref_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	xrCreateReferenceSpace(xr_session, &ref_space, &xr_app_space);

	ref_space.poseInReferenceSpace = xr_pose_identity;
	ref_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	xrCreateReferenceSpace(xr_session, &ref_space, &xr_head_space);
	
	// Now we need to find all the viewpoints we need to take care of! For a stereo headset, this should be 2.
	// Similarly, for an AR phone, we'll need 1, and a VR cave could have 6, or even 12!
	uint32_t view_count = 0;
	xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, 0, &view_count, nullptr);
	xr_config_views.resize(view_count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	xr_views.resize(view_count, { XR_TYPE_VIEW });
	xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, view_count, &view_count, xr_config_views.data());
	// Can we override this stuff?
	static int samples=1;
	for (uint32_t i = 0; i < view_count; i++)
	{
		xr_config_views[i].maxSwapchainSampleCount=samples;
		xr_config_views[i].recommendedSwapchainSampleCount=samples;
	}
	int64_t swapchain_format = 0;
	// Find out what format to use:
	uint32_t formatCount = 0;
	XrResult res = xrEnumerateSwapchainFormats(
		xr_session,
		0,
		&formatCount,
		nullptr);
	if (!formatCount)
		return false;
	std::vector<int64_t> formats(formatCount);
	res = xrEnumerateSwapchainFormats(
		xr_session,
		formatCount,
		&formatCount,
		formats.data());
	std::cout<<"xrEnumerateSwapchainFormats:\n";
	for(auto f:formats)
	{
		DXGI_FORMAT F=(DXGI_FORMAT)f;
		std::cout<<"    "<<F<<std::endl;
	}
	if (std::find(formats.begin(), formats.end(), swapchain_format) == formats.end())
	{
		swapchain_format = formats[0];
	}

	for (uint32_t i = 0; i < view_count; i++)
	{
		// Create a swapchain for this viewpoint! A swapchain is a set of texture buffers used for displaying to screen,
		// typically this is a backbuffer and a front buffer, one for rendering data to, and one for displaying on-screen.
		// A note about swapchain image format here! OpenXR doesn't create a concrete image format for the texture, like 
		// DXGI_FORMAT_R8G8B8A8_UNORM. Instead, it switches to the TYPELESS variant of the provided texture format, like 
		// DXGI_FORMAT_R8G8B8A8_TYPELESS. When creating an ID3D11RenderTargetView for the swapchain texture, we must specify
		// a concrete type like DXGI_FORMAT_R8G8B8A8_UNORM, as attempting to create a TYPELESS view will throw errors, so 
		// we do need to store the format separately and remember it later.
		XrViewConfigurationView& view = xr_config_views[i];
		XrSwapchainCreateInfo    swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		XrSwapchain              handle;
		swapchain_info.arraySize = 1;
		swapchain_info.mipCount		= 1;
		swapchain_info.faceCount	= 1;
		swapchain_info.format		= swapchain_format;
		swapchain_info.width		= view.recommendedImageRectWidth;
		swapchain_info.height		= view.recommendedImageRectHeight;
		swapchain_info.sampleCount	= view.recommendedSwapchainSampleCount;
		swapchain_info.usageFlags	= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrCreateSwapchain(xr_session, &swapchain_info, &handle);

		// Find out how many textures were generated for the swapchain
		uint32_t surface_count = 0;
		xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

		// We'll want to track our own information about the swapchain, so we can draw stuff onto it! We'll also create
		// a depth buffer for each generated texture here as well with make_surfacedata.
		swapchain_t swapchain = {};
		swapchain.width = swapchain_info.width;
		swapchain.height = swapchain_info.height;
		swapchain.handle = handle;
#if TELEPORT_CLIENT_USE_D3D12
		swapchain.surface_images.resize(surface_count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
#else
		swapchain.surface_images.resize(surface_count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
#endif
		swapchain.surface_data.resize(surface_count);
		xrEnumerateSwapchainImages(swapchain.handle, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)swapchain.surface_images.data());
		for (uint32_t i = 0; i < surface_count; i++)
		{
			swapchain.surface_data[i] = CreateSurfaceData(renderPlatform,(XrBaseInStructure&)swapchain.surface_images[i],swapchain_format);
		}
		xr_swapchains.push_back(swapchain);
	}

	haveXRDevice = true;
	return true;
}

const char* left = "user/hand/left";
const char* right = "user/hand/right";
void UseOpenXR::MakeActions()
{
	XrActionSetCreateInfo actionset_info = { XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy_s(actionset_info.actionSetName, "teleport_client");
	strcpy_s(actionset_info.localizedActionSetName, "TeleportClient");
	XR_CHECK(xrCreateActionSet(xr_instance, &actionset_info, &xr_input_session.actionSet));

	xr_input_session.SetActions(xr_instance, {
		// Create an action to track the position and orientation of the hands! This is
		// the controller location, or the center of the palms for actual hands.
		 // Create an action for listening to the select action! This is primary trigger
		 // on controllers, and an airtap on HoloLens
		 {SELECT		,"select"				,"Select"		,XR_ACTION_TYPE_BOOLEAN_INPUT}
		,{SHOW_MENU		,"menu"					,"Menu"			,XR_ACTION_TYPE_BOOLEAN_INPUT}
		,{A				,"a"					,"A"			,XR_ACTION_TYPE_BOOLEAN_INPUT}
		,{B				,"b"					,"B"			,XR_ACTION_TYPE_BOOLEAN_INPUT}
		,{X				,"x"					,"X"			,XR_ACTION_TYPE_BOOLEAN_INPUT}
		,{Y				,"y"					,"Y"			,XR_ACTION_TYPE_BOOLEAN_INPUT}
		// Action for left controller
		,{LEFT_GRIP_POSE	,"left_grip_pose"			,"Left Grip Pose"		,XR_ACTION_TYPE_POSE_INPUT}
		,{LEFT_AIM_POSE		,"left_aim_pose"			,"Left Aim Pose"		,XR_ACTION_TYPE_POSE_INPUT}
		,{LEFT_TRIGGER		,"left_trigger"				,"Left Trigger"			,XR_ACTION_TYPE_FLOAT_INPUT}
		,{LEFT_SQUEEZE		,"left_squeeze"				,"Left Squeeze"			,XR_ACTION_TYPE_FLOAT_INPUT}
		,{LEFT_STICK_X		,"left_thumbstick_x"		,"Left Thumbstick X"	,XR_ACTION_TYPE_FLOAT_INPUT}
		,{LEFT_STICK_Y		,"left_thumbstick_y"		,"Left Thumbstick Y"	,XR_ACTION_TYPE_FLOAT_INPUT}
		,{LEFT_HAPTIC		,"left_haptic"				,"Left Haptic"			,XR_ACTION_TYPE_VIBRATION_OUTPUT}
		// Action for right controller
		,{RIGHT_GRIP_POSE	,"right_grip_pose"			,"Right Grip Pose"		,XR_ACTION_TYPE_POSE_INPUT}
		,{RIGHT_AIM_POSE	,"right_aim_pose"			,"Right Aim Pose"		,XR_ACTION_TYPE_POSE_INPUT}
		,{RIGHT_TRIGGER		,"right_trigger"			,"Right Trigger"		,XR_ACTION_TYPE_FLOAT_INPUT}
		,{RIGHT_SQUEEZE		,"right_squeeze"			,"Right Squeeze"		,XR_ACTION_TYPE_FLOAT_INPUT}
		,{RIGHT_STICK_X		,"right_thumbstick_x"		,"Right Thumbstick X"	,XR_ACTION_TYPE_FLOAT_INPUT}
		,{RIGHT_STICK_Y		,"right_thumbstick_y"		,"Right Thumbstick Y"	,XR_ACTION_TYPE_FLOAT_INPUT}
		,{RIGHT_HAPTIC		,"right_haptic"				,"Right Haptic"			,XR_ACTION_TYPE_VIBRATION_OUTPUT}
		});

	// Bind the actions we just created to specific locations on the Khronos simple_controller
	// definition! These are labeled as 'suggested' because they may be overridden by the runtime
	// preferences. For example, if the runtime allows you to remap buttons, or provides input
	// accessibility settings.
	#define LEFT	"/user/hand/left"
	#define RIGHT	"/user/hand/right"
	interactionProfiles.resize(3);
	auto SuggestBind = [](InteractionProfile &p)
	{
		//The application can call xrSuggestInteractionProfileBindings once per interaction profile that it supports.
		XrInteractionProfileSuggestedBinding suggested_binds = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		suggested_binds.interactionProfile = p.profilePath;
		suggested_binds.suggestedBindings = p.xrActionSuggestedBindings.data();
#if TELEPORT_INTERNAL_CHECKS
		for(suggested_binds.countSuggestedBindings=1;suggested_binds.countSuggestedBindings<=p.xrActionSuggestedBindings.size();suggested_binds.countSuggestedBindings++)
		{
			XrResult res=xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds);
			if(res!=XR_SUCCESS)
			{
				TELEPORT_CERR<<GetXRErrorString(res)<<" for path "<<FromXrPath(xr_instance,p.xrActionSuggestedBindings[suggested_binds.countSuggestedBindings-1].binding)<<std::endl;
			}
		}
#else
		suggested_binds.countSuggestedBindings = p.xrActionSuggestedBindings.size();
		XR_CHECK(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds));
#endif
	};
	InteractionProfile &khrSimpleIP	=interactionProfiles[0];
	InteractionProfile &valveIndexIP=interactionProfiles[1];
	InteractionProfile &oculusTouch	=interactionProfiles[2];
	khrSimpleIP.Init(xr_instance
			,"/interaction_profiles/khr/simple_controller"
			,{
				 {xr_input_session.actionDefinitions[SELECT].xrAction			, LEFT "/input/select/click"}
				,{xr_input_session.actionDefinitions[SELECT].xrAction			,RIGHT "/input/select/click"}
				,{xr_input_session.actionDefinitions[LEFT_GRIP_POSE].xrAction	, LEFT "/input/grip/pose"}
				,{xr_input_session.actionDefinitions[RIGHT_GRIP_POSE].xrAction	,RIGHT "/input/grip/pose"}
				,{xr_input_session.actionDefinitions[LEFT_AIM_POSE].xrAction	, LEFT "/input/aim/pose"}
				,{xr_input_session.actionDefinitions[RIGHT_AIM_POSE].xrAction	,RIGHT "/input/aim/pose"}
				,{xr_input_session.actionDefinitions[LEFT_HAPTIC].xrAction		, LEFT "/output/haptic"}
				,{xr_input_session.actionDefinitions[RIGHT_HAPTIC].xrAction		,RIGHT "/output/haptic"}
			});
	SuggestBind(khrSimpleIP);
	valveIndexIP.Init(xr_instance
		,"/interaction_profiles/valve/index_controller"
		,{
			 {xr_input_session.actionDefinitions[LEFT_GRIP_POSE].xrAction		, LEFT "/input/grip/pose"}
			,{xr_input_session.actionDefinitions[RIGHT_GRIP_POSE].xrAction		,RIGHT "/input/grip/pose"}
			,{xr_input_session.actionDefinitions[SHOW_MENU].xrAction			, LEFT "/input/b/click" }
			,{xr_input_session.actionDefinitions[SHOW_MENU].xrAction			,RIGHT "/input/b/click" }
			,{xr_input_session.actionDefinitions[A].xrAction					,RIGHT "/input/a/click" }
			,{xr_input_session.actionDefinitions[B].xrAction					,RIGHT "/input/b/click" }
			,{xr_input_session.actionDefinitions[X].xrAction					, LEFT "/input/a/click" }		// Note: a and b buttons on both controllers.
			,{xr_input_session.actionDefinitions[Y].xrAction					, LEFT "/input/b/click" }
			,{xr_input_session.actionDefinitions[LEFT_TRIGGER].xrAction			, LEFT "/input/trigger/value"}
			,{xr_input_session.actionDefinitions[RIGHT_TRIGGER].xrAction		,RIGHT "/input/trigger/value"}
			,{xr_input_session.actionDefinitions[LEFT_SQUEEZE].xrAction			, LEFT "/input/squeeze/value"}
			,{xr_input_session.actionDefinitions[RIGHT_SQUEEZE].xrAction		,RIGHT "/input/squeeze/value"}
			,{xr_input_session.actionDefinitions[LEFT_STICK_X].xrAction			, LEFT "/input/thumbstick/x"}
			,{xr_input_session.actionDefinitions[RIGHT_STICK_X].xrAction		,RIGHT "/input/thumbstick/x"}
			,{xr_input_session.actionDefinitions[LEFT_STICK_Y].xrAction			, LEFT "/input/thumbstick/y"}
			,{xr_input_session.actionDefinitions[RIGHT_STICK_Y].xrAction		,RIGHT "/input/thumbstick/y"}
		});
	SuggestBind(valveIndexIP);
	oculusTouch.Init(xr_instance
		, "/interaction_profiles/oculus/touch_controller"
		, {
			 {xr_input_session.actionDefinitions[LEFT_GRIP_POSE].xrAction		, LEFT "/input/grip/pose"}
			,{xr_input_session.actionDefinitions[RIGHT_GRIP_POSE].xrAction		,RIGHT "/input/grip/pose"}
			,{xr_input_session.actionDefinitions[SHOW_MENU].xrAction			, LEFT "/input/menu/click" }
			,{xr_input_session.actionDefinitions[A].xrAction					,RIGHT "/input/a/click" }
			,{xr_input_session.actionDefinitions[B].xrAction					,RIGHT "/input/b/click" }
			,{xr_input_session.actionDefinitions[X].xrAction					, LEFT "/input/x/click" }
			,{xr_input_session.actionDefinitions[Y].xrAction					, LEFT "/input/y/click" }
			,{xr_input_session.actionDefinitions[LEFT_TRIGGER].xrAction			, LEFT "/input/trigger/value" }
			,{xr_input_session.actionDefinitions[RIGHT_TRIGGER].xrAction		,RIGHT "/input/trigger/value" }
			,{xr_input_session.actionDefinitions[LEFT_SQUEEZE].xrAction			, LEFT "/input/squeeze/value" }
			,{xr_input_session.actionDefinitions[RIGHT_SQUEEZE].xrAction		,RIGHT "/input/squeeze/value" }
			,{xr_input_session.actionDefinitions[LEFT_STICK_X].xrAction			, LEFT "/input/thumbstick/x"	}
			,{xr_input_session.actionDefinitions[RIGHT_STICK_X].xrAction		,RIGHT "/input/thumbstick/x"}
			,{xr_input_session.actionDefinitions[LEFT_STICK_Y].xrAction			, LEFT "/input/thumbstick/y"}
			,{xr_input_session.actionDefinitions[RIGHT_STICK_Y].xrAction		,RIGHT "/input/thumbstick/y"}
		});
	SuggestBind(oculusTouch);

	// Attach the action set we just made to the session
	XrSessionActionSetsAttachInfo attach_info = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attach_info.countActionSets = 1;
	attach_info.actionSets = &xr_input_session.actionSet;
	XR_CHECK(xrAttachSessionActionSets(xr_session, &attach_info));
	xr_input_session.SessionInit(xr_session);
	RecordCurrentBindings();
}


void UseOpenXR::PollActions()
{
	if (xr_session_state != XR_SESSION_STATE_FOCUSED)
		return;

	// Update our action set with up-to-date input data!
	XrActiveActionSet action_set = { };
	action_set.actionSet = xr_input_session.actionSet;
	action_set.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo sync_info = { XR_TYPE_ACTIONS_SYNC_INFO };
	sync_info.countActiveActionSets = 1;
	sync_info.activeActionSets = &action_set;

	xrSyncActions(xr_session, &sync_info);

	// Now we'll get the current states of our actions, and store them for later use
	for(size_t i=0;i<MAX_ACTIONS;i++)
	{
		auto &def=xr_input_session.actionDefinitions[i];
		if(!def.xrAction)
			continue;
		auto &state=xr_input_session.actionStates[i];
		XrActionStateGetInfo get_info = { XR_TYPE_ACTION_STATE_GET_INFO };
		get_info.action=def.xrAction;
		switch(def.xrActionType)
		{
			case XR_ACTION_TYPE_POSE_INPUT:
			{
				XrActionStatePose pose_state	= { XR_TYPE_ACTION_STATE_POSE };
				get_info.action					= def.xrAction;
				xrGetActionStatePose(xr_session, &get_info, &pose_state);
				state.poseActive=pose_state.isActive;
			}
			break;
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
			{
				XrActionStateBoolean bool_state	= { XR_TYPE_ACTION_STATE_BOOLEAN };
				get_info.action					= def.xrAction;
				xrGetActionStateBoolean(xr_session, &get_info, &bool_state);
				state.u32=bool_state.currentState;
			}
			break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
			{
				XrActionStateFloat float_state	= { XR_TYPE_ACTION_STATE_FLOAT };
				get_info.action					= def.xrAction;
				xrGetActionStateFloat(xr_session, &get_info, &float_state);
				state.f32=float_state.currentState;
			}
			break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
			{
				XrActionStateVector2f vec2_state	= { XR_TYPE_ACTION_STATE_VECTOR2F };
				get_info.action					= def.xrAction;
				xrGetActionStateVector2f(xr_session, &get_info, &vec2_state);
				state.vec2f[0]=vec2_state.currentState.x;
				state.vec2f[1]=vec2_state.currentState.y;
			}
			break;
			default:
			break;
		};
	}
	// record the updated pose states for bound nodes:

	for (uint32_t hand = 0; hand < xr_input_session.inputDeviceStates.size(); hand++)
	{
		XrActionStateGetInfo get_info = { XR_TYPE_ACTION_STATE_GET_INFO };
		auto &inputDeviceState =xr_input_session.inputDeviceStates[hand];
	
		XrActionStatePose pose_state = { XR_TYPE_ACTION_STATE_POSE };
		get_info.action				= xr_input_session.actionDefinitions[LEFT_GRIP_POSE+hand].xrAction;
		xrGetActionStatePose(xr_session, &get_info, &pose_state);
		inputDeviceState.renderThisDevice	= pose_state.isActive;

		// Events come with a timestamp
		XrActionStateBoolean select_state = { XR_TYPE_ACTION_STATE_BOOLEAN };
		get_info.action = xr_input_session.actionDefinitions[SELECT].xrAction;
		xrGetActionStateBoolean(xr_session, &get_info, &select_state);
		xr_input_session.inputDeviceStates[hand].handSelect = select_state.currentState && select_state.changedSinceLastSync;

		get_info.action = xr_input_session.actionDefinitions[SHOW_MENU].xrAction;
		xrGetActionStateBoolean(xr_session, &get_info, &select_state);
		xr_input_session.inputDeviceStates[hand].handMenu = select_state.currentState && select_state.changedSinceLastSync;
		// If we have a select event, update the hand pose to match the event's timestamp
		if (xr_input_session.inputDeviceStates[hand].handSelect)
		{
			XrSpaceLocation space_location = { XR_TYPE_SPACE_LOCATION };
			XrResult        res = xrLocateSpace(xr_input_session.actionDefinitions[LEFT_GRIP_POSE+hand].space, xr_app_space, select_state.lastChangeTime, &space_location);
			if (XR_UNQUALIFIED_SUCCESS(res) &&
				(space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
				(space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
			{
				xr_input_session.actionStates[LEFT_GRIP_POSE+hand].pose= space_location.pose;
				if(hand>= controllerPoses.size())
					controllerPoses.resize(hand+1);
				controllerPoses[hand].position = crossplatform::ConvertPosition(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec3*)&space_location.pose.position));
				controllerPoses[hand].orientation = crossplatform::ConvertRotation(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec4*)&space_location.pose.orientation));
			}
		}
		if (xr_input_session.actionStates[SHOW_MENU].u32)
		{
			menuButtonHandler();
		}
	}
}
XrPath userHandLeftActiveProfile;
XrPath userHandRightActiveProfile;
void UseOpenXR::RecordCurrentBindings()
{
	// now we are ready to:
	XrInteractionProfileState interactionProfile={XR_TYPE_INTERACTION_PROFILE_STATE,0,0};
	// for each action, what is the binding?
	XR_CHECK(xrGetCurrentInteractionProfile(xr_session,MakeXrPath(xr_instance, "/user/hand/left"),&interactionProfile));
	std::cout<<" userHandLeftActiveProfile "<<FromXrPath(xr_instance,interactionProfile.interactionProfile).c_str()<<std::endl;
	userHandLeftActiveProfile=interactionProfile.interactionProfile;
	XR_CHECK(xrGetCurrentInteractionProfile(xr_session,MakeXrPath(xr_instance,"/user/hand/right"),&interactionProfile));
	std::cout<<"userHandRightActiveProfile "<<FromXrPath(xr_instance,interactionProfile.interactionProfile).c_str()<<std::endl;
	userHandRightActiveProfile=interactionProfile.interactionProfile;
	activeInteractionProfilePaths.clear();
	activeInteractionProfilePaths.push_back(userHandLeftActiveProfile);
	activeInteractionProfilePaths.push_back(userHandRightActiveProfile);
}

const InteractionProfile *GetActiveBinding(XrPath p)
{
	for(size_t i=0;i<interactionProfiles.size();i++)
	{
		if(interactionProfiles[i].profilePath==p)
			return &(interactionProfiles[i]);
	}
	return nullptr;
}

std::string GetBoundPath(const ActionDefinition &def)
{
	for(size_t b=0;b<activeInteractionProfilePaths.size();b++)
	{
		const InteractionProfile *interactionProfile=GetActiveBinding(activeInteractionProfilePaths[b]);
		// For each action, get the currently bound path.
		if(interactionProfile)
		for(size_t a=0;a<interactionProfile->xrActionSuggestedBindings.size();a++)
		{
			auto &binding=interactionProfile->xrActionSuggestedBindings[a];
			if(binding.action==def.xrAction)
			{
				return interactionProfile->bindingPaths[a];
			}
		}
	}
	return "";
}

// An InputMapping is created for each InputDefinition that the server has sent.
// It defines which xrActions are linked to which inputs needed by the server.
// The mappings are initialized on connection and can be changed at any time by the server.
// So we have a set of mappings for each currently connected server.
void UseOpenXR::OnInputsSetupChanged(avs::uid server_uid,const std::vector<avs::InputDefinition>& inputDefinitions_)
{
	if(!xr_session)
		return;
	RecordCurrentBindings();
	auto &inputMappings=openXRServers[server_uid].inputMappings;
	inputMappings.clear();
	auto &inputStates=openXRServers[server_uid].inputStates;
	inputStates.clear();
	for (const auto& def : inputDefinitions_)
	{
		std::regex re(def.regexPath, std::regex_constants::icase | std::regex::extended);
		// which, if any, action should be used to map to this?
		// we match by the bindings.
		// For each action, get the currently bound path.
		for(size_t a=0;a<MAX_ACTIONS;a++)
		{
			auto &actionDef=xr_input_session.actionDefinitions[a];
			std::string path_str=GetBoundPath(actionDef);
			if(!path_str.length())
				continue;
			// Now we try to match this path to the input def.
			std::smatch match;
			if (std::regex_search(path_str, match, re))
			{
				string matching=match.str(0);
				std::cout<<"Binding matches: "<<def.regexPath.c_str()<<" with "<<matching.c_str()<<std::endl;
				
				inputMappings.push_back(InputMapping());
				inputStates.push_back(InputState());
				InputMapping& mapping = inputMappings.back();
				// store the definition.
				mapping.serverInputDefinition=def;
				mapping.clientActionId=(ActionId)a;
			}
		}
	}
}

void UseOpenXR::BindUnboundPoses(avs::uid server_uid)
{
	auto &unboundPoses=openXRServers[server_uid].unboundPoses;
	auto &nodePoseMappings=openXRServers[server_uid].nodePoseMappings;
	for (std::map<avs::uid,NodePoseMapping>::iterator u = unboundPoses.begin();u!=unboundPoses.end();u++)
	{
		avs::uid uid=u->first;
		std::string regexPath=u->second.regexPath;
		std::regex re(regexPath, std::regex_constants::icase | std::regex::extended);
		// which, if any, action should be used to map to this?
		// we match by the bindings.
		// For each action, get the currently bound path.
		for(size_t a=0;a<MAX_ACTIONS;a++)
		{
			auto &actionDef=xr_input_session.actionDefinitions[a];
			if(actionDef.xrActionType!=XR_ACTION_TYPE_POSE_INPUT)
				continue;
			std::string path_str=GetBoundPath(actionDef);
			if(!path_str.length())
				continue;
			// Now we try to match this path to the input def.
			std::smatch match;
			if (std::regex_search(path_str, match, re))
			{
				string matching=match.str(0);
				TELEPORT_COUT<<"Node "<<uid<<" pose Binding matches: "<<regexPath.c_str()<<" with "<<matching.c_str()<<std::endl;
				
				nodePoseMappings[uid].actionId=(ActionId)a;
				nodePoseMappings[uid].poseOffset=u->second.poseOffset;
				nodePoseMappings[uid].regexPath=u->second.regexPath;
				u=unboundPoses.begin();
				unboundPoses.erase(uid);
			}
		}
	}
	if(unboundPoses.size())
		std::cout<<unboundPoses.size()<<" poses remain unbound."<<std::endl;
}

void UseOpenXR::MapNodeToPose(avs::uid server_uid,avs::uid uid,const std::string &regexPath)
{
	avs::Pose poseOffset;
	auto &unboundPoses=openXRServers[server_uid].unboundPoses;
	unboundPoses[uid].regexPath=regexPath;
	unboundPoses[uid].poseOffset=poseOffset;
	if(!xr_session)
		return;
	BindUnboundPoses(server_uid);
}
void UseOpenXR::UpdateServerState(avs::uid server_uid,unsigned long long framenumber)
{
	auto &server=openXRServers[server_uid];
	if(server.framenumber!=framenumber)
	{
		for(auto m:server.nodePoseMappings)
		{
			auto &def=m.second;
			auto &state=server.nodePoseStates[m.first];
			XrSpaceLocation space_location = { XR_TYPE_SPACE_LOCATION };
			XrResult        res = xrLocateSpace(xr_input_session.actionDefinitions[def.actionId].space, xr_app_space, lastTime, &space_location);
			if (XR_UNQUALIFIED_SUCCESS(res) &&
				(space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
				(space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
			{
				xr_input_session.actionStates[def.actionId].pose= space_location.pose;
				state.pose.position = crossplatform::ConvertPosition(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec3*)&space_location.pose.position));
				state.pose.orientation = crossplatform::ConvertRotation(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec4*)&space_location.pose.orientation));
			}
		}
		server.inputs.clear();
		for(size_t i=0;i<server.inputMappings.size();i++)
		{
			auto &mapping=server.inputMappings[i];
			auto &state=server.inputStates[i];
			const auto &actionState		=xr_input_session.actionStates[mapping.clientActionId];
			const auto &actionDefinition=xr_input_session.actionDefinitions[mapping.clientActionId];
			switch(actionDefinition.xrActionType)
			{
				case XR_ACTION_TYPE_BOOLEAN_INPUT:
				{
					if(actionState.u32!=state.uint32)
					{
						state.uint32=actionState.u32;
						server.inputs.addBinaryEvent(mapping.serverInputDefinition.inputId,actionState.u32!=0);
					}
				}
				break;
				case XR_ACTION_TYPE_FLOAT_INPUT:
				{
					if(actionState.f32!=state.float32)
					{
						state.float32=actionState.f32;
						server.inputs.addAnalogueEvent(mapping.serverInputDefinition.inputId,actionState.f32);
					}
				}
				break;
				case XR_ACTION_TYPE_VIBRATION_OUTPUT:
				{
				}
				break;
				default:
				break;
			};
		}
		server.framenumber=framenumber;
	}
}

const teleport::client::Input &UseOpenXR::GetServerInputs(avs::uid server_uid,unsigned long long framenumber)
{
	UpdateServerState(server_uid, framenumber);
	auto &server=openXRServers[server_uid];
	return server.inputs;
}

const std::map<avs::uid,NodePoseState> &UseOpenXR::GetNodePoseStates(avs::uid server_uid,unsigned long long framenumber)
{
	UpdateServerState(server_uid, framenumber);
	const std::map<avs::uid,NodePoseState> &nodePoseStates=openXRServers[server_uid].nodePoseStates;
	return nodePoseStates;
}

void UseOpenXR::openxr_poll_predicted(XrTime predicted_time)
{
	if (xr_session_state != XR_SESSION_STATE_FOCUSED)
		return;

	// Update hand position based on the predicted time of when the frame will be rendered! This 
	// should result in a more accurate location, and reduce perceived lag.
	for (size_t i = 0; i < 2; i++)
	{
		if (!xr_input_session.inputDeviceStates[i].renderThisDevice)
			continue;
		XrSpaceLocation space_location = { XR_TYPE_SPACE_LOCATION };
		XrResult        res = xrLocateSpace(xr_input_session.actionDefinitions[LEFT_GRIP_POSE+i].space, xr_app_space, predicted_time, &space_location);
		if (XR_UNQUALIFIED_SUCCESS(res) &&
			(space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
			(space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
		{
			xr_input_session.actionStates[LEFT_GRIP_POSE+i].pose = space_location.pose;
			if (i >= controllerPoses.size())
				controllerPoses.resize(i + 1);
			controllerPoses[i].position = platform::crossplatform::ConvertPosition(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec3*)&space_location.pose.position));
			controllerPoses[i].orientation = platform::crossplatform::ConvertRotation(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec4*)&space_location.pose.orientation));
		}
	}
}

void app_update_predicted()
{
	// Update the location of the hand cubes. This is done after the inputs have been updated to 
	// use the predicted location, but during the render code, so we have the most up-to-date location.
	//if (app_cubes.size() < 2)
	//	app_cubes.resize(2, xr_pose_identity);
	//for (uint32_t i = 0; i < 2; i++) {
	//	app_cubes[i] = xr_input_session.renderHand[i] ? xr_input_session.handPose[i] : xr_pose_identity;
	//}
}

 mat4  AffineTransformation(vec4 q,vec3 p)
{
	 platform::crossplatform::Quaternion<float> rotation = (const float*)&q;
	 vec3 Translation = (const float*)&p;
	 vec4 VTranslation = { Translation.x,Translation.y,Translation.z,1.0f };
	 mat4 M;
	 platform::crossplatform::QuaternionToMatrix(M, rotation);
	 vec4 row3 = M.M[3];
	 row3 = row3 + VTranslation;
	 M.M[3][0] = row3.x;
	 M.M[3][1] = row3.y;
	 M.M[3][2] = row3.z;
	 M.M[3][3] = row3.w;
	 return M;
}

mat4 MatrixPerspectiveOffCenterRH
(
	float ViewLeft,
	float ViewRight,
	float ViewBottom,
	float ViewTop,
	float NearZ,
	float FarZ
)
{
	float TwoNearZ = NearZ + NearZ;
	float ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
	float ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
	float fRange = FarZ / (NearZ - FarZ);

	mat4 M;
	M.M[0][0] = TwoNearZ * ReciprocalWidth;
	M.M[0][1] = 0.0f;
	M.M[0][2] = 0.0f;
	M.M[0][3] = 0.0f;

	M.M[1][0] = 0.0f;
	M.M[1][1] = TwoNearZ * ReciprocalHeight;
	M.M[1][2] = 0.0f;
	M.M[1][3] = 0.0f;

	M.M[2][0] = (ViewLeft + ViewRight) * ReciprocalWidth;
	M.M[2][1] = (ViewTop + ViewBottom) * ReciprocalHeight;
	M.M[2][2] = 0.f;// NearZ / (FarZ - NearZ); //fRange;
	M.M[2][3] = -1.0f;

	M.M[3][0] = 0.0f;
	M.M[3][1] = 0.0f;
	M.M[3][2] = NearZ;// FarZ* NearZ / (FarZ - NearZ); //fRange * NearZ;
	M.M[3][3] = 0.0f;
	return M;
}
mat4 xr_projection(XrFovf fov, float clip_near, float clip_far)
{
	const float left = clip_near * tanf(fov.angleLeft);
	const float right = clip_near * tanf(fov.angleRight);
	const float down = clip_near * tanf(fov.angleDown);
	const float up = clip_near * tanf(fov.angleUp);

	return MatrixPerspectiveOffCenterRH(left, right, down, up, clip_near, clip_far);
}

void UseOpenXR::RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext,XrCompositionLayerProjectionView& view
	, swapchain_surfdata_t& surface, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation)
{
	// Set up camera matrices based on OpenXR's predicted viewpoint information
	mat4 proj = xr_projection(view.fov, 0.1f, 200.0f);
	crossplatform::Quaternionf rot = crossplatform::ConvertRotation(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const crossplatform::Quaternionf*)&view.pose.orientation));
	vec3 pos=crossplatform::ConvertPosition(crossplatform::AxesStandard::OpenGL,crossplatform::AxesStandard::Engineering,*((const vec3 *)&view.pose.position));
	crossplatform::Quaternionf orig_rot = origin_orientation;
	Multiply(pos,orig_rot,pos);
	pos += origin_pos;
	deviceContext.viewStruct.proj = *((const platform::math::Matrix4x4*)&proj); 

	rot=orig_rot*rot;

	platform::math::SimulOrientation globalOrientation;
	// global pos/orientation:
	globalOrientation.SetPosition((const float*)&pos);

	platform::math::Quaternion q0(3.1415926536f / 2.0f, platform::math::Vector3(-1.f, 0.0f, 0.0f));
	platform::math::Quaternion q1 = (const float*)&rot;

	auto q_rel = q1 / q0;
	globalOrientation.SetOrientation(q_rel);
	
	deviceContext.viewStruct.view = globalOrientation.GetInverseMatrix().RowPointer(0);
	deviceContext.viewStruct.Init();

	// Set up where on the render target we want to draw, the view has a 
	XrRect2Di& rect = view.subImage.imageRect;
	crossplatform::Viewport viewport{ (int)rect.offset.x, (int)rect.offset.y, (int)rect.extent.width, (int)rect.extent.height };
	renderPlatform->SetViewports(deviceContext,1,&viewport);

	// Wipe our swapchain color and depth target clean, and then set them up for rendering!
	static float clear[] = { 0.2f, 0.3f, 0.5f, 1 };
	renderPlatform->ActivateRenderTargets(deviceContext,1, &surface.target_view, surface.depth_view);
	renderPlatform->Clear(deviceContext, clear);
	surface.depth_view->ClearDepthStencil(deviceContext, 0.0f, 0);

	// And now that we're set up, pass on the rest of our rendering to the application
	renderDelegate(deviceContext);
	renderPlatform->DeactivateRenderTargets(deviceContext);
}

platform::crossplatform::Texture* UseOpenXR::GetRenderTexture(int index)
{
	if (index < 0 || index >= xr_swapchains.size())
		return nullptr;
	auto sw = xr_swapchains[index];
	if (sw.last_img_id < 0 || sw.last_img_id >= sw.surface_data.size())
		return nullptr;
	return sw.surface_data[sw.last_img_id].target_view;
}

bool UseOpenXR::RenderLayer(platform::crossplatform::GraphicsDeviceContext& deviceContext, XrTime predictedTime
	, vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer
	, platform::crossplatform::RenderDelegate& renderDelegate, vec3 origin_pos, vec4 origin_orientation)
{
	lastTime=predictedTime;
	// Find the state and location of each viewpoint at the predicted time
	uint32_t         view_count = 0;
	XrViewState      view_state = { XR_TYPE_VIEW_STATE };
	XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
	locate_info.viewConfigurationType = app_config_view;
	locate_info.displayTime = predictedTime;
	locate_info.space = xr_app_space;
	xrLocateViews(xr_session, &locate_info, &view_state, (uint32_t)xr_views.size(), &view_count, xr_views.data());
	views.resize(view_count);
	static int64_t frame = 0;
	frame++;
	// And now we'll iterate through each viewpoint, and render it!
	for (uint32_t i = 0; i < view_count; i++)
	{
		// We need to ask which swapchain image to use for rendering! Which one will we get?
		// Who knows! It's up to the runtime to decide.
		uint32_t                    img_id;
		XrSwapchainImageAcquireInfo acquire_info = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		xrAcquireSwapchainImage(xr_swapchains[i].handle, &acquire_info, &img_id);
		xr_swapchains[i].last_img_id = img_id;
		// Wait until the image is available to render to. The compositor could still be
		// reading from it.
		XrSwapchainImageWaitInfo wait_info = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		wait_info.timeout = XR_INFINITE_DURATION;
		xrWaitSwapchainImage(xr_swapchains[i].handle, &wait_info);

		// Set up our rendering information for the viewpoint we're using right now!
		views[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
		views[i].pose = xr_views[i].pose;
		views[i].fov = xr_views[i].fov;
		views[i].subImage.swapchain = xr_swapchains[i].handle;
		views[i].subImage.imageRect.offset = { 0, 0 };
		views[i].subImage.imageRect.extent = { xr_swapchains[i].width, xr_swapchains[i].height };

		deviceContext.setDefaultRenderTargets(nullptr,nullptr, 0, 0, xr_swapchains[i].width, xr_swapchains[i].height
			,&xr_swapchains[i].surface_data[img_id].target_view,1, xr_swapchains[i].surface_data[img_id].depth_view);
		
		deviceContext.renderPlatform = renderPlatform;

		deviceContext.viewStruct.view_id = i;
		deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;
		// Call the rendering callback with our view and swapchain info
		RenderLayer(deviceContext,views[i], xr_swapchains[i].surface_data[img_id],renderDelegate, origin_pos,origin_orientation);

		// And tell OpenXR we're done with rendering to this one!
		XrSwapchainImageReleaseInfo release_info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		xrReleaseSwapchainImage(xr_swapchains[i].handle, &release_info);
	}

	layer.space = xr_app_space;
	layer.viewCount = (uint32_t)views.size();
	layer.views = views.data();
	return true;
}

void UseOpenXR::PollEvents(bool& exit)
{
	exit = false;

	XrEventDataBuffer event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };
	XrResult res;
	res=xrPollEvent(xr_instance, &event_buffer);
	while ( res== XR_SUCCESS)
	{
		switch (event_buffer.type)
		{
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			XrEventDataSessionStateChanged* changed = (XrEventDataSessionStateChanged*)&event_buffer;
			xr_session_state = changed->state;

			// Session state change is where we can begin and end sessions, as well as find quit messages!
			switch (xr_session_state)
			{
			case XR_SESSION_STATE_READY:
				{
					XrSessionBeginInfo begin_info = { XR_TYPE_SESSION_BEGIN_INFO };
					begin_info.primaryViewConfigurationType = app_config_view;
					if(xrBeginSession(xr_session, &begin_info)==XR_SUCCESS)
					{
						TELEPORT_COUT<<"Beginning OpenXR Session."<<std::endl;
						xr_running = true;
					}
				}
			break;
			case XR_SESSION_STATE_FOCUSED:
				break;
			case XR_SESSION_STATE_VISIBLE :
				break;
			case XR_SESSION_STATE_STOPPING:
				{
					xr_running = false;
					xrEndSession(xr_session);
				}
				break;
			case XR_SESSION_STATE_EXITING:
				exit = true;
				break;
			case XR_SESSION_STATE_LOSS_PENDING:
				exit = true; 
				break;
			}
		} break;
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			exit = true;
			return;
		}
		event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };
		res=xrPollEvent(xr_instance, &event_buffer);
	}
}

bool UseOpenXR::HaveXRDevice() const
{
	return UseOpenXR::haveXRDevice;
}

bool UseOpenXR::IsXRDeviceActive() const
{
	if(!UseOpenXR::haveXRDevice)
		return false;
	return (xr_session_state == XR_SESSION_STATE_VISIBLE || xr_session_state == XR_SESSION_STATE_FOCUSED);
}

const avs::Pose& UseOpenXR::GetHeadPose() const
{
	return headPose;
}

const avs::Pose& UseOpenXR::GetControllerPose(int index) const
{
	if (index >= 0 && controllerPoses.size())
		return controllerPoses[index];
	else
		return avs::Pose();
}

void UseOpenXR::RenderFrame(platform::crossplatform::GraphicsDeviceContext	&deviceContext,platform::crossplatform::RenderDelegate &renderDelegate,vec3 origin_pos,vec4 origin_orientation)
{
	// Block until the previous frame is finished displaying, and is ready for another one.
	// Also returns a prediction of when the next frame will be displayed, for use with predicting
	// locations of controllers, viewpoints, etc.
	XrFrameState frame_state = { XR_TYPE_FRAME_STATE };
	xrWaitFrame(xr_session, nullptr, &frame_state);
	// Must be called before any rendering is done! This can return some interesting flags, like 
	// XR_SESSION_VISIBILITY_UNAVAILABLE, which means we could skip rendering this frame and call
	// xrEndFrame right away.
	xrBeginFrame(xr_session, nullptr);

	// Execute any code that's dependant on the predicted time, such as updating the location of
	// controller models.
	openxr_poll_predicted(frame_state.predictedDisplayTime);
	app_update_predicted();

	XrSpaceLocation space_location = { XR_TYPE_SPACE_LOCATION };
	XrResult        res = xrLocateSpace(xr_head_space, xr_app_space, frame_state.predictedDisplayTime, &space_location);
	if (XR_UNQUALIFIED_SUCCESS(res) &&
		(space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
		(space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
	{
		headPose.position	= crossplatform::ConvertPosition(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec3*)&space_location.pose.position));
 		headPose.orientation= crossplatform::ConvertRotation(crossplatform::AxesStandard::OpenGL, crossplatform::AxesStandard::Engineering, *((const vec4*)&space_location.pose.orientation));
	}
	// If the session is active, lets render our layer in the compositor!
	XrCompositionLayerBaseHeader* layer			= nullptr;
	XrCompositionLayerProjection  layer_proj	= { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	vector<XrCompositionLayerProjectionView> views;
	bool session_active = xr_session_state == XR_SESSION_STATE_VISIBLE || xr_session_state == XR_SESSION_STATE_FOCUSED;
	if (session_active && RenderLayer(deviceContext,frame_state.predictedDisplayTime, views, layer_proj,renderDelegate, origin_pos, origin_orientation))
	{
		layer = (XrCompositionLayerBaseHeader*)&layer_proj;
	}

	// We're finished with rendering our layer, so send it off for display!
	XrFrameEndInfo end_info{ XR_TYPE_FRAME_END_INFO };
	end_info.displayTime = frame_state.predictedDisplayTime;
	end_info.environmentBlendMode = xr_blend;
	end_info.layerCount = layer == nullptr ? 0 : 1;
	end_info.layers = &layer;
	xrEndFrame(xr_session, &end_info);
}

void UseOpenXR::Shutdown()
{
	haveXRDevice = false;
	// We used a graphics API to initialize the swapchain data, so we'll
	// give it a chance to release anythig here!
	for (int32_t i = 0; i < xr_swapchains.size(); i++)
	{
		xrDestroySwapchain(xr_swapchains[i].handle);
		swapchain_destroy(xr_swapchains[i]);
	}
	xr_swapchains.clear();

	// Release all the other OpenXR resources that we've created!
	// What gets allocated, must get deallocated!
	for(int i=0;i<MAX_ACTIONS;i++)
	{
		if (xr_input_session.actionDefinitions[i].space != XR_NULL_HANDLE)
			xrDestroySpace(xr_input_session.actionDefinitions[i].space);
	}
	if (xr_input_session.actionSet != XR_NULL_HANDLE)
	{
		xrDestroyActionSet(xr_input_session.actionSet);
	}
	if (xr_app_space != XR_NULL_HANDLE)
		xrDestroySpace(xr_app_space);
	if (xr_session != XR_NULL_HANDLE)
		xrDestroySession(xr_session);
	if (xr_debug != XR_NULL_HANDLE)
		ext_xrDestroyDebugUtilsMessengerEXT(xr_debug);
	if (xr_instance != XR_NULL_HANDLE)
		xrDestroyInstance(xr_instance);
}

const std::string &UseOpenXR::GetDebugString() const
{
	static std::string str;
	str.clear();
	str+="Bound profile paths:\n";
	for(int i=0;i<activeInteractionProfilePaths.size();i++)
	{
		if(i)
			str+=", ";
		if(activeInteractionProfilePaths[i])
			str+=FromXrPath(xr_instance,activeInteractionProfilePaths[i]);
	}
	str+="\n";
	for(int i=0;i<MAX_ACTIONS;i++)
	{
		auto &def=xr_input_session.actionDefinitions[i];
		auto &state=xr_input_session.actionStates[i];
		str+=def.name+": ";
		switch(def.xrActionType)
		{
			case XR_ACTION_TYPE_POSE_INPUT:
			{
			}
			break;
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
			{
				str+=fmt::format("{0}",state.u32!=0?"true":"false");
			}
			break;
			case XR_ACTION_TYPE_FLOAT_INPUT:
			{
				str+=fmt::format("{: .3f}",state.f32);
			}
			break;
			case XR_ACTION_TYPE_VECTOR2F_INPUT:
			{
				str+=fmt::format("{0},{1}",state.vec2f[0],state.vec2f[1]);
			}
			break;
			case XR_ACTION_TYPE_VIBRATION_OUTPUT:
			{
			}
			break;
			default:
			break;
		};
		str+="\n";
	}
	return str;
}