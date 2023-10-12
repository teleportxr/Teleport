#if TELEPORT_CLIENT_USE_D3D12
#include <d3d12.h>
#define XR_USE_GRAPHICS_API_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#endif
#if TELEPORT_CLIENT_USE_D3D11
#include <d3d11.h>
#define XR_USE_GRAPHICS_API_D3D11
#include "Platform/DirectX11/RenderPlatform.h"
#endif
#if TELEPORT_CLIENT_USE_VULKAN
#include <vulkan/vulkan.hpp>
#define XR_USE_GRAPHICS_API_VULKAN
#include "Platform/Vulkan/RenderPlatform.h"
#endif
#define XR_USE_PLATFORM_WIN32
#include "UseOpenXR.h"

#include <openxr/openxr.h>
#include <WTypesbase.h>	// for LARGE_INTEGER  etc
#include <Unknwn.h>	// for  IUnknown etc
#include <openxr/openxr_platform.h>

#include "fmt/core.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "Platform/CrossPlatform/AxesStandard.h"
#include "TeleportCore/ErrorHandling.h"

#include <regex>

using namespace std;
using namespace platform;
using namespace teleport;
using namespace client;

const XrPosef xr_pose_identity = { {0,0,0,1}, {0,0,0} };

const char *UseOpenXR::GetOpenXRGraphicsAPIExtensionName() const
{
#if TELEPORT_CLIENT_USE_VULKAN
	return XR_KHR_VULKAN_ENABLE_EXTENSION_NAME; // Use Direct3D11 for rendering
#endif
#if TELEPORT_CLIENT_USE_D3D12
	return XR_KHR_D3D12_ENABLE_EXTENSION_NAME; // Use Direct3D12 for rendering
#endif
#if TELEPORT_CLIENT_USE_D3D11
	return XR_KHR_D3D11_ENABLE_EXTENSION_NAME; // Use Direct3D11 for rendering
#endif
}

swapchain_surfdata_t CreateSurfaceData(crossplatform::RenderPlatform *renderPlatform,XrBaseInStructure& swapchain_img, XrSwapchainCreateInfo swapchain_info)
{
	swapchain_surfdata_t result = {};
	platform::crossplatform::TextureCreate textureCreate = {};

	// Get information about the swapchain image that OpenXR made for us!
#if TELEPORT_CLIENT_USE_VULKAN
	XrSwapchainImageVulkanKHR &api_swapchain_img = (XrSwapchainImageVulkanKHR &)swapchain_img;
	textureCreate.external_texture = api_swapchain_img.image;
	platform::crossplatform::PixelFormat pixelFormat = platform::vulkan::RenderPlatform::FromVulkanFormat((vk::Format)swapchain_info.format);
#endif
#if TELEPORT_CLIENT_USE_D3D12
	XrSwapchainImageD3D12KHR &api_swapchain_img = (XrSwapchainImageD3D12KHR &)swapchain_img;
	textureCreate.external_texture = api_swapchain_img.texture;
	platform::crossplatform::PixelFormat pixelFormat = platform::dx12::RenderPlatform::FromDxgiFormat((DXGI_FORMAT)swapchain_info.format);
#endif
#if TELEPORT_CLIENT_USE_D3D11
	XrSwapchainImageD3D11KHR &api_swapchain_img = (XrSwapchainImageD3D11KHR &)swapchain_img;
	textureCreate.external_texture = api_swapchain_img.texture;
	platform::crossplatform::PixelFormat pixelFormat = platform::dx11::RenderPlatform::FromDxgiFormat((DXGI_FORMAT)swapchain_info.format);
#endif

	textureCreate.w					= swapchain_info.width;
	textureCreate.l					= swapchain_info.height;
	textureCreate.arraysize			= swapchain_info.arraySize;
	textureCreate.make_rt			= true;
	textureCreate.computable		= false;
	textureCreate.f					= pixelFormat;
	textureCreate.numOfSamples		= std::max((uint32_t)1, swapchain_info.sampleCount);
	result.target_view				= renderPlatform->CreateTexture("swapchain target");
	result.target_view->InitFromExternalTexture(renderPlatform, &textureCreate);

	textureCreate.external_texture	= nullptr;
	textureCreate.w					= result.target_view->width;
	textureCreate.l					= result.target_view->length;
	textureCreate.f					= platform::crossplatform::PixelFormat::D_32_FLOAT;
	textureCreate.make_rt			= false;
	textureCreate.setDepthStencil	= true;
	textureCreate.numOfSamples		= std::max(1,result.target_view->GetSampleCount());
	result.depth_view				= renderPlatform->CreateTexture("swapchain depth");
	result.depth_view->EnsureTexture(renderPlatform, &textureCreate);
	return result;
}

