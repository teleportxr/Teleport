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
#include "Platform/CrossPlatform/BaseFramebuffer.h"
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
		"Skin",
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
			auto &img=((vulkan::Texture*)texture)->AsVulkanImage();
			return new avs::SurfaceVulkan(&img,texture->width,texture->length,vulkan::RenderPlatform::ToVulkanFormat((texture->pixelFormat)));
	#endif
}

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
}

Renderer::~Renderer()
{
	InvalidateDeviceObjects(); 
}

void Renderer::Init(crossplatform::RenderPlatform* r, teleport::client::OpenXR* u, teleport::PlatformWindow* active_window)
{
	// Initialize the audio (asynchronously)
	renderPlatform = r;
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
	auto connectButtonHandler = std::bind(&client::SessionClient::ConnectButtonHandler, 1, std::placeholders::_1);
	gui.SetConnectHandler(connectButtonHandler);
	auto cancelConnectHandler = std::bind(&client::SessionClient::CancelConnectButtonHandler, 1);
	gui.SetCancelConnectHandler(cancelConnectHandler);
	
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

	renderState.videoTexture = renderPlatform->CreateTexture();
	renderState.specularCubemapTexture = renderPlatform->CreateTexture();
	renderState.diffuseCubemapTexture = renderPlatform->CreateTexture();
	renderState.lightingCubemapTexture = renderPlatform->CreateTexture();
	errno = 0;
	RecompileShaders();

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
	localGeometryCache.setCacheFolder("assets/localGeometryCache");

	client::SessionClient::GetSessionClient(server_uid)->SetSessionCommandInterface(GetInstanceRenderer(server_uid).get());

	InitLocalGeometry();
}

