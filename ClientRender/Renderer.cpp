#include "ClientRender/Renderer.h"
#include "ClientRender/VideoDecoderBackend.h"
#include <libavstream/libavstream.hpp>
#if TELEPORT_CLIENT_USE_D3D12
#include "Platform/DirectX12/RenderPlatform.h"
#include <libavstream/surfaces/surface_dx12.hpp>
#endif
#if TELEPORT_CLIENT_USE_D3D11
#include <libavstream/surfaces/surface_dx11.hpp>
#endif
#ifdef _MSC_VER
#include "libavstream/platforms/platform_windows.hpp"
#endif
#include "TeleportClient/ServerTimestamp.h"
#include "TeleportClient/Log.h"
#include <regex>
#include "Tests.h"
#include "TeleportClient/Config.h"
#include "TeleportClient/DiscoveryService.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/Core/StringFunctions.h"
#include "TeleportClient/OpenXR.h"
#include <fmt/core.h>
#if TELEPORT_CLIENT_USE_VULKAN
#include <libavstream/surfaces/surface_vulkan.hpp>
#include "Platform/Vulkan/Texture.h"
#include "Platform/Vulkan/RenderPlatform.h"
#endif
#include "Platform/External/magic_enum/include/magic_enum.hpp"
#include "TeleportClient/ClientTime.h"
#include "TeleportClient/OpenXRRenderModel.h"
#include <functional>
#include "TeleportClient/TabContext.h"

using namespace std::string_literals;

avs::Timestamp clientrender::platformStartTimestamp ;
static bool timestamp_initialized=false;
using namespace clientrender;
using namespace teleport;
using namespace platform;

void msgHandler(avs::LogSeverity severity, const char* msg, void* userData)
{
	if (severity > avs::LogSeverity::Warning)
		std::cerr << msg;
	else
		std::cout << msg ;
}

static const char* ToString(clientrender::Light::Type type)
{
	const char* lightTypeName = "";
	switch (type)
	{
	case clientrender::Light::Type::POINT:
		lightTypeName = "Point";
		break;
	case clientrender::Light::Type::DIRECTIONAL:
		lightTypeName = "  Dir";
		break;
	case clientrender::Light::Type::SPOT:
		lightTypeName = " Spot";
		break;
	case clientrender::Light::Type::AREA:
		lightTypeName = " Area";
		break;
	default:
	case clientrender::Light::Type::DISC:
		lightTypeName = " Disc";
		break;
		break;
	};
	return lightTypeName;
}

static const char *stringof(avs::GeometryPayloadType t)
{
	static const char *txt[]=
	{
		"Invalid", 
		"Mesh",
		"Material",
		"MaterialInstance",
		"Texture",
		"Animation",
		"Node",
		"Skeleton",
		"Bone"
	};
	return txt[(size_t)t];
}

avs::SurfaceBackendInterface* AVSTextureImpl::createSurface() const 
{
	#if TELEPORT_CLIENT_USE_D3D12
			return new avs::SurfaceDX12(texture->AsD3D12Resource());
	#endif
	#if TELEPORT_CLIENT_USE_D3D11
			return new avs::SurfaceDX11(texture->AsD3D11Texture2D());
	#endif
	#if TELEPORT_CLIENT_USE_VULKAN
			vk::Image* img=((vulkan::Texture*)texture)->AsVulkanImage();
			return new avs::SurfaceVulkan(img,texture->width,texture->length,vulkan::RenderPlatform::ToVulkanFormat((texture->pixelFormat)));
	#endif
}
platform::crossplatform::RenderDelegate renderDelegate;
platform::crossplatform::RenderDelegate overlayDelegate;

Renderer::Renderer(teleport::Gui& g)
	:gui(g)
	,config(teleport::client::Config::GetInstance())
{
	if (!timestamp_initialized)
#ifdef _MSC_VER
		platformStartTimestamp = avs::Platform::getTimestamp();
#else
		platformStartTimestamp = avs::Platform::getTimestamp();
#endif
	timestamp_initialized=true;

	clientrender::Tests::RunAllTests();
	renderDelegate = std::bind(&clientrender::Renderer::RenderVRView, this, std::placeholders::_1);
	overlayDelegate = std::bind(&clientrender::Renderer::RenderVROverlay, this, std::placeholders::_1);
}

Renderer::~Renderer()
{
	InvalidateDeviceObjects(); 
}

void Renderer::Init(crossplatform::RenderPlatform* r, teleport::client::OpenXR* u, teleport::PlatformWindow* active_window)
{
	u->SetSessionChangedCallback(std::bind(&Renderer::XrSessionChanged, this, std::placeholders::_1));
	u->SetBindingsChangedCallback(std::bind(&Renderer::XrBindingsChanged, this, std::placeholders::_1, std::placeholders::_2));

	u->SetHandTrackingChangedCallback(std::bind(&Renderer::HandTrackingChanged, this, std::placeholders::_1, std::placeholders::_2));

	// Initialize the audio (asynchronously)
	renderPlatform = r;
	GeometryCache::SetRenderPlatform(r);
	renderState.openXR = u;

	renderPlatform->SetShaderBuildMode(crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);

	renderState.hDRRenderer = new crossplatform::HdrRenderer();

	renderState.hdrFramebuffer = renderPlatform->CreateFramebuffer();
	renderState.hdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
	renderState.hdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
	renderState.hdrFramebuffer->SetAntialiasing(1);
	camera.SetPositionAsXYZ(0.f, 0.f, 2.f);
	vec3 look(0.f, 1.f, 0.f), up(0.f, 0.f, 1.f);
	camera.LookInDirection(look, up);

	camera.SetHorizontalFieldOfViewDegrees(HFOV);

	// Automatic vertical fov - depends on window shape:
	camera.SetVerticalFieldOfViewDegrees(0.f);

	crossplatform::CameraViewStruct vs;
	vs.exposure = 1.f;
	vs.farZ = 3000.f;
	vs.nearZ = 0.01f;
	vs.gamma = 1.0f;
	vs.InfiniteFarPlane = true;
	vs.projection = crossplatform::DEPTH_REVERSE;

	camera.SetCameraViewStruct(vs);

	memset(keydown, 0, sizeof(keydown));
	text3DRenderer.PushFontPath("assets/fonts");
	text3DRenderer.RestoreDeviceObjects(renderPlatform);
	renderState.hDRRenderer->RestoreDeviceObjects(renderPlatform);
	renderState.hdrFramebuffer->RestoreDeviceObjects(renderPlatform);

	gui.RestoreDeviceObjects(renderPlatform, active_window);
	auto connectButtonHandler = std::bind(&client::TabContext::ConnectButtonHandler, std::placeholders::_1, std::placeholders::_2);
	gui.SetConnectHandler(connectButtonHandler);
	auto cancelConnectHandler = std::bind(&client::TabContext::CancelConnectButtonHandler, std::placeholders::_1);
	gui.SetCancelConnectHandler(cancelConnectHandler);
	gui.SetConsoleCommandHandler(std::bind(&Renderer::ConsoleCommand,this,std::placeholders::_1));
	auto startSessionHandler = [this](){
		start_xr_session=true;
		end_xr_session=false;
	};
	gui.SetStartXRSessionHandler(startSessionHandler);
	auto endSessionHandler = [this](){
		start_xr_session=false;
		end_xr_session=true;
	};
	gui.SetEndXRSessionHandler(endSessionHandler);
	errno = 0;
	LoadShaders();

	renderState.pbrConstants.RestoreDeviceObjects(renderPlatform);
	renderState.pbrConstants.LinkToEffect(renderState.pbrEffect, "pbrConstants");
	renderState.perNodeConstants.RestoreDeviceObjects(renderPlatform);
	renderState.perNodeConstants.LinkToEffect(renderState.pbrEffect, "perNodeConstants");
	renderState.cubemapConstants.RestoreDeviceObjects(renderPlatform);
	renderState.cubemapConstants.LinkToEffect(renderState.cubemapClearEffect, "CubemapConstants");
	renderState.cameraConstants.RestoreDeviceObjects(renderPlatform);
	renderState.stereoCameraConstants.RestoreDeviceObjects(renderPlatform);
	renderState.tagDataIDBuffer.RestoreDeviceObjects(renderPlatform, 1, true);
	renderState.tagDataCubeBuffer.RestoreDeviceObjects(renderPlatform, RenderState::maxTagDataSize, false, true);
	renderState.lightsBuffer.RestoreDeviceObjects(renderPlatform, 10, false, true);
	renderState.boneMatrices.RestoreDeviceObjects(renderPlatform);
	renderState.boneMatrices.LinkToEffect(renderState.pbrEffect, "boneMatrices");

	avs::Context::instance()->setMessageHandler(msgHandler, nullptr);

	geometryDecoder.setCacheFolder(config.GetStorageFolder());
	auto localInstanceRenderer = GetInstanceRenderer(0);
	auto& localGeometryCache = localInstanceRenderer->geometryCache;
	localGeometryCache->setCacheFolder("assets/localGeometryCache");

	InitLocalGeometry();
}

void Renderer::ConsoleCommand(const std::string &str)
{
	console.push(str);
}

void Renderer::ExecConsoleCommands()
{
	while(!console.empty())
	{
		std::string str = console.front();
		console.pop();
		ExecConsoleCommand(str);
	}
}