bool UseOpenXR::StartSession()
{
	if(!CanStartSession())
		return false;

	// Check what blend mode is valid for this device (opaque vs transparent displays)
	// We'll just take the first one available!
	uint32_t blend_count = 0;
	XrResult res=xrEnumerateEnvironmentBlendModes(xr_instance, xr_system_id, app_config_view, 1, &blend_count, &xr_environment_blend);
	if(res!=XrResult::XR_SUCCESS)
	{
		xr_system_id=XR_NULL_SYSTEM_ID;
		return false;
	}

	// OpenXR wants to ensure apps are using the correct graphics card, so this MUST be called 
	// before xrCreateSession. This is crucial on devices that have multiple graphics cards, 
	// like laptops with integrated graphics chips in addition to dedicated graphics cards.
#if TELEPORT_CLIENT_USE_D3D12
	PFN_xrGetD3D12GraphicsRequirementsKHR ext_xrGetD3D12GraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(xr_instance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D12GraphicsRequirementsKHR));
	XrGraphicsRequirementsD3D12KHR requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
	ext_xrGetD3D12GraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBindingD3D12KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
	binding.device = renderPlatform->AsD3D12Device();
	auto *rp12=(platform::dx12::RenderPlatform*)renderPlatform;
	binding.queue = rp12->GetCommandQueue();
#endif
#if TELEPORT_CLIENT_USE_VULKAN
	PFN_xrGetVulkanGraphicsRequirementsKHR ext_xrGetVulkanGraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(xr_instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction *)(&ext_xrGetVulkanGraphicsRequirementsKHR));
	XrGraphicsRequirementsVulkanKHR requirement = {XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
	ext_xrGetVulkanGraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBindingVulkanKHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
	binding.device = renderPlatform->AsVulkanDevice()->operator VkDevice();
	binding.instance = renderPlatform->AsVulkanInstance()->operator VkInstance();
	PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR;
	xrGetInstanceProcAddr(xr_instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR);
	VkPhysicalDevice vkPhysicalDevice;
	if (xrGetVulkanGraphicsDeviceKHR != NULL)
	{
		xrGetVulkanGraphicsDeviceKHR(xr_instance, xr_system_id, renderPlatform->AsVulkanInstance()->operator VkInstance(), &vkPhysicalDevice);
	}
	binding.physicalDevice = vkPhysicalDevice; // vulkanPhysicalDevice->operator VkPhysicalDevice();
	auto *vk = (platform::vulkan::RenderPlatform *)renderPlatform;
	//binding.queueFamilyIndex = vk->GetCommandQueue();
#endif
#if TELEPORT_CLIENT_USE_D3D11
	PFN_xrGetD3D11GraphicsRequirementsKHR ext_xrGetD3D11GraphicsRequirementsKHR = nullptr;
	xrGetInstanceProcAddr(xr_instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D11GraphicsRequirementsKHR));
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
	res=xrCreateSession(xr_instance, &sessionInfo, &xr_session);
	if (!CheckXrResult(xr_instance,res))
	{
		xr_system_id=XR_NULL_SYSTEM_ID;
		std::cerr<<fmt::format("Failed to create XR Session\n").c_str() << std::endl;
		return false;
	}

	// Unable to start a session, may not have an MR device attached or ready
	if (xr_session == nullptr)
	{
		xr_system_id=XR_NULL_SYSTEM_ID;
		return false;
	}


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

	// Check the two eyes for stereo are the same
	XrViewConfigurationView& config_view = xr_config_views[0];
	if (app_config_view == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO && xr_config_views.size() == 2)
	{
		SIMUL_ASSERT(xr_config_views[0].recommendedImageRectWidth == xr_config_views[1].recommendedImageRectWidth)
		SIMUL_ASSERT(xr_config_views[0].recommendedImageRectHeight == xr_config_views[1].recommendedImageRectHeight)
	}

	// Find out what format to use:
#if TELEPORT_CLIENT_USE_VULKAN
	int64_t swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
	constexpr int64_t SupportedColorSwapchainFormats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
#else
	int64_t swapchain_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr int64_t SupportedColorSwapchainFormats[] = { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM };
#endif
	uint32_t formatCount = 0;
	res = xrEnumerateSwapchainFormats(xr_session, 0, &formatCount, nullptr);
	if (!formatCount)
	{
		xr_system_id=XR_NULL_SYSTEM_ID;
		return false;
	}


	std::vector<int64_t> formats(formatCount);
	res = xrEnumerateSwapchainFormats(xr_session, formatCount, &formatCount, formats.data());