void Renderer::InitLocalGeometry()
{
	auto localInstanceRenderer=GetInstanceRenderer(0);
	auto &localResourceCreator=localInstanceRenderer->resourceCreator;
	// initialize the default local geometry:
	avs::uid hand_mesh_uid = avs::GenerateUid();
	lobbyGeometry.hand_skin_uid = avs::GenerateUid();
	avs::uid point_anim_uid = avs::GenerateUid();
	geometryDecoder.decodeFromFile("assets/localGeometryCache/meshes/Hand.mesh_compressed",avs::GeometryPayloadType::Mesh,&localResourceCreator,hand_mesh_uid);
	geometryDecoder.decodeFromFile("assets/localGeometryCache/skins/Hand.skin",avs::GeometryPayloadType::Skin,&localResourceCreator, lobbyGeometry.hand_skin_uid);
	geometryDecoder.decodeFromFile("assets/localGeometryCache/animations/Point.anim",avs::GeometryPayloadType::Animation,&localResourceCreator, point_anim_uid);
	geometryDecoder.WaitFromDecodeThread();
	
	localResourceCreator.Update(0.0f);
	// Generate local uid's for the nodes and resources.
	lobbyGeometry.left_root_node_uid	= avs::GenerateUid();
	lobbyGeometry.right_root_node_uid	= avs::GenerateUid();
	avs::uid grey_material_uid			= avs::GenerateUid();
	avs::uid blue_material_uid			= avs::GenerateUid();
	avs::uid red_material_uid			= avs::GenerateUid();
	auto &localGeometryCache=localInstanceRenderer->geometryCache;
	{
		avs::Material avsMaterial;
		avsMaterial.name="local grey";
		avsMaterial.pbrMetallicRoughness.metallicFactor=0.0f;
		avsMaterial.pbrMetallicRoughness.baseColorFactor={.5f,.5f,.5f,.5f};
		localResourceCreator.CreateMaterial(grey_material_uid,avsMaterial);// not used just now.
		avsMaterial.name="local blue glow";
		avsMaterial.emissiveFactor={0.0f,0.2f,0.5f};
		localResourceCreator.CreateMaterial(blue_material_uid,avsMaterial);
		avsMaterial.name="local red glow";
		avsMaterial.emissiveFactor={0.4f,0.1f,0.1f};
		localResourceCreator.CreateMaterial(red_material_uid,avsMaterial);

		localGeometryCache.mMaterialManager.Get(grey_material_uid)->SetShaderOverride("local_hand");
		localGeometryCache.mMaterialManager.Get(blue_material_uid)->SetShaderOverride("local_hand");
		localGeometryCache.mMaterialManager.Get(red_material_uid)->SetShaderOverride("local_hand");
	}
	avs::Node avsNode;

	avsNode.name		="local Left Root";
	localResourceCreator.CreateNode(lobbyGeometry.left_root_node_uid,avsNode);
	avsNode.name		="local Right Root";
	localResourceCreator.CreateNode(lobbyGeometry.right_root_node_uid,avsNode);

	avsNode.data_type	=avs::NodeDataType::Mesh;
	
	avsNode.data_uid	=hand_mesh_uid;
	avsNode.materials.push_back(blue_material_uid);
	avsNode.materials.push_back(grey_material_uid);
	
	avsNode.name						="local Left Hand";
	avsNode.skinID						=lobbyGeometry.hand_skin_uid;
	avsNode.animations.push_back(point_anim_uid);
	avsNode.materials[0]				=blue_material_uid;
	avsNode.parentID					=lobbyGeometry.left_root_node_uid;
	avsNode.localTransform.rotation		={0.707f,0,0,0.707f};
	avsNode.localTransform.scale		={-1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position		={0.f,0.1f,0.f};
	lobbyGeometry.local_left_hand_uid	=avs::GenerateUid();
	localResourceCreator.CreateNode(lobbyGeometry.local_left_hand_uid,avsNode);

	avsNode.name="local Right Hand";
	avsNode.materials[0]				=red_material_uid;
	avsNode.parentID					=lobbyGeometry.right_root_node_uid;
	avsNode.localTransform.scale		={1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position		={0.f,0.1f,0.f};
	lobbyGeometry.local_right_hand_uid	=avs::GenerateUid();
	localResourceCreator.CreateNode(lobbyGeometry.local_right_hand_uid,avsNode);

	if(renderState.openXR)
	{
		renderState.openXR->SetFallbackBinding(client::LEFT_AIM_POSE	,"left/input/aim/pose");
		renderState.openXR->SetFallbackBinding(client::RIGHT_AIM_POSE	,"right/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_root_node_uid	,"left/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_root_node_uid	,"right/input/aim/pose");
		
		renderState.openXR->SetFallbackBinding(client::LEFT_GRIP_POSE	,"left/input/grip/pose");
		renderState.openXR->SetFallbackBinding(client::RIGHT_GRIP_POSE	,"right/input/grip/pose");
		
		renderState.openXR->SetFallbackBinding(client::MOUSE_LEFT_BUTTON	,"mouse/left/click");
		renderState.openXR->SetFallbackBinding(client::MOUSE_RIGHT_BUTTON	,"mouse/right/click");

		// Hard-code the menu button
		renderState.openXR->SetHardInputMapping(local_server_uid,local_menu_input_id		,avs::InputType::IntegerEvent,teleport::client::ActionId::SHOW_MENU);
		renderState.openXR->SetHardInputMapping(local_server_uid,local_cycle_osd_id		,avs::InputType::IntegerEvent,teleport::client::ActionId::X);
		renderState.openXR->SetHardInputMapping(local_server_uid,local_cycle_shader_id	,avs::InputType::IntegerEvent,teleport::client::ActionId::Y);
	}
	
	auto rightHand=localGeometryCache.mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
	lobbyGeometry.palm_to_hand_r=rightHand->GetLocalTransform();
	auto leftHand=localGeometryCache.mNodeManager->GetNode(lobbyGeometry.local_left_hand_uid);
	lobbyGeometry.palm_to_hand_l=leftHand->GetLocalTransform();
}

void Renderer::UpdateShaderPasses()
{
	auto SetPasses= [this](const char *techname)
		{
			ShaderPassSetup shaderPassSetup;
			shaderPassSetup.technique		=renderState.pbrEffect->GetTechniqueByName(techname);
			shaderPassSetup.noLightmapPass	=shaderPassSetup.technique->GetPass("pbr_nolightmap");
			shaderPassSetup.lightmapPass	=shaderPassSetup.technique->GetPass("pbr_lightmap");
			shaderPassSetup.digitizingPass	=shaderPassSetup.technique->GetPass("digitizing");
			if (!renderState.overridePassName.empty() 
				&& shaderPassSetup.technique->HasPass(renderState.overridePassName.c_str()))
				shaderPassSetup.overridePass	=shaderPassSetup.technique->GetPass(renderState.overridePassName.c_str());
			return shaderPassSetup;
		};
	renderState.pbrEffect_solid					=SetPasses("solid");
	renderState.pbrEffect_solidAnim				=SetPasses("solid_anim");
	renderState.pbrEffect_solid					=SetPasses("solid");
	renderState.pbrEffect_solidMultiview		=SetPasses("solid_multiview");
	renderState.pbrEffect_solidAnimMultiview	=SetPasses("solid_anim_multiview");
	
	renderState.pbrEffect_transparent			=SetPasses("transparent");
	renderState.pbrEffect_transparentMultiview	=SetPasses("transparent_multiview");
	
	renderState.pbrEffect_solidMultiviewTechnique_localPass	=renderState.pbrEffect_solidMultiview.technique->GetPass("local");
}

// This allows live-recompile of shaders. 
void Renderer::RecompileShaders()
{
	renderPlatform->RecompileShaders();
	text3DRenderer.RecompileShaders();
	renderState.hDRRenderer->RecompileShaders();
	gui.RecompileShaders();
	TextCanvas::RecompileShaders();
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
	SAFE_DELETE(renderState.diffuseCubemapTexture);
	SAFE_DELETE(renderState.specularCubemapTexture);
	SAFE_DELETE(renderState.lightingCubemapTexture);
	SAFE_DELETE(renderState.videoTexture);
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
	float elevation	= elev_mult*(y-0.5f);
	q.Reset();
	q.Rotate(elevation,vec3(-1.0f, 0, 0));
	q.Rotate(azimuth,vec3(0,0,1.0f));
	vec3 point_dir=q*vec3(0, 1.0f, 0);
	static float roll=-1.3f;
	q.Rotate(roll*offset, point_dir);

	avs::Pose pose;
	pose.position=*((vec3*)&footspace_pos);
	pose.orientation=*((const vec4*)&q);

	renderState.openXR->SetFallbackPoseState(index?client::RIGHT_GRIP_POSE:client::LEFT_GRIP_POSE,pose);
	pose.position.z-=0.1f;
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
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	std::shared_ptr<Node> origin_node=GetInstanceRenderer(server_uid)->geometryCache.mNodeManager->GetNode(clientServerState.origin_node_uid);
	if(origin_node)
	{
		origin_pose.position=origin_node->GetGlobalPosition();
		origin_pose.orientation=*((vec4*)&origin_node->GetGlobalRotation());
	}

	return origin_pose;
}

void Renderer::RenderVRView(crossplatform::GraphicsDeviceContext& deviceContext)
{
	RenderView(deviceContext);
	gui.Render3DGUI(deviceContext);
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

	auto sessionClient = client::SessionClient::GetSessionClient(server_uid);
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	// TODO: This should render only if no background clients are connected.
	if (!sessionClient->IsConnected())
	{
		if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
		{
			crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
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
		if(!renderState.openXR->IsPassthroughActive())
		{
			renderState.cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());
			renderPlatform->DrawQuad(deviceContext);
			renderState.cubemapClearEffect->Unapply(deviceContext);
		}
	}

	// Init the viewstruct in global space - i.e. with the server offsets.
	avs::Pose origin_pose;
	std::shared_ptr<Node> origin_node=GetInstanceRenderer(server_uid)->geometryCache.mNodeManager->GetNode(clientServerState.origin_node_uid);
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
	auto l=nodePoseStates.find(lobbyGeometry.left_root_node_uid);
	vec4 white={1.f,1.f,1.f,1.f};
	std::vector<vec4> hand_pos_press;
	if(l!=nodePoseStates.end())
	{
		avs::Pose handPose	= l->second.pose_footSpace.pose;
		vec3 pos		= LocalToGlobal(handPose,*((vec3*)&lobbyGeometry.index_finger_offset));
		//Clang can't handle overloaded functions, where a parameter could be upcast to another overload. Hence split the function calls.
	/*	if (multiview) 
			renderPlatform->PrintAt3dPos(*mvgdc, (const float*)&pos, "L", (const float*)&white);
		else
			renderPlatform->PrintAt3dPos(deviceContext, (const float*)&pos, "L", (const float*)&white);*/
		vec4 pos4;
		pos4.xyz			= (const float*)&pos;
		pos4.w				= 0.0f;
		hand_pos_press.push_back(pos4);
	}

	auto r=nodePoseStates.find(lobbyGeometry.right_root_node_uid);
	if(r!=nodePoseStates.end())
	{
		avs::Pose rightHand = r->second.pose_footSpace.pose;
		vec3 pos = LocalToGlobal(rightHand,*((vec3*)&lobbyGeometry.index_finger_offset));
		vec4 pos4;
		pos4.xyz = (const float*)&pos;
		pos4.w = 0.0f;
		hand_pos_press.push_back(pos4);
	}
	static bool override_have_vr_device=false;
	gui.Update(hand_pos_press, have_vr_device||override_have_vr_device);
	if(have_vr_device || override_have_vr_device)
		gui.Render3DGUI(deviceContext);

	renderState.selected_uid=gui.GetSelectedUid();
	if((have_vr_device || override_have_vr_device)&&(!sessionClient->IsConnected()||gui.IsVisible()||config.options.showGeometryOffline))
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
			renderState.overridePassName = "";
			break;
		case ShaderMode::ALBEDO:
			renderState.overridePassName = "albedo_only";
			break;
		case ShaderMode::NORMALS:
			renderState.overridePassName = "normals";
			break;
		case ShaderMode::DEBUG_ANIM:
			renderState.overridePassName = "debug_anim";
			break;
		case ShaderMode::LIGHTMAPS:
			renderState.overridePassName = "debug_lightmaps";
			break;
		case ShaderMode::NORMAL_VERTEXNORMALS:
			renderState.overridePassName = "normal_vertexnormals";
			break;
		case ShaderMode::REZZING:
			renderState.overridePassName = "digitizing";
			break;
		default:
			renderState.overridePassName = "";
			break;
	}
	UpdateShaderPasses();
}
void Renderer::Update(double timestamp_ms)
{
	double timeElapsed_s = (timestamp_ms - previousTimestamp) / 1000.0f;//ms to seconds

	teleport::client::ServerTimestamp::tick(timeElapsed_s);
	for(auto i:instanceRenderers)
	{
		i.second->geometryCache.Update(static_cast<float>(timeElapsed_s));
		i.second->resourceCreator.Update(static_cast<float>(timeElapsed_s));

		if(i.first!=0)
		{
			const auto &removedNodeUids=i.second->geometryCache.mNodeManager->GetRemovedNodeUids();
			// Node has been deleted!
			for(const auto u:removedNodeUids)
				renderState.openXR->RemoveNodePoseMapping(i.first,u);
		}
	}
	previousTimestamp = timestamp_ms;
	if(start_xr_session)
	{
		renderState.openXR->StartSession();
		{
			renderState.openXR->MakeActions();
		}
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

void Renderer::OnFrameMove(double fTime,float time_step,bool have_headset)
{
	std::shared_ptr<teleport::client::SessionClient> sessionClient=client::SessionClient::GetSessionClient(server_uid);
	// returns true if a connection occurred:
	if(sessionClient->HandleConnections())
	{
		auto ir=GetInstanceRenderer(server_uid);
		sessionClient->SetGeometryCache(&ir->geometryCache);
		config.StoreRecentURL(sessionClient->GetConnectionURL().c_str());
		gui.Hide();
	}
	if(renderState.openXR->IsSessionActive())
	{
		if (!sessionClient->IsConnected())
		{
			if (!gui.IsVisible())
			{
				ShowHideGui();
			}
		}
	}
	using_vr = have_headset;
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
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	if (renderState.openXR)
	{
		const teleport::core::Input& local_inputs=renderState.openXR->GetServerInputs(local_server_uid,renderPlatform->GetFrameNumber());
		HandleLocalInputs(local_inputs);
		have_vr_device=renderState.openXR->HaveXRDevice();
		if (have_headset)
		{
			const avs::Pose &headPose_stageSpace=renderState.openXR->GetHeadPose_StageSpace();
			clientServerState.SetHeadPose_StageSpace(headPose_stageSpace.position, *((quat*)&headPose_stageSpace.orientation));
		}
	}
	if (!have_headset)
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
			cam_pos.z = 2.0f;
		if (cam_pos.z < 1.0f)
			cam_pos.z = 1.0f;
		math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(1.f, 0.0f, 0.0f));
		auto q = camera.Orientation.GetQuaternion();
		auto q_rel = q / q0;
		clientServerState.SetHeadPose_StageSpace(*((vec3*)&cam_pos), *((clientrender::quat*)&q_rel));
		const teleport::core::Input& inputs = renderState.openXR->GetServerInputs(local_server_uid,renderPlatform->GetFrameNumber());
		clientServerState.SetInputs( inputs);
	}
	// Handle networked session.
	auto ir=GetInstanceRenderer(server_uid);
	if (sessionClient->IsConnected())
	{
		auto instanceRenderer=GetInstanceRenderer(server_uid);
		avs::DisplayInfo displayInfo = {static_cast<uint32_t>(renderState.hdrFramebuffer->GetWidth()), static_cast<uint32_t>(renderState.hdrFramebuffer->GetHeight())};
		const auto &nodePoses=renderState.openXR->GetNodePoses(server_uid,renderPlatform->GetFrameNumber());
		
		if (renderState.openXR)
		{
			const teleport::core::Input& inputs = renderState.openXR->GetServerInputs(server_uid,renderPlatform->GetFrameNumber());
			clientServerState.SetInputs(inputs);
		}
		sessionClient->Frame(displayInfo, clientServerState.headPose.localPose, nodePoses, instanceRenderer->receivedInitialPos, GetOriginPose(server_uid),
			clientServerState.input,fTime, time_step);

	}

	gui.SetVideoDecoderStatus(ir->GetVideoDecoderStatus());

	if (!have_headset)
	{
		FillInControllerPose(0, -1.f);
		FillInControllerPose(1, 1.f);
	}
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
		case 'O':
			show_osd =!show_osd;
			break;
		case 'H':
			WriteHierarchies(server_uid);
			break;
		case 'N':
			renderState.show_node_overlays = !renderState.show_node_overlays;
			break;
		case 'K':
			{
				auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
				if(sessionClient->IsConnected())
					sessionClient->Disconnect(0);
			}
			break;
		case 'R':
			RecompileShaders();
			break;
		case 'Y':
			{
				auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
				if (sessionClient->IsConnected())
					sessionClient->GetClientPipeline().decoder.toggleShowAlphaAsColor();
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
			#endif
		default:
			break;
		}
	}
}