void Renderer::ExecConsoleCommand(const std::string &str)
{
	if (str == "killstreaming")
	{
		std::set<avs::uid> ids=client::SessionClient::GetSessionClientIds();
		for(auto i:ids)
		{
			auto sessionClient=client::SessionClient::GetSessionClient(i);
			sessionClient->KillStreaming();
		}
	}
	if (str == "reloadshaders")
	{
		RecompileShaders();
	}
	else if (str == "reloadgeometry")
	{
		//geometryDecoder.clearCache();
		InitLocalGeometry();
	}
	else if (str == "reloadtextures")
	{
		renderState.pbrConstants.RestoreDeviceObjects(renderPlatform);
		renderState.pbrConstants.LinkToEffect(renderState.pbrEffect, "pbrConstants");
		renderState.perNodeConstants.RestoreDeviceObjects(renderPlatform);
		renderState.perNodeConstants.LinkToEffect(renderState.pbrEffect, "perNodeConstants");
		renderState.cubemapConstants.RestoreDeviceObjects(renderPlatform);
		renderState.cubemapConstants.LinkToEffect(renderState.cubemapClearEffect, "CubemapConstants");
		renderState.cameraConstants.RestoreDeviceObjects(renderPlatform);
		renderState.stereoCameraConstants.RestoreDeviceObjects(renderPlatform);
		renderState.tagDataIDBuffer.RestoreDeviceObjects(renderPlatform, 1, true);
		renderState.tagDataCubeBuffer.RestoreDeviceObjects(renderPlatform, RenderState::maxTagDataSize, false, true);
		renderState.lightsBuffer.RestoreDeviceObjects(renderPlatform, 10, false, true);
		renderState.boneMatrices.RestoreDeviceObjects(renderPlatform);
		renderState.boneMatrices.LinkToEffect(renderState.pbrEffect, "boneMatrices");
	}
	else if (str == "reloadfonts")
	{
		text3DRenderer.RestoreDeviceObjects(renderPlatform);
	}
	else if (str == "reloadgui")
	{
		gui.RestoreDeviceObjects(renderPlatform, nullptr);
	}
	else if (str == "reloadall")
	{
		RecompileShaders();
		//geometryDecoder.clearCache();
		InitLocalGeometry();
		text3DRenderer.RestoreDeviceObjects(renderPlatform);
		gui.RestoreDeviceObjects(renderPlatform, nullptr);
	}
}

void Renderer::InitLocalGeometry()
{
	auto localInstanceRenderer=GetInstanceRenderer(0);
	auto &localResourceCreator=localInstanceRenderer->resourceCreator;
	// initialize the default local geometry:
	avs::uid hand_mesh_uid = avs::GenerateUid();
	lobbyGeometry.hand_skeleton_uid = avs::GenerateUid();
	avs::uid point_anim_uid = avs::GenerateUid();
	//geometryDecoder.decodeFromFile("assets/localGeometryCache/meshes/2CylinderEngine.gltf",avs::GeometryPayloadType::Mesh,&localResourceCreator,gltf_uid);
	geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/meshes/Hand.mesh_compressed",avs::GeometryPayloadType::Mesh,&localResourceCreator,hand_mesh_uid);
	geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/skeletons/Hand.skeleton",avs::GeometryPayloadType::Skeleton,&localResourceCreator, lobbyGeometry.hand_skeleton_uid);
	geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/animations/Point.anim",avs::GeometryPayloadType::Animation,&localResourceCreator, point_anim_uid);
	geometryDecoder.WaitFromDecodeThread();
	
	// Generate local uid's for the nodes and resources.
	lobbyGeometry.left_hand_node_uid	= avs::GenerateUid();
	lobbyGeometry.right_hand_node_uid	= avs::GenerateUid();
	avs::uid grey_material_uid			= avs::GenerateUid();
	avs::uid blue_material_uid			= avs::GenerateUid();
	avs::uid red_material_uid			= avs::GenerateUid();
	auto &localGeometryCache=localInstanceRenderer->geometryCache;
	{
		avs::Material avsMaterial;
		avsMaterial.doubleSided=true;
		avsMaterial.name="local grey";
		avsMaterial.pbrMetallicRoughness.metallicFactor=0.0f;
		avsMaterial.pbrMetallicRoughness.baseColorFactor={.5f,.5f,.5f,.5f};
		localResourceCreator.CreateMaterial(0,grey_material_uid,avsMaterial);// not used just now.
		avsMaterial.name="local blue glow";
		avsMaterial.emissiveFactor={0.0f,0.2f,0.5f};
		localResourceCreator.CreateMaterial(0,blue_material_uid,avsMaterial);
		avsMaterial.name="local red glow";
		avsMaterial.emissiveFactor={0.4f,0.1f,0.1f};
		localResourceCreator.CreateMaterial(0,red_material_uid,avsMaterial);

		localGeometryCache->mMaterialManager.Get(grey_material_uid)->SetShaderOverride("ps_local_hand");
		localGeometryCache->mMaterialManager.Get(blue_material_uid)->SetShaderOverride("ps_local_hand");
		localGeometryCache->mMaterialManager.Get(red_material_uid)->SetShaderOverride("ps_local_hand");
	}
	avs::Node avsNode;
	lobbyGeometry.self_node_uid=avs::GenerateUid();
	localResourceCreator.CreateNode(0,lobbyGeometry.self_node_uid,avsNode);
	avsNode.parentID					=lobbyGeometry.self_node_uid;
	avsNode.name		="local Left Root";
	localResourceCreator.CreateNode(0,lobbyGeometry.left_hand_node_uid,avsNode);
	avsNode.name		="local Right Root";
	localResourceCreator.CreateNode(0,lobbyGeometry.right_hand_node_uid,avsNode);
	
	{
		avs::Node leftSkeletonNode;
		lobbyGeometry.left_hand_skeleton_node_uid=avs::GenerateUid();
		leftSkeletonNode.data_type	=avs::NodeDataType::Skeleton;
		leftSkeletonNode.data_uid	=lobbyGeometry.hand_skeleton_uid;
		leftSkeletonNode.parentID					=lobbyGeometry.left_hand_node_uid;
		localResourceCreator.CreateNode(0,lobbyGeometry.left_hand_skeleton_node_uid,leftSkeletonNode);
	}
	{
		avs::Node rightSkeletonNode;
		lobbyGeometry.right_hand_skeleton_node_uid=avs::GenerateUid();
		rightSkeletonNode.data_type	=avs::NodeDataType::Skeleton;
		rightSkeletonNode.data_uid	=lobbyGeometry.hand_skeleton_uid;
		rightSkeletonNode.parentID					=lobbyGeometry.right_hand_node_uid;
		localResourceCreator.CreateNode(0,lobbyGeometry.right_hand_skeleton_node_uid,rightSkeletonNode);
	}
	avsNode.data_type	=avs::NodeDataType::Mesh;
	
	avsNode.data_uid	=0;//hand_mesh_uid;
	avsNode.materials.push_back(blue_material_uid);
	avsNode.materials.push_back(grey_material_uid);
	
	avsNode.name						="local Left Hand";
	avsNode.skeletonNodeID				=lobbyGeometry.left_hand_skeleton_node_uid;
	avsNode.animations.push_back(point_anim_uid);
	avsNode.materials[0]				=blue_material_uid;
	avsNode.parentID					=lobbyGeometry.left_hand_node_uid;
	avsNode.skeletonNodeID				=lobbyGeometry.left_hand_skeleton_node_uid;
	avsNode.localTransform.rotation		={0.707f,0,0,0.707f};
	avsNode.localTransform.scale		={-1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position		={0.f,0.1f,0.f};
	lobbyGeometry.local_left_hand_uid	=avs::GenerateUid();
	localResourceCreator.CreateNode(0,lobbyGeometry.local_left_hand_uid,avsNode);

	avsNode.name="local Right Hand";
	avsNode.materials[0]				=red_material_uid;
	avsNode.parentID					=lobbyGeometry.right_hand_node_uid;
	avsNode.skeletonNodeID				=lobbyGeometry.right_hand_skeleton_node_uid;
	avsNode.localTransform.scale		={1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position		={0.f,0.1f,0.f};
	
	lobbyGeometry.local_right_hand_uid	=avs::GenerateUid();
	localResourceCreator.CreateNode(0,lobbyGeometry.local_right_hand_uid,avsNode);
	
	lobbyGeometry.left_controller_node_uid	=avs::GenerateUid();
	lobbyGeometry.right_controller_node_uid	=avs::GenerateUid();
	if(renderState.openXR)
	{
		renderState.openXR->SetFallbackBinding(client::LEFT_AIM_POSE		,"left/input/aim/pose");
		renderState.openXR->SetFallbackBinding(client::RIGHT_AIM_POSE		,"right/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_hand_node_uid	,"left/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_hand_node_uid	,"right/input/aim/pose");
		
		renderState.openXR->SetFallbackBinding(client::LEFT_GRIP_POSE		,"left/input/grip/pose");
		renderState.openXR->SetFallbackBinding(client::RIGHT_GRIP_POSE		,"right/input/grip/pose");

		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_controller_node_uid	,"left/input/grip/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_controller_node_uid	,"right/input/grip/pose");
		
		renderState.openXR->SetFallbackBinding(client::MOUSE_LEFT_BUTTON	,"mouse/left/click");
		renderState.openXR->SetFallbackBinding(client::MOUSE_RIGHT_BUTTON	,"mouse/right/click");

		// Hard-code the menu button
		renderState.openXR->SetHardInputMapping(local_server_uid,local_menu_input_id	,avs::InputType::IntegerEvent,teleport::client::ActionId::SHOW_MENU);
		renderState.openXR->SetHardInputMapping(local_server_uid,local_cycle_osd_id		,avs::InputType::IntegerEvent,teleport::client::ActionId::X);
		renderState.openXR->SetHardInputMapping(local_server_uid,local_cycle_shader_id	,avs::InputType::IntegerEvent,teleport::client::ActionId::Y);
	}
	auto rightHand=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
	lobbyGeometry.palm_to_hand_r=rightHand->GetLocalTransform();
	auto leftHand=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.local_left_hand_uid);
	lobbyGeometry.palm_to_hand_l=leftHand->GetLocalTransform();
	leftHand->SetVisible(false);

	// local lighting.
	avs::uid diffuse_cubemap_uid=avs::GenerateUid();
	avs::uid specular_cubemap_uid=avs::GenerateUid();
	if(1)
	{
		geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/textures/diffuseRenderTexture.texture"
			,avs::GeometryPayloadType::Texture,&localResourceCreator, diffuse_cubemap_uid);
		geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/textures/specularRenderTexture.texture"
			,avs::GeometryPayloadType::Texture,&localResourceCreator, specular_cubemap_uid);

	}
	else
	{
		std::vector<unsigned int> diffuse_cubemap_data={0xFF808080,0xFF808080,0xFF808080,0xFF808080,0xFFFFFFFF,0};
		avs::Texture avsTexture={
			"diffuse_cubemap"
			,1,1,1
			,4,6,1
			,avs::TextureFormat::RGBA8,avs::TextureCompression::UNCOMPRESSED
			,false,0,1.0f,true 
			, (uint32_t)(diffuse_cubemap_data.size()*sizeof(unsigned))
			, (uint8_t*)diffuse_cubemap_data.data()
			, false};
		localResourceCreator.CreateTexture(0,diffuse_cubemap_uid,avsTexture);

		std::vector<unsigned int> specular_cubemap_data={0xFFBBBBBB,0xFFBBBBBB,0xFFBBBBBB,0xFFBBBBBB,0xFF808080,0};
		avs::Texture avsCTexture={
			"specular_cubemap"
			,1,1,1
			,4,6,1
			,avs::TextureFormat::RGBA8,avs::TextureCompression::UNCOMPRESSED
			,false,0,1.0f,true 
			, (uint32_t)(specular_cubemap_data.size()*sizeof(unsigned))
			, (uint8_t*)specular_cubemap_data.data()
			, false};
		localResourceCreator.CreateTexture(0,specular_cubemap_uid,avsCTexture);
	}
	// test gltf loading.
