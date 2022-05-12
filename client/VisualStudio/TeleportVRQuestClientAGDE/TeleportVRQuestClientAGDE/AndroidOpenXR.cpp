#if 1
#include "AndroidOpenXR.h"
#include <android/log.h>

#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <sys/system_properties.h>
#include <vulkan/vulkan.hpp>
#define XR_USE_GRAPHICS_API_VULKAN 1
#define XR_USE_PLATFORM_ANDROID 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_oculus_helpers.h>
#include <fmt/core.h>

using namespace platform;
using namespace teleport;
using namespace android;
using namespace std;

const XrPosef	xr_pose_identity = { {0,0,0,1}, {0,0,0} };
#define TELEPORT_LOG_TAG "Teleport"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TELEPORT_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TELEPORT_LOG_TAG, __VA_ARGS__)

#if defined(DEBUG)
void OXR_CheckErrors(XrInstance instance, XrResult result, const char* function, bool failOnError)
{
	if (XR_FAILED(result))
	{
		char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString(instance, result, errorBuffer);
		if (failOnError)
		{
			ALOGE("OpenXR error: %s: %s\n", function, errorBuffer);
		}
		else
		{
			ALOGV("OpenXR error: %s: %s\n", function, errorBuffer);
		}
	}
}
#endif

client::swapchain_surfdata_t CreateSurfaceData(crossplatform::RenderPlatform* renderPlatform, XrBaseInStructure& swapchain_img, int64_t d3d_swapchain_fmt)
{
	client::swapchain_surfdata_t result = {};
	// Get information about the swapchain image that OpenXR made for us!
	XrSwapchainImageVulkanKHR& vulkan_swapchain_img = (XrSwapchainImageVulkanKHR&)swapchain_img;
	result.target_view = renderPlatform->CreateTexture("swapchain target");
	result.target_view->InitFromExternalTexture2D(renderPlatform, vulkan_swapchain_img.image, nullptr, 0, 0, platform::crossplatform::UNKNOWN, true);
	result.depth_view = renderPlatform->CreateTexture("swapchain depth");
	platform::crossplatform::TextureCreate textureCreate = {};
	textureCreate.numOfSamples = std::max(1, result.target_view->GetSampleCount());
	textureCreate.mips = 1;
	textureCreate.w = result.target_view->width;
	textureCreate.l = result.target_view->length;
	textureCreate.arraysize = 1;
	textureCreate.f = platform::crossplatform::PixelFormat::D_32_FLOAT;
	textureCreate.setDepthStencil = true;
	textureCreate.computable = false;
	result.depth_view->EnsureTexture(renderPlatform, &textureCreate);

	return result;
}

bool OpenXR::TryInitDevice()
{
	// Request a form factor from the device (HMD, Handheld, etc.)
	// If the device is not on, not connected, or its app is not running, this may fail here:
	XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = app_config_form;
	if (!CheckXrResult(xr_instance, xrGetSystem(xr_instance, &systemInfo, &xr_system_id)))
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

	PFN_xrGetVulkanGraphicsRequirementsKHR ext_xrGetVulkanGraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(xr_instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetVulkanGraphicsRequirementsKHR));
	XrGraphicsRequirementsVulkanKHR requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
	ext_xrGetVulkanGraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBindingVulkanKHR binding = { XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
	binding.device = renderPlatform->AsVulkanDevice()->operator VkDevice();

	XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.next = &binding;
	sessionInfo.systemId = xr_system_id;
	if (!CheckXrResult(xr_instance, xrCreateSession(xr_instance, &sessionInfo, &xr_session)))
	{
		std::cerr << fmt::format("Failed to create XR Session\n").c_str() << std::endl;
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
	static int samples = 1;
	for (uint32_t i = 0; i < view_count; i++)
	{
		xr_config_views[i].maxSwapchainSampleCount = samples;
		xr_config_views[i].recommendedSwapchainSampleCount = samples;
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
	std::cout << "xrEnumerateSwapchainFormats:\n";
	for (auto f : formats)
	{
		vk::Format F = (vk::Format)f;
		std::cout << "    format " << f << std::endl;
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
		// DXGI_FORMAT_R8G8B8A8_TYPELESS. When creating an IVulkanRenderTargetView for the swapchain texture, we must specify
		// a concrete type like DXGI_FORMAT_R8G8B8A8_UNORM, as attempting to create a TYPELESS view will throw errors, so 
		// we do need to store the format separately and remember it later.
		XrViewConfigurationView& view = xr_config_views[i];
		XrSwapchainCreateInfo    swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		XrSwapchain              handle;
		swapchain_info.arraySize = 1;
		swapchain_info.mipCount = 1;
		swapchain_info.faceCount = 1;
		swapchain_info.format = swapchain_format;
		swapchain_info.width = view.recommendedImageRectWidth;
		swapchain_info.height = view.recommendedImageRectHeight;
		swapchain_info.sampleCount = view.recommendedSwapchainSampleCount;
		swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrCreateSwapchain(xr_session, &swapchain_info, &handle);

		// Find out how many textures were generated for the swapchain
		uint32_t surface_count = 0;
		xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

		// We'll want to track our own information about the swapchain, so we can draw stuff onto it! We'll also create
		// a depth buffer for each generated texture here as well with make_surfacedata.
		client::swapchain_t swapchain = {};
		swapchain.width = swapchain_info.width;
		swapchain.height = swapchain_info.height;
		swapchain.handle = handle;

		vector<XrSwapchainImageVulkanKHR> surface_images;
		surface_images.resize(surface_count, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR });

		swapchain.surface_data.resize(surface_count);
		xrEnumerateSwapchainImages(swapchain.handle, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)surface_images.data());
		for (uint32_t i = 0; i < surface_count; i++)
		{
			swapchain.surface_data[i] = CreateSurfaceData(renderPlatform, (XrBaseInStructure&)surface_images[i], swapchain_format);
		}
		xr_swapchains.push_back(swapchain);
	}

	haveXRDevice = true;
	return true;
}

const char* OpenXR::GetOpenXRGraphicsAPIExtensionName() const
{
}

std::vector<std::string> OpenXR::GetRequiredExtensions() const
{
	std::vector<std::string> str;
	str.push_back(GetOpenXRGraphicsAPIExtensionName());
	str.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
	str.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
	str.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
	str.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
	str.push_back(XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME);
	str.push_back(XR_FB_SWAPCHAIN_UPDATE_STATE_VULKAN_EXTENSION_NAME);
	str.push_back(XR_FB_SPACE_WARP_EXTENSION_NAME);
	return str;
}
#endif