void Renderer::ShowHideGui()
{
	gui.ShowHide();
	auto localInstanceRenderer=GetInstanceRenderer(0);
	auto &localGeometryCache=localInstanceRenderer->geometryCache;
	auto rightHand=localGeometryCache.mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
	auto leftHand=localGeometryCache.mNodeManager->GetNode(lobbyGeometry.local_left_hand_uid);
	avs::uid point_anim_uid=localGeometryCache.mAnimationManager.GetUidByName("Point");
	rightHand->animationComponent.setAnimation(point_anim_uid);
	leftHand->animationComponent.setAnimation(point_anim_uid);
	AnimationState *leftAnimState=leftHand->animationComponent.GetAnimationState(point_anim_uid);
	AnimationState *rightAnimState=rightHand->animationComponent.GetAnimationState(point_anim_uid);
	if(gui.IsVisible())
	{
		show_osd = false; //TODO: Find a better fix for OSD and Keyboard resource collision in Vulkan/ImGui - AJR.

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
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_root_node_uid,"left/input/aim/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_root_node_uid,"right/input/aim/pose");
		// Now adjust the local transform of the hand object based on the root being at the finger.
		clientrender::Transform finger_to_hand;
		// "global" transform is in hand's root cooords.
		auto rSkin=rightHand->GetSkinInstance()->GetSkin();
		clientrender::Transform hand_to_finger=rSkin->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
		clientrender::Transform root_to_hand=rSkin->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
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
	else
	{
	// reverse the animation.
		if(leftAnimState)
			leftAnimState->speed=-1.0f;
		if(rightAnimState)
			rightAnimState->speed=-1.0f;
		// The GRIP pose should be mapped to the palm.
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.left_root_node_uid,"left/input/grip/pose");
		renderState.openXR->MapNodeToPose(local_server_uid, lobbyGeometry.right_root_node_uid,"right/input/grip/pose");
		rightHand->SetLocalTransform(lobbyGeometry.palm_to_hand_r);
		leftHand->SetLocalTransform(lobbyGeometry.palm_to_hand_l);
	}
	
	{
		auto rightHand=localGeometryCache.mNodeManager->GetNode(lobbyGeometry.local_right_hand_uid);
		auto rSkin=localGeometryCache.mSkinManager.Get(lobbyGeometry.hand_skin_uid);
		clientrender::Transform hand_to_finger=rSkin->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
		vec3 v=rightHand->GetLocalTransform().LocalToGlobal(hand_to_finger.m_Translation);
		lobbyGeometry.index_finger_offset=*((vec3*)&v);
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
	for(std::shared_ptr<clientrender::Node> node :ir->geometryCache.mNodeManager->GetRootNodes())
	{
		WriteHierarchy(0, node);
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
	//deviceContext.viewStruct.Init();
	crossplatform::SetGpuProfilingInterface(deviceContext, renderPlatform->GetGpuProfiler());
	renderPlatform->GetGpuProfiler()->SetMaxLevel(5);
	renderPlatform->GetGpuProfiler()->StartFrame(deviceContext);
	SIMUL_COMBINED_PROFILE_START(deviceContext, "Renderer::Render");
	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);

	renderState.hdrFramebuffer->Activate(deviceContext);
	renderState.hdrFramebuffer->Clear(deviceContext, 0.5f, 0.25f, 0.5f, 0.f, reverseDepth ? 0.f : 1.f);

	float aspect = (float)viewport.w / (float)viewport.h;
	if (reverseDepth)
		deviceContext.viewStruct.proj = camera.MakeDepthReversedProjectionMatrix(aspect);
	else
		deviceContext.viewStruct.proj = camera.MakeProjectionMatrix(aspect);
		
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	// Init the viewstruct in local space - i.e. with no server offsets.
	{
		math::SimulOrientation globalOrientation;
		// global pos/orientation:
		globalOrientation.SetPosition((const float*)&clientServerState.headPose.localPose.position);
		math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(-1.f, 0.0f, 0.0f));
		math::Quaternion q1 = (const float*)&clientServerState.headPose.localPose.orientation;
		auto q_rel = q1/q0;
		globalOrientation.SetOrientation(q_rel);
		deviceContext.viewStruct.view = globalOrientation.GetInverseMatrix().RowPointer(0);
		// MUST call init each frame.
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
	// Show the 2D GUI on Desktop view, only if the 3D gui is not visible.
	if(!gui.IsVisible()&&!show_osd)
		gui.Render2DGUI(deviceContext);
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
	auto instanceRenderer=GetInstanceRenderer(server_uid);
	auto &geometryCache=instanceRenderer->geometryCache;
	if (!tt)
	{
		tt=1000;
	}
	//
	#if 0
	{
		static int tw = 128;
		int x = 0, y = 0;//renderState.hdrFramebuffer->GetHeight()-tw*2;
		avs::uid sel_uid=gui.GetSelectedUid();
		if(sel_uid!=0)
		{
			std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
			std::shared_ptr<Node> n=geometryCache.mNodeManager->GetNode(sel_uid);
			if(n.get())
			{
				avs::uid gi_uid=n->GetGlobalIlluminationTextureUid();
				if(gi_uid)
				{
					int w=tw*2;
					std::shared_ptr<Texture> t=geometryCache.mTextureManager.Get(gi_uid);
					if(t)
						renderPlatform->DrawTexture(deviceContext, x, y, w, w, t->GetSimulTexture());
				}
			}
			std::shared_ptr<clientrender::Texture> t=geometryCache.mTextureManager.Get(sel_uid);
			if(t.get())
			{
				int w=tw*2;
				renderPlatform->DrawTexture(deviceContext, x, y, w, w, t->GetSimulTexture());
			}
		}
		else if (show_textures)
		{
			std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
			auto& textures = geometryCache.mTextureManager.GetCache(cacheLock);
			for (auto t : textures)
			{
				clientrender::Texture* pct = t.second.resource.get();
				renderPlatform->DrawTexture(deviceContext, x, y, tw, tw, pct->GetSimulTexture());
				x += tw;
				if (x > renderState.hdrFramebuffer->GetWidth() - tw)
				{
					x = 0;
					y += tw;
				}
			}
			y += tw;
			renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, resourceCreator.m_DummyWhite.get()->GetSimulTexture());
			renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, resourceCreator.m_DummyNormal.get()->GetSimulTexture());
			renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, resourceCreator.m_DummyCombined.get()->GetSimulTexture());
			renderPlatform->DrawTexture(deviceContext, x += tw, y, tw, tw, resourceCreator.m_DummyBlack.get()->GetSimulTexture());
		}
	}
	#endif
	renderState.hdrFramebuffer->Deactivate(deviceContext);
	renderState.hDRRenderer->Render(deviceContext, renderState.hdrFramebuffer->GetTexture(), 1.0f, gamma);
	
	DrawOSD(deviceContext);