/*	avs::uid gltf_uid = avs::GenerateUid();
	// gltf_uid will refer to a SubScene asset in cache zero.
	geometryDecoder.decodeFromFile(0,"assets/localGeometryCache/meshes/generic_controller.glb",avs::GeometryPayloadType::Mesh,&localResourceCreator,gltf_uid,platform::crossplatform::AxesStandard::OpenGL);
	{
		avs::Node gltfNode;
		gltfNode.name		="generic_controller.glb";
		gltfNode.data_type	=avs::NodeDataType::SubScene;
		gltfNode.data_uid	=gltf_uid;
	//	gltfNode.materials.resize(3,0);
		static float sc=1.01f;
		gltfNode.localTransform.position=vec3(1.0f,1.0f,1.0f);
		gltfNode.localTransform.scale=vec3(sc,sc,sc);
		localResourceCreator.CreateNode(0,avs::GenerateUid(),gltfNode);
	}*/
	auto local_session_client=client::SessionClient::GetSessionClient(0);
	auto setupCommand=local_session_client->GetSetupCommand();
	setupCommand.clientDynamicLighting.diffuseCubemapTexture=diffuse_cubemap_uid;
	setupCommand.clientDynamicLighting.specularCubemapTexture=specular_cubemap_uid;
	setupCommand.draw_distance=10.0f;
	localGeometryCache->SetLifetimeFactor(0.f);
	local_session_client->ApplySetup(setupCommand);
	XrSessionChanged(true);
	xr_profile_to_controller_model_name["/interaction_profiles/khr/simple_controller"]			= "generic-trigger-squeeze-touchpad-thumbstick/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/google/daydream_controller"]		= "google-daydream/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/htc/vive_controller"]			= "htc-vive-focus/none";
	xr_profile_to_controller_model_name["/interaction_profiles/htc/vive_pro"]					= "htc-vive-focus-pro/none";
	xr_profile_to_controller_model_name["/interaction_profiles/microsoft/motion_controller"]	= "microsoft-mixed-reality/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/microsoft/xbox_controller"]		= "";
	xr_profile_to_controller_model_name["/interaction_profiles/oculus/go_controller"]			= "oculus-go/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/oculus/touch_controller"]		= "oculus-touch-v3/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/valve/index_controller"] = "valve-index/{SIDE}";
	xr_profile_to_controller_model_name["/interaction_profiles/microsoft/hand_controller"] = "valve-index/{SIDE}";
	//XrBindingsChanged("/user/hand/left", "/interaction_profiles/khr/simple_controller");
	//XrBindingsChanged("/user/hand/right", "/interaction_profiles/oculus/touch_controller");
	
}

void Renderer::HandTrackingChanged(int left_right,bool on_off)
{
	std::shared_ptr<Node> controller_node = GetInstanceRenderer(0)->geometryCache->mNodeManager->GetNode(left_right == 0 ? lobbyGeometry.left_controller_node_uid : lobbyGeometry.right_controller_node_uid);
	if(on_off)
	{
		TELEPORT_CERR << (left_right == 0 ? "Left" : "Right") << " Hand Tracking enabled.\n";
		if (controller_node)
			controller_node->SetVisible(false);
	}
	else
	{
		TELEPORT_CERR << (left_right == 0 ? "Left" : "Right") << " Hand Tracking disabled.\n";
		if (controller_node)
			controller_node->SetVisible(true);
	}
}

void Renderer::XrBindingsChanged(std::string user_path,std::string profile)
{
	std::string systemName=renderState.openXR->GetSystemName();
	auto localInstanceRenderer = GetInstanceRenderer(0);
	auto &localResourceCreator = localInstanceRenderer->resourceCreator;
	std::string source_root="https://simul.co:443/wp-content/uploads/teleport/content";
	auto u = xr_profile_to_controller_model_name.find(profile);
	if(u!=xr_profile_to_controller_model_name.end())
	{
		std::string model_name = u->second;

		if(user_path=="/user/hand/left")
		{
			std::string left_model_name = u->second;
			platform::core::find_and_replace(left_model_name, "{SIDE}", "left");
			avs::Node avsNode;
			avsNode.parentID = lobbyGeometry.self_node_uid;
			if (!lobbyGeometry.left_model_uid)
				lobbyGeometry.left_model_uid = avs::GenerateUid();
			geometryDecoder.decodeFromWeb(0, source_root + "/"s + left_model_name + ".glb", avs::GeometryPayloadType::Mesh, &localResourceCreator, lobbyGeometry.left_model_uid, platform::crossplatform::AxesStandard::OpenGL);
			avsNode.name = "Left Controller";
			avsNode.data_type = avs::NodeDataType::SubScene;
			avsNode.data_uid = lobbyGeometry.left_model_uid;
			localResourceCreator.CreateNode(0, lobbyGeometry.left_controller_node_uid, avsNode);
		}
		if (user_path == "/user/hand/right")
		{
			std::string right_model_name = u->second;
			platform::core::find_and_replace(right_model_name, "{SIDE}", "right");
			avs::Node avsNode;
			avsNode.parentID = lobbyGeometry.self_node_uid;
			if (!lobbyGeometry.right_model_uid)
				lobbyGeometry.right_model_uid = avs::GenerateUid();
			geometryDecoder.decodeFromWeb(0, source_root + "/"s + right_model_name + ".glb", avs::GeometryPayloadType::Mesh, &localResourceCreator, lobbyGeometry.right_model_uid, platform::crossplatform::AxesStandard::OpenGL);
			avsNode.name = "Right Controller";
			avsNode.data_type = avs::NodeDataType::SubScene;
			avsNode.data_uid = lobbyGeometry.right_model_uid;
			localResourceCreator.CreateNode(0, lobbyGeometry.right_controller_node_uid, avsNode);
		}
	}
}

void Renderer::XrSessionChanged(bool active)
{
// invalidate shaders - could have switched to dual output mode.
	renderState.shaderValidity++;
}

void Renderer::UpdateShaderPasses()
{
	auto solid=renderState.pbrEffect->GetTechniqueByName("solid");
	renderState.solidVariantPass					=solid?solid->GetVariantPass("pbr"):nullptr;
	auto transparent=renderState.pbrEffect->GetTechniqueByName("transparent");
	renderState.transparentVariantPass				=transparent?transparent->GetVariantPass("pbr"):nullptr;
}

// This allows live-recompile of shaders. 
void Renderer::RecompileShaders()
{
	renderPlatform->RecompileShaders();
	text3DRenderer.RecompileShaders();
	renderState.hDRRenderer->RecompileShaders();
	gui.RecompileShaders();
	TextCanvas::RecompileShaders();
	renderPlatform->ScheduleRecompileEffects({"pbr","cubemap_clear"},[this](){reload_shaders=true;});
}

void Renderer::LoadShaders()
{
	reload_shaders=false;
	renderState.shaderValidity++;
	delete renderState.pbrEffect;
	delete renderState.cubemapClearEffect;
	renderState.pbrEffect			= renderPlatform->CreateEffect("pbr");
	renderState.cubemapClearEffect	= renderPlatform->CreateEffect("cubemap_clear");

	UpdateShaderPasses();

	renderState._RWTagDataIDBuffer						=renderState.cubemapClearEffect->GetShaderResource("RWTagDataIDBuffer");
	renderState.cubemapClearEffect_TagDataCubeBuffer	=renderState.cubemapClearEffect->GetShaderResource("TagDataCubeBuffer");
	renderState._lights									=renderState.pbrEffect->GetShaderResource("lights");
	renderState.plainTexture							=renderState.cubemapClearEffect->GetShaderResource("plainTexture");
	renderState.RWTextureTargetArray					=renderState.cubemapClearEffect->GetShaderResource("RWTextureTargetArray");
	renderState.cubemapClearEffect_TagDataIDBuffer		=renderState.cubemapClearEffect->GetShaderResource("TagDataIDBuffer");
	renderState.pbrEffect_TagDataIDBuffer				=renderState.pbrEffect->GetShaderResource("TagDataIDBuffer");
	
	renderState.pbrEffect_diffuseCubemap				=renderState.pbrEffect->GetShaderResource("diffuseCubemap");
	renderState.pbrEffect_specularCubemap				=renderState.pbrEffect->GetShaderResource("specularCubemap");
	renderState.pbrEffect_diffuseTexture				=renderState.pbrEffect->GetShaderResource("diffuseTexture");
	renderState.pbrEffect_normalTexture					=renderState.pbrEffect->GetShaderResource("normalTexture");
	renderState.pbrEffect_combinedTexture				=renderState.pbrEffect->GetShaderResource("combinedTexture");
	renderState.pbrEffect_emissiveTexture				=renderState.pbrEffect->GetShaderResource("emissiveTexture");
	renderState.pbrEffect_globalIlluminationTexture		=renderState.pbrEffect->GetShaderResource("globalIlluminationTexture");
}