#if 0
	std::cout<<"xrEnumerateSwapchainFormats:\n";
	for(auto f:formats)
	{
		DXGI_FORMAT F=(DXGI_FORMAT)f;
		std::cout<<"    "<<F<<std::endl;
	}
#endif
	auto swapchainFormatIt = std::find_first_of(formats.begin(), formats.end(), std::begin(SupportedColorSwapchainFormats), std::end(SupportedColorSwapchainFormats));
	if (swapchainFormatIt == formats.end())
	{
		xr_system_id=XR_NULL_SYSTEM_ID;
		throw("No runtime swapchain format supported for color swapchain");
		return false;
	}
	swapchain_format = *swapchainFormatIt;

	//Add XR Swapchain Lambda
	auto AddXrSwapchain = [&, this](XrSwapchainCreateInfo swapchain_info) -> void
	{
		XrSwapchain handle;
		XR_CHECK(xrCreateSwapchain(xr_session, &swapchain_info, &handle));

		// Find out how many textures were generated for the swapchain
		uint32_t surface_count = 0;
		xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

		// We'll want to track our own information about the swapchain, so we can draw stuff onto it! We'll also create
		// a depth buffer for each generated texture here as well with make_surfacedata.
		client::swapchain_t swapchain = {};
		swapchain.width = swapchain_info.width;
		swapchain.height = swapchain_info.height;
		swapchain.handle = handle;
#if TELEPORT_CLIENT_USE_D3D12
		vector<XrSwapchainImageD3D12KHR> surface_images;
		surface_images.resize(surface_count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
#endif
#if TELEPORT_CLIENT_USE_VULKAN
		vector<XrSwapchainImageVulkanKHR> surface_images;
		surface_images.resize(surface_count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
#endif

#if TELEPORT_CLIENT_USE_D3D11
		vector<XrSwapchainImageD3D11KHR> surface_images;
		surface_images.resize(surface_count, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
#endif

		swapchain.surface_data.resize(surface_count);
		xrEnumerateSwapchainImages(swapchain.handle, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)surface_images.data());
		for (uint32_t i = 0; i < surface_count; i++)
		{
			swapchain.surface_data[i] = CreateSurfaceData(renderPlatform, (XrBaseInStructure&)surface_images[i], swapchain_info);
		}
		xr_swapchains.push_back(swapchain);
	};

	//Main view swapchain:
	// Create a swapchain for this viewpoint! A swapchain is a set of texture buffers used for displaying to screen,
	// typically this is a backbuffer and a front buffer, one for rendering data to, and one for displaying on-screen.
	// A note about swapchain image format here! OpenXR doesn't create a concrete image format for the texture, like 
	// DXGI_FORMAT_R8G8B8A8_UNORM. Instead, it switches to the TYPELESS variant of the provided texture format, like 
	// DXGI_FORMAT_R8G8B8A8_TYPELESS. When creating an ID3D11RenderTargetView for the swapchain texture, we must specify
	// a concrete type like DXGI_FORMAT_R8G8B8A8_UNORM, as attempting to create a TYPELESS view will throw errors, so 
	// we do need to store the format separately and remember it later.
	MAIN_SWAPCHAIN = (int)xr_swapchains.size();
	XrSwapchainCreateInfo swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
	swapchain_info.createFlags	= 0;
	swapchain_info.usageFlags	= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.format		= swapchain_format;
	swapchain_info.sampleCount	= config_view.recommendedSwapchainSampleCount;
	swapchain_info.width		= config_view.recommendedImageRectWidth;
	swapchain_info.height		= config_view.recommendedImageRectHeight;
	swapchain_info.faceCount	= 1;
	swapchain_info.arraySize	= view_count;
	swapchain_info.mipCount		= 1;
	AddXrSwapchain(swapchain_info);

	OVERLAY_SWAPCHAIN			= (int)xr_swapchains.size();
	swapchain_info.createFlags	= 0;
	swapchain_info.usageFlags	= XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.format = swapchain_format;//DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	swapchain_info.sampleCount	= 1;
	swapchain_info.width		= 1024;
	swapchain_info.height		= 512;
	swapchain_info.faceCount	= 1;
	swapchain_info.arraySize	= 1;
	swapchain_info.mipCount		= 1;
	AddXrSwapchain(swapchain_info);

	haveXRDevice = true;
	return true;
}

set<std::string> UseOpenXR::GetRequiredExtensions() const
{
	set<std::string> str=client::OpenXR::GetRequiredExtensions();
#ifdef _MSC_VER
	str.insert("XR_KHR_win32_convert_performance_counter_time");
#endif
	return str;
}