#ifdef ONSCREEN_PROF
	static std::string profiling_text;
	renderPlatform->LinePrint(deviceContext, profiling_text.c_str());
#endif
	SIMUL_COMBINED_PROFILE_END(deviceContext);
#ifdef ONSCREEN_PROF
	static char c = 0;
	c--;
	if(!c)
		profiling_text=renderPlatform->GetGpuProfiler()->GetDebugText();
#endif
}


void Renderer::RemoveView(int)
{
}

void Renderer::DrawOSD(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (!show_osd||gui.IsVisible())
		return;
	auto instanceRenderer=GetInstanceRenderer(server_uid);
	auto &geometryCache=instanceRenderer->geometryCache;
	gui.setGeometryCache(&geometryCache);
	auto sessionClient=client::SessionClient::GetSessionClient(server_uid);
	gui.setSessionClient(sessionClient.get());
	if(renderState.openXR)
	{
		avs::Pose p=renderState.openXR->GetActionPose(client::RIGHT_AIM_POSE);
		// from hand to overlay is diff:
		vec3 start				=*((vec3*)&p.position);
		static vec3 y			={0,1.0f,0};
		avs::Pose overlay_pose	=renderState.openXR->ConvertGLStageSpacePoseToLocalSpacePose(renderState.openXR->overlay.pose) ;
		vec3 overlay_centre		=*((vec3*)&overlay_pose.position);
		crossplatform::Quaternionf  ovrl_q=*(crossplatform::Quaternionf*)&overlay_pose.orientation;
		vec3 normal				=-ovrl_q.RotateVector(y);
		vec3 diff				=overlay_centre-start;
		float nf				=-dot(normal,diff);
		
		crossplatform::Quaternionf  aim_q=*(crossplatform::Quaternionf*)&p.orientation;
		vec3 dir				=aim_q.RotateVector(y);
		float nr				=-dot(dir,normal);
		float distance			=nf/nr;
		hit						=start+distance*dir;

		vec3 h					=hit-overlay_centre;
		vec3 h_on_surface		=(!ovrl_q).RotateVector(h);
		h_on_surface.x			/=(float)renderState.openXR->overlay.size.width;
		h_on_surface.z			/=(float)renderState.openXR->overlay.size.height;
		vec2 m(h_on_surface.x,h_on_surface.z);
		float rightTrigger		=renderState.openXR->GetActionFloatState(client::RIGHT_TRIGGER);
		gui.SetDebugGuiMouse(m,rightTrigger>0.5f);
	}
	gui.BeginDebugGui(deviceContext);
	vec4 white(1.f, 1.f, 1.f, 1.f);
	vec4 text_colour={1.0f,1.0f,0.5f,1.0f};
	vec4 background={0.0f,0.0f,0.0f,0.5f};
	auto status = sessionClient->GetConnectionStatus();
	avs::StreamingConnectionState streamingStatus = sessionClient->GetStreamingConnectionState();

	deviceContext.framePrintX = 8;
	deviceContext.framePrintY = 8;
	gui.LinePrint(fmt::format("Server {0}:{1}", sessionClient->GetServerIP().c_str(), sessionClient->GetPort()).c_str());
	gui.LinePrint(fmt::format("  Session Status: {0}",teleport::client::StringOf(status)),white);
	gui.LinePrint(fmt::format("Streaming Status: {0}",avs::stringOf(streamingStatus)),white);
	gui.LinePrint(platform::core::QuickFormat("Framerate: %4.4f", framerate));
	
	if(gui.Tab("Debug"))
	{
		gui.DebugPanel(debugOptions);
		gui.EndTab();
	}
	if(gui.Tab("Network"))
	{
		gui.NetworkPanel(sessionClient->GetClientPipeline());
		gui.EndTab();
	}
	if(gui.Tab("Camera"))
	{
		auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
		vec3 offset=camera.GetPosition();
		auto originPose=GetOriginPose(server_uid);
		gui.LinePrint(instanceRenderer->receivedInitialPos?(platform::core::QuickFormat("Origin: %4.4f %4.4f %4.4f", originPose.position.x, originPose.position.y, originPose.position.z)):"Origin:", white);
		gui.LinePrint(platform::core::QuickFormat(" Local: %4.4f %4.4f %4.4f", clientServerState.headPose.localPose.position.x, clientServerState.headPose.localPose.position.y, clientServerState.headPose.localPose.position.z),white);
		gui.LinePrint(platform::core::QuickFormat(" Final: %4.4f %4.4f %4.4f\n", clientServerState.headPose.globalPose.position.x, clientServerState.headPose.globalPose.position.y, clientServerState.headPose.globalPose.position.z),white);
		if (instanceRenderer->videoPosDecoded)
		{
			gui.LinePrint(platform::core::QuickFormat(" Video: %4.4f %4.4f %4.4f", instanceRenderer->videoPos.x, instanceRenderer->videoPos.y, instanceRenderer->videoPos.z), white);
		}	
		else
		{
			gui.LinePrint(platform::core::QuickFormat(" Video: -"), white);
		}
		gui.EndTab();
	}
	if(gui.Tab("Video"))
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
			gui.DrawTexture(renderState.specularCubemapTexture);
			gui.LinePrint(platform::core::QuickFormat("DiffuseC"), white);
			gui.DrawTexture(renderState.diffuseCubemapTexture);
			gui.LinePrint(platform::core::QuickFormat("Lighting"), white);
			gui.DrawTexture(renderState.lightingCubemapTexture);
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
		const avs::DecoderParams& params = sessionClient->GetClientPipeline().decoderParams;
		const auto& videoCodecNames = magic_enum::enum_names<avs::VideoCodec>();
		const auto& decoderFrequencyNames = magic_enum::enum_names<avs::DecodeFrequency>();
		gui.LinePrint(platform::core::QuickFormat("Video Codec: %s", videoCodecNames[size_t(params.codec)]));
		gui.LinePrint(platform::core::QuickFormat("Decode Frequency: %s", decoderFrequencyNames[size_t(params.decodeFrequency)]));
		gui.LinePrint(platform::core::QuickFormat("Use 10-Bit Decoding: %s", params.use10BitDecoding ? "true" : "false"));
		gui.LinePrint(platform::core::QuickFormat("Chroma Format: %s", params.useYUV444ChromaFormat ? "YUV444" : "YUV420"));
		gui.LinePrint(platform::core::QuickFormat("Use Alpha Layer Decoding: %s", params.useAlphaLayerDecoding ? "true" : "false"));
		gui.EndTab();
	}
	if(gui.Tab("Cubemap"))
	{
		gui.CubemapOSD(renderState.videoTexture);
		gui.EndTab();
	}
	if(gui.Tab("Geometry"))
	{
		gui.GeometryOSD();
		gui.EndTab();
	}
	if(gui.Tab("Tags"))
	{
		gui.TagOSD(instanceRenderer->videoTagDataCubeArray,instanceRenderer->videoTagDataCube);
		gui.EndTab();
	}
	if(gui.Tab("Controllers"))
	{
		gui.InputsPanel(server_uid,sessionClient.get(), renderState.openXR);
#if 0
		gui.LinePrint( "CONTROLS\n");
		if(renderState.openXR)
		{
			vec3 pos=gui.Get3DPos();
			gui.LinePrint(fmt::format("gui pos: {: .3f},{: .3f},{: .3f}",pos.x,pos.y,pos.z).c_str());
			gui.LinePrint(renderState.openXR->GetDebugString().c_str());
		}
		else
		{
		#ifdef _MSC_VER
			gui.LinePrint(platform::core::QuickFormat("     Shift: %d ",keydown[VK_SHIFT]));
		#endif
			gui.LinePrint(platform::core::QuickFormat("     W %d A %d S %d D %d",keydown['w'],keydown['a'],keydown['s'],keydown['d']));
			gui.LinePrint(platform::core::QuickFormat("     Mouse: %d %d %3.3d",mouseCameraInput.MouseX,mouseCameraInput.MouseY,mouseCameraState.right_left_spd));
			gui.LinePrint(platform::core::QuickFormat("      btns: %d",mouseCameraInput.MouseButtons));
		}
#endif
		gui.EndTab();
	}
	gui.EndDebugGui(deviceContext);

	//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
	//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
	//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
}

void Renderer::SetExternalTexture(crossplatform::Texture* t)
{
	externalTexture = t;
	have_vr_device = (externalTexture != nullptr);
}

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
}

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
				show_osd =!show_osd;
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