std::shared_ptr<InstanceRenderer> Renderer::GetInstanceRenderer(avs::uid server_uid)
{
	auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
	auto i=instanceRenderers.find(server_uid);
	if(i==instanceRenderers.end())
	{
		auto r=std::make_shared<InstanceRenderer>(server_uid,config,geometryDecoder,renderState,sessionClient.get());
		instanceRenderers[server_uid]=r;
		r->RestoreDeviceObjects(renderPlatform);
		return r;
	}
	return i->second;
}

void Renderer::RemoveInstanceRenderer(avs::uid server_uid)
{
	auto i=instanceRenderers.find(server_uid);
	if(i!=instanceRenderers.end())
	{
		i->second->InvalidateDeviceObjects();
		instanceRenderers.erase(i);
	}
}

void Renderer::InvalidateDeviceObjects()
{
	for(auto i:instanceRenderers)
		i.second->InvalidateDeviceObjects();

	GeometryCache::DestroyAllCaches();
	text3DRenderer.InvalidateDeviceObjects();
	gui.InvalidateDeviceObjects();
	if(renderState.pbrEffect)
	{
		renderState.pbrEffect->InvalidateDeviceObjects();
		delete renderState.pbrEffect;
		renderState.pbrEffect=nullptr;
	}
	if(renderState.hDRRenderer)
		renderState.hDRRenderer->InvalidateDeviceObjects();
	if(renderPlatform)
		renderPlatform->InvalidateDeviceObjects();
	if(renderState.hdrFramebuffer)
		renderState.hdrFramebuffer->InvalidateDeviceObjects();
	SAFE_DELETE(renderState.hDRRenderer);
	SAFE_DELETE(renderState.hdrFramebuffer);
	SAFE_DELETE(renderState.pbrEffect);
	SAFE_DELETE(renderState.cubemapClearEffect);
}

void Renderer::FillInControllerPose(int index, float offset)
{
	if(!renderState.hdrFramebuffer->GetHeight())
		return;
	float x= mouseCameraInput.MouseX / (float)renderState.hdrFramebuffer->GetWidth();
	float y= mouseCameraInput.MouseY / (float)renderState.hdrFramebuffer->GetHeight();
	vec3 controller_dir	=camera.ScreenPositionToDirection(x, y, renderState.hdrFramebuffer->GetWidth() / static_cast<float>(renderState.hdrFramebuffer->GetHeight()));
	vec3 view_dir			=camera.ScreenPositionToDirection(0.5f,0.5f,1.0f);
	// we seek the angle positive on the Z-axis representing the view direction azimuth:
	static float cc=0.0f;
	cc+=0.01f;
	float angle=atan2f(-view_dir.x, view_dir.y);
	float sine= sin(angle), cosine=cos(angle);
	float sine_elev= view_dir.z;
	static float hand_dist=0.7f;
	// Position the hand based on mouse pos.
	static float xmotion_scale = 1.0f;
	static float ymotion_scale = .5f;
	static float ymotion_offset = .6f;
	static float z_offset = -0.1f;
	vec2 pos; 
	pos.x = 0.4f*offset+(x - 0.5f) * xmotion_scale;
	pos.y = ymotion_offset + (0.5f-y)*ymotion_scale;

	vec3 pos_offset=vec3(hand_dist*(-pos.y*sine+ pos.x*cosine),hand_dist*(pos.y*cosine+pos.x*sine),z_offset+hand_dist*sine_elev*pos.y);

	// Get horizontal azimuth of view.
	vec3 camera_local_pos	=camera.GetPosition();
	vec3 footspace_pos		=camera_local_pos;
	footspace_pos			+=pos_offset;

	// For the orientation, we want to point the controller towards controller_dir. The pointing direction is y.
	// The up direction is x, and the left direction is z.
	const avs::Pose &headPose=renderState.openXR->GetHeadPose_StageSpace();
	crossplatform::Quaternionf q = (const float*)(&headPose.orientation);
	float azimuth	= angle;
	static float elev_mult=1.2f;
	static float elev_offset = -0.55f;
	float elevation	= elev_mult*(y+elev_offset);
	q.Reset();
	q.Rotate(elevation	,vec3(-1.0f, 0, 0));
	q.Rotate(azimuth	,vec3(0,0,1.0f));
	vec3 point_dir=q*vec3(0, 1.0f, 0);
	static float roll=-0.3f;
	q.Rotate(roll*offset, point_dir);

	avs::Pose pose;
	pose.position=*((vec3*)&footspace_pos);
	pose.orientation=*((const vec4*)&q);

	renderState.openXR->SetFallbackPoseState(index?client::RIGHT_GRIP_POSE:client::LEFT_GRIP_POSE,pose);
	//pose.position.z-=0.1f;
	renderState.openXR->SetFallbackPoseState(index?client::RIGHT_AIM_POSE:client::LEFT_AIM_POSE,pose);
}

void SetRenderPose(crossplatform::GraphicsDeviceContext& deviceContext, const avs::Pose& originPose)
{
// Here we must transform the viewstructs in the device context by the specified origin pose,
// so that the new view matrices will be in a global space which has the stage space at the specified origin.
// This assumes that the initial viewStructs are in stage space.

	math::Matrix4x4 originMat=client::OpenXR::CreateTransformMatrixFromPose(originPose);
	math::Matrix4x4 tmp=deviceContext.viewStruct.view;
	Multiply4x4(deviceContext.viewStruct.view,originMat,tmp);
	// MUST call init each frame, or whenever the matrices change.
	deviceContext.viewStruct.Init();

	crossplatform::MultiviewGraphicsDeviceContext* mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
	if (mvgdc)
	{
		for (int i=0;i<mvgdc->viewStructs.size();i++)
		{
			auto &viewStruct=mvgdc->viewStructs[i];
			tmp=viewStruct.view;
			Multiply4x4(viewStruct.view,originMat,tmp);
			// MUST call init each frame.
			viewStruct.Init();
		}
	}	
}

avs::Pose Renderer::GetOriginPose(avs::uid server_uid)
{
	avs::Pose origin_pose;
	auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
	auto &clientServerState=sessionClient->GetClientServerState();
	std::shared_ptr<Node> origin_node=GetInstanceRenderer(server_uid)->geometryCache->mNodeManager->GetNode(clientServerState.origin_node_uid);
	if(origin_node)
	{
		origin_pose.position=origin_node->GetGlobalPosition();
		origin_pose.orientation=*((vec4*)&origin_node->GetGlobalRotation());
	}

	return origin_pose;
}

void Renderer::RenderVRView(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if(reload_shaders)
		LoadShaders();
	RenderView(deviceContext);
	DrawGUI(deviceContext, true);
}

void Renderer::RenderView(crossplatform::GraphicsDeviceContext& deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "RenderView");
	bool multiview = false;
	crossplatform::MultiviewGraphicsDeviceContext* mvgdc=nullptr;
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
		multiview = true;
	}
	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);
	renderState.pbrEffect->UnbindTextures(deviceContext);
	static std::vector<crossplatform::ViewStruct> defaultViewStructs;
	if(mvgdc)
		defaultViewStructs=mvgdc->viewStructs;
	else
	{
		defaultViewStructs.resize(1);
		defaultViewStructs[0]=deviceContext.viewStruct;
	}

	const std::set<int32_t> &tab_ids=client::TabContext::GetTabIndices();
	// Each tab context has one active server at most.
	static std::set<avs::uid> server_uids;
	server_uids.clear();
	for(const auto t:tab_ids)
	{
		auto tabContext=client::TabContext::GetTabContext(t);
		if(!tabContext)
			continue;
		auto server_uid = tabContext->GetServerUid();
		if(server_uid!=0)
		{
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			if(sessionClient->IsConnected())
				server_uids.insert(server_uid);
		}
	}
	if(!server_uids.size())
	{
		if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
		{
			crossplatform::MultiviewGraphicsDeviceContext &mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
			renderState.stereoCameraConstants.leftInvWorldViewProj = mgdc.viewStructs[0].invViewProj;
			renderState.stereoCameraConstants.rightInvWorldViewProj = mgdc.viewStructs[1].invViewProj;
			renderState.stereoCameraConstants.stereoViewPosition = mgdc.viewStruct.cam_pos;
			renderState.cubemapClearEffect->SetConstantBuffer(mgdc, &renderState.stereoCameraConstants);
		}
		renderState.cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
		renderState.cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
		renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cameraConstants);

		std::string passName = (int)config.options.lobbyView ? "neon" : "white";
		if (deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr)
			passName += "_multiview";
		if (!renderState.openXR->IsPassthroughActive())
		{
			renderState.cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());
			renderPlatform->DrawQuad(deviceContext);
			renderState.cubemapClearEffect->Unapply(deviceContext);
		}
		server_uids.insert(0);
	}
	for (const auto &server_uid : server_uids)
	{
		auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
		// Init the viewstruct in global space - i.e. with the server offsets.
		avs::Pose origin_pose;
		auto &clientServerState = sessionClient->GetClientServerState();
		std::shared_ptr<Node> origin_node=GetInstanceRenderer(server_uid)->geometryCache->mNodeManager->GetNode(clientServerState.origin_node_uid);
		if(origin_node)
		{
			origin_pose.position=origin_node->GetGlobalPosition();
			origin_pose.orientation=*((vec4*)&origin_node->GetGlobalRotation());
			SetRenderPose(deviceContext,GetOriginPose(server_uid));
			GetInstanceRenderer(server_uid)->RenderView(deviceContext);
			if(debugOptions.showAxes)
			{
				renderPlatform->DrawAxes(deviceContext,mat4::identity(),2.0f);
			}
		}
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);

	// Init the viewstruct in local space - i.e. with no server offsets.
	SetRenderPose(deviceContext, avs::Pose());
	if(mvgdc)
	{
		mvgdc->viewStructs=defaultViewStructs;
		for(auto s:mvgdc->viewStructs)
		{
			s.Init();
		}
	}
	deviceContext.viewStruct=defaultViewStructs[0];
	deviceContext.viewStruct.Init();

	const std::map<avs::uid,teleport::client::NodePoseState> &nodePoseStates
		=renderState.openXR->GetNodePoseStates(0,renderPlatform->GetFrameNumber());
	auto l=nodePoseStates.find(lobbyGeometry.left_hand_node_uid);
	vec4 white={1.f,1.f,1.f,1.f};
	std::vector<vec4> hand_pos_press;
	if(l!=nodePoseStates.end())
	{
		avs::Pose handPose	= l->second.pose_footSpace.pose;
		vec3 pos		= LocalToGlobal(handPose,*((vec3*)&lobbyGeometry.index_finger_offset));
		vec4 pos4;
		pos4.xyz			= (const float*)&pos;
		pos4.w				= 0.0f;
		hand_pos_press.push_back(pos4);
	}

	auto r=nodePoseStates.find(lobbyGeometry.right_hand_node_uid);
	if(r!=nodePoseStates.end())
	{
		avs::Pose rightHand = r->second.pose_footSpace.pose;
		vec3 pos = LocalToGlobal(rightHand,*((vec3*)&lobbyGeometry.index_finger_offset));
		vec4 pos4;
		pos4.xyz = (const float*)&pos;
		pos4.w = 0.0f;
		hand_pos_press.push_back(pos4);
	}
	static bool override_show_local_geometry = false;
	if (have_vr_device || config.options.simulateVR)
	{
		gui.Update(hand_pos_press, have_vr_device || config.options.simulateVR);
		gui.Render3DConnectionGUI(deviceContext);
	}
	bool show_local_geometry = (gui.GetGuiType()!=GuiType::None&&(have_vr_device || config.options.simulateVR))|| override_show_local_geometry;
	renderState.selected_uid=gui.GetSelectedUid();
	if (show_local_geometry)
	{	
		renderState.pbrConstants.drawDistance = 1000.0f;
		GetInstanceRenderer(0)->RenderLocalNodes(deviceContext, 0);
	}
	
	renderState.pbrEffect->UnbindTextures(deviceContext);
}

void Renderer::ChangePass(ShaderMode newShaderMode)
{
	shaderMode=newShaderMode;
	switch(newShaderMode)
	{
		case ShaderMode::PBR:
			renderState.overridePixelShader = "";
			break;
		case ShaderMode::ALBEDO:
			renderState.overridePixelShader = "ps_solid_albedo_only";
			break;
		case ShaderMode::NORMALS:
			renderState.overridePixelShader = "ps_debug_normals";
			break;
		case ShaderMode::DEBUG_ANIM:
			renderState.overridePixelShader = "ps_debug_anim";
			break;
		case ShaderMode::LIGHTMAPS:
			renderState.overridePixelShader = "ps_debug_lightmaps";
			break;
		case ShaderMode::NORMAL_VERTEXNORMALS:
			renderState.overridePixelShader = "ps_debug_normal_vertexnormals";
			break;
		case ShaderMode::UVS:
			renderState.overridePixelShader = "ps_debug_uvs";
			break;
		case ShaderMode::REZZING:
			renderState.overridePixelShader = "ps_digitizing";
			break;
		default:
			renderState.overridePixelShader = "";
			break;
	}
	renderState.shaderValidity++;
	UpdateShaderPasses();
}
void Renderer::Update(double timestamp_ms)
{
	double timeElapsed_s = (timestamp_ms - previousTimestamp) / 1000.0f;//ms to seconds
	if(timeElapsed_s>0.0)
		framerate=1.0f/(float)timeElapsed_s;
	teleport::client::ServerTimestamp::tick(timeElapsed_s);
	for(auto i:instanceRenderers)
	{
		i.second->geometryCache->Update( static_cast<float>(timeElapsed_s));
		i.second->resourceCreator.Update(static_cast<float>(timeElapsed_s));

		if(i.first!=0)
		{
			const auto &removedNodeUids=i.second->geometryCache->mNodeManager->GetRemovedNodeUids();
			// Node has been deleted!
			for(const auto u:removedNodeUids)
				renderState.openXR->RemoveNodePoseMapping(i.first,u);
		}
	}
	previousTimestamp = timestamp_ms;
	if(start_xr_session)
	{
		renderState.openXR->StartSession();
		renderState.openXR->MakeActions();
		start_xr_session=false;
		end_xr_session=false;
	}
	else if(end_xr_session)
	{
		renderState.openXR->EndSession();
		start_xr_session=false;
		end_xr_session=false;
	}
}


bool Renderer::OnDeviceRemoved()
{
	InvalidateDeviceObjects();
	return true;
}
#define WAIT_TIME_TO_SHOW_ADDRESS_BAR (3.0f)

void Renderer::OnFrameMove(double fTime,float time_step)
{
	int num_connected_servers = 0;
	bool gained_connection=false;
	const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
	for(avs::uid server_uid:server_uids)
	{
		std::shared_ptr<teleport::client::SessionClient> sessionClient=client::SessionClient::GetSessionClient(server_uid);
		num_connected_servers+=sessionClient->IsConnected()?1:0;
		// returns true if a connection occurred:
		if(sessionClient->HandleConnections())
		{
			auto ir=GetInstanceRenderer(server_uid);
			sessionClient->SetGeometryCache(ir->geometryCache.get());
			config.StoreRecentURL(sessionClient->GetConnectionURL().c_str());
			gained_connection=true;
		}
	}
	if(gained_connection)
		gui.SetGuiType(GuiType::None);
	if (!num_connected_servers)
	{
		if (gui.GetGuiType()==GuiType::None)
		{
			static float invisibleTime=0.0f;
			invisibleTime+=time_step;
			if(invisibleTime>WAIT_TIME_TO_SHOW_ADDRESS_BAR)
			{
				ShowHideGui();
				invisibleTime=0.0f;
			}
		}
	}
	vec2 clientspace_input;
	static vec2 stored_clientspace_input(0,0);
#ifdef _MSC_VER
	clientspace_input.y=((float)keydown['w']-(float)keydown['s'])*(float)(keydown[VK_SHIFT]);
	clientspace_input.x=((float)keydown['d']-(float)keydown['a'])*(float)(keydown[VK_SHIFT]);
	static int clientspace_timeout=0;
	if(clientspace_input.y!=0||clientspace_input.x!=0)
	{
		stored_clientspace_input=clientspace_input;
		clientspace_timeout=20;
	}
	else if(clientspace_timeout)
	{
		clientspace_timeout--;
		if(!clientspace_timeout)
			stored_clientspace_input=vec2(0,0);
	}
	mouseCameraInput.forward_back_input	=((float)keydown['w']-(float)keydown['s'])*(float)(!keydown[VK_SHIFT]);
	mouseCameraInput.right_left_input	=((float)keydown['d']-(float)keydown['a'])*(float)(!keydown[VK_SHIFT]);
	mouseCameraInput.up_down_input		=((float)keydown['q']-(float)keydown['z'])*(float)(!keydown[VK_SHIFT]);
	
#endif
	if (renderState.openXR)
	{
		const teleport::core::Input& local_inputs=renderState.openXR->GetServerInputs(local_server_uid,renderPlatform->GetFrameNumber());
		HandleLocalInputs(local_inputs);
		have_vr_device=renderState.openXR->HaveXRDevice();
		if (have_vr_device)
		{
			const avs::Pose &headPose_stageSpace = renderState.openXR->GetHeadPose_StageSpace();
			for(auto server_uid:client::SessionClient::GetSessionClientIds())
			{
				auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
				auto &clientServerState = sessionClient->GetClientServerState();
				clientServerState.SetHeadPose_StageSpace(headPose_stageSpace.position, *((quat*)&headPose_stageSpace.orientation));
			}
		}
	}
	if (!renderState.openXR)
		have_vr_device = false;
	if (!have_vr_device)
	{
		static float spd = 0.5f;
		crossplatform::UpdateMouseCamera(&camera
			, time_step
			, spd
			, mouseCameraState
			, mouseCameraInput
			, 14000.f, false, crossplatform::MouseCameraInput::RIGHT_BUTTON);


		// consider this to be the position relative to the local origin. Don't let it get too far from centre.
		vec3 cam_pos = camera.GetPosition();
		float r = sqrt(cam_pos.x * cam_pos.x + cam_pos.y * cam_pos.y);
		if (cam_pos.z > 2.0f)
		{
			cam_pos.z = 2.0f;
			camera.SetPosition(cam_pos);
		}
		if (cam_pos.z < 1.0f)
		{
			cam_pos.z = 1.0f;
			camera.SetPosition(cam_pos);
		}
		math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(1.f, 0.0f, 0.0f));
		auto q = camera.Orientation.GetQuaternion();
		auto q_rel = q / q0;
		for (auto server_uid : client::SessionClient::GetSessionClientIds())
		{
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			auto &clientServerState = sessionClient->GetClientServerState();
			clientServerState.SetHeadPose_StageSpace(*((vec3 *)&cam_pos), *((clientrender::quat *)&q_rel));
			const teleport::core::Input& inputs = renderState.openXR->GetServerInputs(server_uid,renderPlatform->GetFrameNumber());
			clientServerState.SetInputs(inputs);
		}
	}
	// Handle networked session.
	for (auto server_uid : client::SessionClient::GetSessionClientIds())
	{
		auto ir = GetInstanceRenderer(server_uid);
		auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
		if (sessionClient->IsConnected())
		{
			auto instanceRenderer=GetInstanceRenderer(server_uid);
			avs::DisplayInfo displayInfo = {static_cast<uint32_t>(renderState.hdrFramebuffer->GetWidth()), static_cast<uint32_t>(renderState.hdrFramebuffer->GetHeight())
											,framerate};
			const auto &nodePoses=renderState.openXR->GetNodePoses(server_uid,renderPlatform->GetFrameNumber());
		
			if (renderState.openXR)
			{
				const teleport::core::Input& inputs = renderState.openXR->GetServerInputs(server_uid,renderPlatform->GetFrameNumber());
				sessionClient->GetClientServerState().SetInputs(inputs);
			}
			sessionClient->Frame(displayInfo, sessionClient->GetClientServerState().headPose.localPose, nodePoses, instanceRenderer->receivedInitialPos, GetOriginPose(server_uid),
								 sessionClient->GetClientServerState().input, fTime, time_step);

		}

		gui.SetVideoDecoderStatus(ir->GetVideoDecoderStatus());
	}

	if (config.options.simulateVR && !have_vr_device)
	{
		FillInControllerPose(0, -1.f);
		FillInControllerPose(1, 1.f);
	}
	ExecConsoleCommands();
}

void Renderer::OnMouseButtonPressed(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, int nMouseWheelDelta)
{
	mouseCameraInput.MouseButtons
		|= (bLeftButtonDown ? crossplatform::MouseCameraInput::LEFT_BUTTON : 0)
		| (bRightButtonDown ? crossplatform::MouseCameraInput::RIGHT_BUTTON : 0)
		| (bMiddleButtonDown ? crossplatform::MouseCameraInput::MIDDLE_BUTTON : 0);
}

void Renderer::OnMouseButtonReleased(bool bLeftButtonReleased, bool bRightButtonReleased, bool bMiddleButtonReleased, int nMouseWheelDelta)
{
	mouseCameraInput.MouseButtons
		&= (bLeftButtonReleased ? ~crossplatform::MouseCameraInput::LEFT_BUTTON : crossplatform::MouseCameraInput::ALL_BUTTONS)
		& (bRightButtonReleased ? ~crossplatform::MouseCameraInput::RIGHT_BUTTON : crossplatform::MouseCameraInput::ALL_BUTTONS)
		& (bMiddleButtonReleased ? ~crossplatform::MouseCameraInput::MIDDLE_BUTTON : crossplatform::MouseCameraInput::ALL_BUTTONS);
}

void Renderer::OnMouseMove(int xPos
			,int yPos,bool bLeftButtonDown
			,bool bRightButtonDown
			,bool bMiddleButtonDown
			,int nMouseWheelDelta
			 )
{
	mouseCameraInput.MouseX=xPos;
	mouseCameraInput.MouseY=yPos;
	mouseCameraInput.MouseButtons
		|= (bLeftButtonDown ? crossplatform::MouseCameraInput::LEFT_BUTTON : 0)
		| (bRightButtonDown ? crossplatform::MouseCameraInput::RIGHT_BUTTON : 0)
		| (bMiddleButtonDown ? crossplatform::MouseCameraInput::MIDDLE_BUTTON : 0);
}


void Renderer::OnKeyboard(unsigned wParam,bool bKeyDown,bool gui_shown)
{
	if (gui_shown)
		gui.OnKeyboard(wParam, bKeyDown);
	else
	{
		switch (wParam) 
		{
	#ifdef _MSC_VER
			case VK_LEFT: 
			case VK_RIGHT: 
			case VK_UP: 
			case VK_DOWN:
				return;
	#endif
			default:
				int  k = tolower(wParam);
				if (k > 255)
					return;
				keydown[k] = bKeyDown ? 1 : 0;
			break; 
		}
	}
	if (!bKeyDown && ! gui.URLInputActive())
	{
		switch (wParam)
		{
#if TELEPORT_INTERNAL_CHECKS
		case 'O':
			if(gui.GetGuiType()!=teleport::GuiType::Debug)
			{
				gui.SetGuiType(teleport::GuiType::Debug);
			}
			else
			{
				gui.SetGuiType(teleport::GuiType::None);
			}
			if(renderState.openXR)
				renderState.openXR->SetOverlayEnabled(gui.GetGuiType() == teleport::GuiType::Debug);
			break;
		case 'N':
			renderState.show_node_overlays = !renderState.show_node_overlays;
			break;
		case 'B':
		{
			static int b=0;
			b++;
			if(b>=xr_profile_to_controller_model_name.size())
				b=0;
			auto p = xr_profile_to_controller_model_name.begin();
			int i=0;
			for(i=0;i<b;i++)
			{
				p++;
			}

			XrBindingsChanged("/user/hand/left", p->first);
			XrBindingsChanged("/user/hand/right", p->first);
			break;
		}
		case 'R':
			RecompileShaders();
			break;
#endif
		case 'K':
		{
				const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
				for (const auto &server_uid : server_uids)
				{
					auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
					if(sessionClient->IsConnected())
						sessionClient->Disconnect(0);
				}
		}
		break;
		case 'Y':
		{
				const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
				for (const auto &server_uid : server_uids)
				{
					auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
					if (sessionClient->IsConnected())
						sessionClient->GetClientPipeline().decoder.toggleShowAlphaAsColor();
				}
			}
			break;
		#ifdef _MSC_VER
		case VK_SPACE:
			ShowHideGui();
			break;
		case VK_NUMPAD0: //Display full PBR rendering.
			ChangePass(clientrender::ShaderMode::PBR);
			break;
		case VK_NUMPAD1: //Display only albedo/diffuse.
			ChangePass(clientrender::ShaderMode::ALBEDO);
			break;
		case VK_NUMPAD4: //Display normals for native PC client frame-of-reference.
			ChangePass(clientrender::ShaderMode::NORMALS);
			break;
		case VK_NUMPAD5: //Display normals swizzled for matching Unreal output.
			ChangePass(clientrender::ShaderMode::DEBUG_ANIM);
			break;
		case VK_NUMPAD6: //Display normals swizzled for matching Unity output.
			ChangePass(clientrender::ShaderMode::LIGHTMAPS);
			break;
		case VK_NUMPAD2: //Display normals swizzled for matching Unity output.
			ChangePass(clientrender::ShaderMode::NORMAL_VERTEXNORMALS);
			break;
		case VK_NUMPAD7: //.
			ChangePass(clientrender::ShaderMode::REZZING);
			break;
		case VK_NUMPAD8: //.
			ChangePass(clientrender::ShaderMode::UVS);
			break;
			#endif
		default:
			break;
		}
	}
}

void Renderer::ShowHideGui()
{
	if(gui.GetGuiType()==GuiType::None)
		gui.SetGuiType(GuiType::Connection);
	else
		gui.SetGuiType(GuiType::None);
		
	auto localInstanceRenderer=GetInstanceRenderer(0);
	auto &localGeometryCache=localInstanceRenderer->geometryCache;
		auto selfRoot=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.self_node_uid);
	auto rightHand=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
	auto leftHand=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.local_left_hand_uid);
	avs::uid point_anim_uid=localGeometryCache->mAnimationManager.GetUidByName("Point");
	rightHand->GetOrCreateComponent<AnimationComponent>()->setAnimation(point_anim_uid);
	leftHand->GetOrCreateComponent<AnimationComponent>()->setAnimation(point_anim_uid);
	AnimationState *leftAnimState=leftHand->GetOrCreateComponent<AnimationComponent>()->GetAnimationState(point_anim_uid);
	AnimationState *rightAnimState=rightHand->GetOrCreateComponent<AnimationComponent>()->GetAnimationState(point_anim_uid);
	if (gui.GetGuiType()!=GuiType::None)
	{
		selfRoot->SetVisible(true);
		if (renderState.openXR)
			renderState.openXR->SetOverlayEnabled(gui.GetGuiType()==GuiType::Debug);
		// If we've just started to show the gui, let's make the hands point, so the index finger alone is extended for typing.
		if(leftAnimState)
		{
			leftAnimState->setAnimationTimeMode(clientrender::AnimationTimeMode::TIMESTAMP);
			leftAnimState->speed=1.0f;
		}
		if(rightAnimState)
		{
			rightAnimState->setAnimationTimeMode(clientrender::AnimationTimeMode::TIMESTAMP);
			rightAnimState->speed=1.0f;
		}
		rightHand->GetLocalTransform();
		// The AIM pose should be mapped to the index finger.
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_hand_node_uid,"left/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_hand_node_uid,"right/input/aim/pose");
		// Now adjust the local transform of the hand object based on the root being at the finger.
		clientrender::Transform finger_to_hand;
		// "global" transform is in hand's root cooords.
		if(rightHand->GetSkeletonInstance())
		{
			auto rSkeleton=rightHand->GetSkeletonInstance()->GetSkeleton();
			clientrender::Transform hand_to_finger=rSkeleton->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
			clientrender::Transform root_to_hand=rSkeleton->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
			// We want the transform in the finger's coords!
			finger_to_hand=hand_to_finger.GetInverse();
			{
				Transform tr=rightHand->GetLocalTransform();
				tr.m_Translation=tr.m_Rotation.RotateVector(finger_to_hand.m_Translation);
				rightHand->SetLocalTransform(tr);
			}
			{
				Transform tr=leftHand->GetLocalTransform();
				tr.m_Translation=tr.m_Rotation.RotateVector(finger_to_hand.m_Translation);
				leftHand->SetLocalTransform(tr);
			}
		}
	}
	else
	{
//		selfRoot->SetVisible(false);
	// reverse the animation.
		if(leftAnimState)
			leftAnimState->speed=-1.0f;
		if(rightAnimState)
			rightAnimState->speed=-1.0f;
		// The GRIP pose should be mapped to the palm.
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_hand_node_uid,"left/input/grip/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_hand_node_uid,"right/input/grip/pose");
		rightHand->SetLocalTransform(lobbyGeometry.palm_to_hand_r);
		leftHand->SetLocalTransform(lobbyGeometry.palm_to_hand_l);
	}
	
	{
		auto rightHand=localGeometryCache->mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
		auto rSkeleton=localGeometryCache->mSkeletonManager.Get(lobbyGeometry.hand_skeleton_uid);
		if(rSkeleton&&rSkeleton->GetBoneByName("IndexFinger4_R"))
		{
			clientrender::Transform hand_to_finger=rSkeleton->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
			vec3 v=rightHand->GetLocalTransform().LocalToGlobal(hand_to_finger.m_Translation);
			lobbyGeometry.index_finger_offset=*((vec3*)&v);
		}
	}
}

void Renderer::WriteHierarchy(int tabDepth, std::shared_ptr<clientrender::Node> node)
{
	for(int i = 0; i < tabDepth; i++)
	{
		std::cout << "\t";
	}
	std::cout << node->id << "(" << node->name << ")" << std::endl;

	for(auto child : node->GetChildren())
	{
		WriteHierarchy(tabDepth + 1, child.lock());
	}
}

void Renderer::WriteHierarchies(avs::uid server)
{
	std::cout << "Node Tree\n----------------------------------\n";
	auto ir=GetInstanceRenderer(server);
	for(std::weak_ptr<clientrender::Node> node :ir->geometryCache->mNodeManager->GetRootNodes())
	{
		WriteHierarchy(0, node.lock());
	}

	std::cout << std::endl;
}

// We only ever create one view in this example, but in general, this should return a new value each time it's called.
int Renderer::AddView()
{
	static int last_view_id=0;
	// We override external_framebuffer here and pass "true" to demonstrate how external depth buffers are used.
	// In this case, we use renderState.hdrFramebuffer's depth buffer.
	return last_view_id++;
}

void Renderer::ResizeView(int view_id,int W,int H)
{
	if(renderState.hDRRenderer)
		renderState.hDRRenderer->SetBufferSize(W,H);
	if(renderState.hdrFramebuffer)
	{
		renderState.hdrFramebuffer->SetWidthAndHeight(W,H);
		renderState.hdrFramebuffer->SetAntialiasing(1);
	}
}

void Renderer::RenderDesktopView(int view_id, void* context, void* renderTexture, int w, int h, long long frame, void* context_allocator)
{
	if(reload_shaders)
		LoadShaders();
	static platform::core::Timer timer;
	static float last_t = 0.0f;
	timer.UpdateTime();
	if (last_t != 0.0f && timer.TimeSum != last_t)
	{
		framerate = 1000.0f / (timer.TimeSum - last_t);
	}
	last_t = timer.TimeSum;
	crossplatform::GraphicsDeviceContext	deviceContext;
	deviceContext.setDefaultRenderTargets(renderTexture, nullptr, 0, 0, w, h);
	deviceContext.platform_context = context;
	deviceContext.renderPlatform = renderPlatform;
	deviceContext.viewStruct.view_id = view_id;
	deviceContext.viewStruct.depthTextureStyle = crossplatform::PROJECTION;

	// For desktop, we will use ClientTime for the predicted display time.
	deviceContext.predictedDisplayTimeS = client::ClientTime::GetInstance().GetTimeS();
	//deviceContext.viewStruct.Init();
	crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
	renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
	renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
	SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext)
	SIMUL_COMBINED_PROFILE_START(deviceContext, "all");
	SIMUL_COMBINED_PROFILE_START(deviceContext, "Renderer::Render");
	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);

	if (renderState.openXR&&renderState.openXR->IsSessionActive())
	{
		crossplatform::Texture *eyeTexture=renderState.openXR->GetRenderTexture(0);
		renderPlatform->DrawTexture(deviceContext,0,0,w,h,eyeTexture);
	}
	else
	{
		renderState.hdrFramebuffer->Activate(deviceContext);
		renderState.hdrFramebuffer->Clear(deviceContext, 0.5f, 0.25f, 0.5f, 0.f, reverseDepth ? 0.f : 1.f);

		float aspect = (float)viewport.w / (float)viewport.h;
		if (reverseDepth)
			deviceContext.viewStruct.proj = camera.MakeDepthReversedProjectionMatrix(aspect);
		else
			deviceContext.viewStruct.proj = camera.MakeProjectionMatrix(aspect);
		
		//auto &clientServerState=sessionClient->GetClientServerState();
		// Init the viewstruct in local space - i.e. with no server offsets.
		{
			math::SimulOrientation globalOrientation;
			const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
			for (const auto &server_uid : server_uids)
			{
				auto instanceRenderer = GetInstanceRenderer(server_uid);
				auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			// global pos/orientation:
				auto &clientServerState = sessionClient->GetClientServerState();
				globalOrientation.SetPosition((const float*)&clientServerState.headPose.localPose.position);
				math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(-1.f, 0.0f, 0.0f));
				math::Quaternion q1 = (const float*)&clientServerState.headPose.localPose.orientation;
				auto q_rel = q1/q0;
				globalOrientation.SetOrientation(q_rel);
				deviceContext.viewStruct.view = globalOrientation.GetInverseMatrix().RowPointer(0);
				// MUST call init each frame.
			}
			deviceContext.viewStruct.Init();
		}

		if (externalTexture)
		{
			renderPlatform->DrawTexture(deviceContext, 0, 0, w, h, externalTexture);
		}
		else
		{
			RenderView(deviceContext);
		}	
		vec4 white(1.f, 1.f, 1.f, 1.f);
		// We must deactivate the depth buffer here, in order to use it as a texture:
  		renderState.hdrFramebuffer->DeactivateDepth(deviceContext);
		static int lod = 0;
		static int tt = 1000;
		tt--;
		if (!tt)
		{
			lod++;
		}
		lod = lod % 8;
		//auto instanceRenderer=GetInstanceRenderer(server_uid);
		//auto &geometryCache=instanceRenderer->geometryCache;
		if (!tt)
		{
			tt=1000;
		}
		renderState.hdrFramebuffer->Deactivate(deviceContext);
		renderState.hDRRenderer->Render(deviceContext, renderState.hdrFramebuffer->GetTexture(), 1.0f, gamma);
		DrawGUI(deviceContext,false);
	#ifdef ONSCREEN_PROF
		static std::string profiling_text;
		renderPlatform->LinePrint(deviceContext, profiling_text.c_str());
	#endif
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
#ifdef ONSCREEN_PROF
	static char c = 0;
	c--;
	if(!c)
		profiling_text=renderPlatform->GetGpuProfiler()->GetDebugText();
#endif

	SIMUL_COMBINED_PROFILE_END(deviceContext);
	if (renderState.openXR->HaveXRDevice())
	{
		// Note we do this even when the device is inactive.
		//  if we don't, we will never receive the transition from XR_SESSION_STATE_READY to XR_SESSION_STATE_FOCUSED
		renderState.openXR->SetCurrentFrameDeviceContext(&deviceContext);
		renderState.openXR->RenderFrame(renderDelegate, overlayDelegate);
		if (renderState.openXR->IsXRDeviceRendering())
			SetExternalTexture(renderState.openXR->GetRenderTexture());
	}
	else
	{
		SetExternalTexture(nullptr);
	}
	errno = 0;
	renderPlatform->GetGpuProfiler()->EndFrame(deviceContext);
	SIMUL_COMBINED_PROFILE_ENDFRAME(deviceContext)
}

void Renderer::DrawGUI(platform::crossplatform::GraphicsDeviceContext &deviceContext,bool mode_3d)
{
	// Show the 2D GUI on Desktop view, only if the 3D gui is not visible.
	if(mode_3d)
	{
		gui.Render3DConnectionGUI(deviceContext);
	}
	else
	{
		if (gui.GetGuiType()==GuiType::Connection && !config.options.simulateVR)
		{
			gui.Render2DConnectionGUI(deviceContext);
		}
		if (!renderState.openXR || !renderState.openXR->IsSessionActive())
		{
			DrawOSD(deviceContext);
		}
		// in vr mode, the OSD is drawn from a callback in an OpenXR layer.
	}
}

void Renderer::RemoveView(int)
{
}

void Renderer::UpdateVRGuiMouse()
{
	// In engineering axes:
	avs::Pose p = renderState.openXR->GetActionPose(client::RIGHT_AIM_POSE);
	// from hand to overlay is diff:
	vec3 start = *((vec3 *)&p.position);
	static vec3 y = {0, 1.0f, 0};
	avs::Pose overlay_pose = renderState.openXR->ConvertGLStageSpacePoseToLocalSpacePose(renderState.openXR->overlay.pose);
	vec3 overlay_centre = *((vec3 *)&overlay_pose.position);
	crossplatform::Quaternionf ovrl_q = *(crossplatform::Quaternionf *)&overlay_pose.orientation;
	crossplatform::Quaternionf aim_q = *(crossplatform::Quaternionf *)&p.orientation;
	vec3 dir = aim_q.RotateVector(y);
	float rightTrigger = renderState.openXR->GetActionFloatState(client::RIGHT_TRIGGER);
	vec2 m={0,0};
	if(renderState.openXR->overlay.overlayType==teleport::client::OverlayType::QUAD)
	{
		vec3 normal = -ovrl_q.RotateVector(y);
		vec3 diff = overlay_centre - start;
		float nf = -dot(normal, diff);

		float nr = -dot(dir, normal);
		float distance = nf / nr;
		hit = start + distance * dir;

		vec3 h = hit - overlay_centre;
		vec3 h_on_surface = (!ovrl_q).RotateVector(h);
		h_on_surface.x /= (float)renderState.openXR->overlay.size.width;
		h_on_surface.z /= (float)renderState.openXR->overlay.size.height;
		m={h_on_surface.x, h_on_surface.z};
	}
	else if (renderState.openXR->overlay.overlayType == teleport::client::OverlayType::CYLINDER)
	{
		float R			=renderState.openXR->overlay.radius;
		vec3 u			=start - overlay_pose.position;
		float b			=2.0f*dot(u,dir);
		float c			=dot(u,u)-R*R;
		float distance	= 0.5f * (-b + sqrt(b * b - 4.f * c));
		vec3 hit		=u+distance*dir;
		float azimuth	= atan2f(-hit.x, hit.y);
		float aspectRatio=2.0f;
		float height	=renderState.openXR->overlay.angularSpanRadians/aspectRatio*renderState.openXR->overlay.radius;
		m = {-azimuth / (renderState.openXR->overlay.angularSpanRadians / 2.f), hit.z * 2.f / height};
	}
	gui.SetDebugGuiMouse(m, rightTrigger > 0.5f);
}

void Renderer::RenderVROverlay(crossplatform::GraphicsDeviceContext &deviceContext)
{
	DrawOSD(deviceContext);
}

void Renderer::DrawOSD(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (gui.GetGuiType()!=GuiType::Debug)
		return;
	if(renderState.openXR)
	{
		UpdateVRGuiMouse();
	}
	gui.BeginFrame(deviceContext);
	gui.BeginDebugGui(deviceContext);
	gui.LinePrint(fmt::format("Framerate {0}", framerate));
	static vec4 white(1.f, 1.f, 1.f, 1.f);
	static vec4 text_colour={1.0f,1.0f,0.5f,1.0f};
	static vec4 background={0.0f,0.0f,0.0f,0.5f};
	if(renderState.overridePixelShader.length())
	{
		gui.LinePrint(fmt::format("Override Shader: {0}", renderState.overridePixelShader));
	}		
	
	if(gui.Tab("Debug"))
	{
		gui.DebugPanel(debugOptions);
		gui.EndTab();
	}
	if(gui.Tab("Network"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			auto instanceRenderer = GetInstanceRenderer(server_uid);
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			gui.NetworkPanel(sessionClient->GetClientPipeline());
		}
		gui.EndTab();
	}
	if(gui.Tab("Camera"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			auto instanceRenderer = GetInstanceRenderer(server_uid);
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
		
			auto &clientServerState=sessionClient->GetClientServerState();
			vec3 offset=camera.GetPosition();
			auto originPose=GetOriginPose(server_uid);
			gui.LinePrint(instanceRenderer->receivedInitialPos?(platform::core::QuickFormat("Origin: %4.4f %4.4f %4.4f", originPose.position.x, originPose.position.y, originPose.position.z)):"Origin:", white);
			gui.LinePrint(platform::core::QuickFormat(" Local: %4.4f %4.4f %4.4f", clientServerState.headPose.localPose.position.x, clientServerState.headPose.localPose.position.y, clientServerState.headPose.localPose.position.z),white);
		//	gui.LinePrint(platform::core::QuickFormat(" Final: %4.4f %4.4f %4.4f\n", clientServerState.headPose.globalPose.position.x, clientServerState.headPose.globalPose.position.y, clientServerState.headPose.globalPose.position.z),white);
			if (instanceRenderer->videoPosDecoded)
			{
				gui.LinePrint(platform::core::QuickFormat(" Video: %4.4f %4.4f %4.4f", instanceRenderer->videoPos.x, instanceRenderer->videoPos.y, instanceRenderer->videoPos.z), white);
			}	
			else
			{
				gui.LinePrint(platform::core::QuickFormat(" Video: -"), white);
			}
		}
		gui.EndTab();
	}
	if(gui.Tab("Video"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			std::shared_ptr<clientrender::InstanceRenderer> r=GetInstanceRenderer(server_uid);
			if(r)
			{
				clientrender::AVSTextureHandle th = r->GetInstanceRenderState().avsTexture;
				clientrender::AVSTexture& tx = *th;
				AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
				if(ti)
				{
					gui.LinePrint(platform::core::QuickFormat("Video Texture"), white);
					gui.DrawTexture(ti->texture);
				}
				gui.LinePrint(platform::core::QuickFormat("Specular"), white);
				gui.DrawTexture(r->GetInstanceRenderState().specularCubemapTexture);
				gui.LinePrint(platform::core::QuickFormat("DiffuseC"), white);
				gui.DrawTexture(r->GetInstanceRenderState().diffuseCubemapTexture);
				gui.LinePrint(platform::core::QuickFormat("Lighting"), white);
				gui.DrawTexture(r->GetInstanceRenderState().lightingCubemapTexture);
			}
		}
		gui.EndTab();
	}
	if(gui.Tab("Cubemap"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			std::shared_ptr<clientrender::InstanceRenderer> r=GetInstanceRenderer(server_uid);
			if(r)
			{
				gui.CubemapOSD(r->GetInstanceRenderState().videoTexture);
			}
		}
		gui.EndTab();
	}
	if(gui.Tab("Decoder"))
	{
		gui.LinePrint("Decoder Status:", white);
		const auto& names = magic_enum::enum_names<avs::DecoderStatusNames>();
		avs::DecoderStatus status = gui.GetVideoDecoderStatus();
		if (status == avs::DecoderStatus::DecoderUnavailable)
		{
			gui.LinePrint(std::string(names[0]).c_str(), white);
		}
		else
		{
			for (size_t i = 0; i < 8; i++)
			{
				uint32_t value = (uint32_t(status) & uint32_t(0xF << (i * 4))) >> (i * 4);
				std::string str = std::string(names[i + 1]) + ": %d";
				gui.LinePrint(platform::core::QuickFormat(str.c_str(), value), white);
			}
		}
		gui.LinePrint(" ", white);

		gui.LinePrint("Decoder Parameters:", white);
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			const avs::DecoderParams& params = sessionClient->GetClientPipeline().decoderParams;
			const auto& videoCodecNames = magic_enum::enum_names<avs::VideoCodec>();
			const auto& decoderFrequencyNames = magic_enum::enum_names<avs::DecodeFrequency>();
			gui.LinePrint(platform::core::QuickFormat("Video Codec: %s", videoCodecNames[size_t(params.codec)]));
			gui.LinePrint(platform::core::QuickFormat("Decode Frequency: %s", decoderFrequencyNames[size_t(params.decodeFrequency)]));
			gui.LinePrint(platform::core::QuickFormat("Use 10-Bit Decoding: %s", params.use10BitDecoding ? "true" : "false"));
			gui.LinePrint(platform::core::QuickFormat("Chroma Format: %s", params.useYUV444ChromaFormat ? "YUV444" : "YUV420"));
			gui.LinePrint(platform::core::QuickFormat("Use Alpha Layer Decoding: %s", params.useAlphaLayerDecoding ? "true" : "false"));
		}
		gui.EndTab();
	}
	if(gui.Tab("Geometry"))
	{			
		gui.GeometryOSD();
		gui.EndTab();
	}
	if(gui.Tab("Tags"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			auto instanceRenderer = GetInstanceRenderer(server_uid);
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
		gui.TagOSD(instanceRenderer->videoTagDataCubeArray,instanceRenderer->videoTagDataCube);
		}
		gui.EndTab();
	}
	if(gui.Tab("Controllers"))
	{
		const std::set<avs::uid> &server_uids = client::SessionClient::GetSessionClientIds();
		for (const auto &server_uid : server_uids)
		{
			auto instanceRenderer = GetInstanceRenderer(server_uid);
			auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
			gui.InputsPanel(server_uid,sessionClient.get(), renderState.openXR);
		}
		gui.EndTab();
	}
	gui.EndDebugGui(deviceContext);
	gui.EndFrame(deviceContext);

	//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
	//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
	//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
}

void Renderer::SetExternalTexture(crossplatform::Texture* t)
{
	externalTexture = t;
	have_vr_device = (externalTexture != nullptr);
}
/*
void Renderer::PrintHelpText(crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.framePrintY = 8;
	deviceContext.framePrintX = renderState.hdrFramebuffer->GetWidth() / 2;
	renderPlatform->LinePrint(deviceContext, "K: Connect/Disconnect");
	renderPlatform->LinePrint(deviceContext, "O: Toggle OSD");
	renderPlatform->LinePrint(deviceContext, "V: Show video");
	renderPlatform->LinePrint(deviceContext, "C: Toggle render from centre");
	renderPlatform->LinePrint(deviceContext, "M: Change rendermode");
	renderPlatform->LinePrint(deviceContext, "R: Recompile shaders");
	renderPlatform->LinePrint(deviceContext, "NUM 0: PBR");
	renderPlatform->LinePrint(deviceContext, "NUM 1: Albedo");
	renderPlatform->LinePrint(deviceContext, "NUM 4: Unswizzled Normals");
	renderPlatform->LinePrint(deviceContext, "NUM 5: Debug animation");
	renderPlatform->LinePrint(deviceContext, "NUM 6: Lightmaps");
	renderPlatform->LinePrint(deviceContext, "NUM 2: Vertex Normals");
}*/

void Renderer::HandleLocalInputs(const teleport::core::Input& local_inputs)
{
	for(const  auto &i:local_inputs.binaryEvents)
	{
		if(i.inputID==local_menu_input_id)
		{
			// do this on *releasing* the button:
			if(i.activated==false)
				ShowHideGui();
		}
		else if(i.inputID==local_cycle_osd_id)
		{
			// do this on *releasing* the button:
			if(i.activated==false)
			{
				switch(gui.GetGuiType())
				{
					case GuiType::None:
						gui.SetGuiType(GuiType::Connection);
					break;
					case GuiType::Connection:
						gui.SetGuiType(GuiType::None);
					break;
					default:
						gui.SetGuiType(GuiType::None);
					break;
				};
				if (renderState.openXR)
					renderState.openXR->SetOverlayEnabled(gui.GetGuiType() == GuiType::Debug);
			}
		}
		else if(i.inputID==local_cycle_shader_id)
		{
			// do this on *releasing* the button:
			if(i.activated==false)
			{
				shaderMode =ShaderMode((int(shaderMode)+1)%int(ShaderMode::NUM));
				ChangePass(shaderMode);
			}
		}
	}
}