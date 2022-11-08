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

const char *stringof(avs::GeometryPayloadType t)
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

//TODO: Implement Vector, Matrix and Quaternion conversions between avs:: <-> math:: <-> CppSl.h - AJR
template<typename T1, typename T2> T1 ConvertVec2(const T2& value) { return T1(value.x, value.y); }
template<typename T1, typename T2> T1 ConvertVec3(const T2& value) { return T1(value.x, value.y, value.z); }
template<typename T1, typename T2> T1 ConvertVec4(const T2& value) { return T1(value.x, value.y, value.z, value.w); }
mat4 ConvertMat4(const float value[4][4]) 
{
	mat4 result;
	result.M[0][0] = value[0][0]; result.M[0][1] = value[0][1]; result.M[0][2] = value[0][2]; result.M[0][3] = value[0][3];
	result.M[1][0] = value[1][0]; result.M[1][1] = value[1][1]; result.M[1][2] = value[1][2]; result.M[1][3] = value[1][3];
	result.M[2][0] = value[2][0]; result.M[2][1] = value[2][1]; result.M[2][2] = value[2][2]; result.M[2][3] = value[2][3];
	result.M[3][0] = value[3][0]; result.M[3][1] = value[3][1]; result.M[3][2] = value[3][2]; result.M[3][3] = value[3][3];
	return  result;
}


struct AVSTextureImpl :public clientrender::AVSTexture
{
	AVSTextureImpl(crossplatform::Texture *t)
		:texture(t)
	{
	}
	crossplatform::Texture *texture = nullptr;
	avs::SurfaceBackendInterface* createSurface() const override
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
};

Renderer::Renderer(client::ClientDeviceState *c,clientrender::NodeManager *localNodeManager,clientrender::NodeManager *remoteNodeManager,teleport::client::SessionClient *sc,teleport::Gui& g
	,teleport::client::Config &cfg)
	:sessionClient(sc)
	,localGeometryCache(localNodeManager)
	,geometryCache(remoteNodeManager)
	,clientDeviceState(c)
	,gui(g)
	,config(cfg)
{
	sessionClient->SetSessionCommandInterface(this);
	if (!timestamp_initialized)
#ifdef _MSC_VER
		platformStartTimestamp = avs::Platform::getTimestamp();
#else
		platformStartTimestamp = avs::Platform::getTimestamp();
#endif
	timestamp_initialized=true;
	sessionClient->SetResourceCreator(&resourceCreator);
	sessionClient->SetGeometryCache(&geometryCache);
	resourceCreator.SetGeometryCache(&geometryCache);
	localResourceCreator.SetGeometryCache(&localGeometryCache);

	clientrender::Tests::RunAllTests();
}

Renderer::~Renderer()
{
	clientPipeline.pipeline.deconfigure();
	InvalidateDeviceObjects(); 
}

void Renderer::Init(crossplatform::RenderPlatform *r,teleport::client::OpenXR *u,teleport::PlatformWindow* active_window)
{
	// Initialize the audio (asynchronously)
#ifdef _MSC_VER
	audioPlayer.initializeAudioDevice();
#endif
	renderPlatform = r;
	openXR=u;

	PcClientRenderPlatform.SetSimulRenderPlatform(r);
	r->SetShaderBuildMode(crossplatform::ShaderBuildMode::BUILD_IF_CHANGED);
	resourceCreator.Initialize(&PcClientRenderPlatform, clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);
	localResourceCreator.Initialize(&PcClientRenderPlatform, clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);

	hDRRenderer = new crossplatform::HdrRenderer();

	hdrFramebuffer	=renderPlatform->CreateFramebuffer();
	hdrFramebuffer->SetFormat(crossplatform::RGBA_16_FLOAT);
	hdrFramebuffer->SetDepthFormat(crossplatform::D_32_FLOAT);
	hdrFramebuffer->SetAntialiasing(1);
	camera.SetPositionAsXYZ(0.f,0.f,2.f);
	vec3 look(0.f,1.f,0.f),up(0.f,0.f,1.f);
	camera.LookInDirection(look,up);

	camera.SetHorizontalFieldOfViewDegrees(HFOV);

	// Automatic vertical fov - depends on window shape:
	camera.SetVerticalFieldOfViewDegrees(0.f);
	
	//const float aspect = hdrFramebuffer->GetWidth() / hdrFramebuffer->GetHeight();
	//cubemapConstants.localHorizFOV = HFOV * clientrender::DEG_TO_RAD;
	//cubemapConstants.localVertFOV = clientrender::GetVerticalFOVFromHorizontal(cubemapConstants.localHorizFOV, aspect);

	crossplatform::CameraViewStruct vs;
	vs.exposure=1.f;
	vs.farZ=3000.f;
	vs.nearZ=0.01f;
	vs.gamma=1.0f;
	vs.InfiniteFarPlane=true;
	vs.projection=crossplatform::DEPTH_REVERSE;
	
	camera.SetCameraViewStruct(vs);

	memset(keydown,0,sizeof(keydown));

	hDRRenderer->RestoreDeviceObjects(renderPlatform);
	hdrFramebuffer->RestoreDeviceObjects(renderPlatform);

	gui.RestoreDeviceObjects(renderPlatform,active_window);
	auto connectButtonHandler = std::bind(&Renderer::ConnectButtonHandler, this,std::placeholders::_1);
	gui.SetConnectHandler(connectButtonHandler);
	auto cancelConnectHandler = std::bind(&Renderer::CancelConnectButtonHandler, this);
	gui.SetCancelConnectHandler(cancelConnectHandler);
	videoTexture = renderPlatform->CreateTexture();
	specularCubemapTexture = renderPlatform->CreateTexture();
	diffuseCubemapTexture = renderPlatform->CreateTexture();
	lightingCubemapTexture = renderPlatform->CreateTexture();
	errno=0;
	RecompileShaders();

	pbrConstants.RestoreDeviceObjects(renderPlatform);
	pbrConstants.LinkToEffect(pbrEffect,"pbrConstants");
	cubemapConstants.RestoreDeviceObjects(renderPlatform);
	cubemapConstants.LinkToEffect(cubemapClearEffect, "CubemapConstants");
	cameraConstants.RestoreDeviceObjects(renderPlatform);
	stereoCameraConstants.RestoreDeviceObjects(renderPlatform);
	tagDataIDBuffer.RestoreDeviceObjects(renderPlatform, 1, true);
	tagDataCubeBuffer.RestoreDeviceObjects(renderPlatform, maxTagDataSize, false, true);
	lightsBuffer.RestoreDeviceObjects(renderPlatform,10,false,true);
	boneMatrices.RestoreDeviceObjects(renderPlatform);
	boneMatrices.LinkToEffect(pbrEffect, "boneMatrices");

	avs::Context::instance()->setMessageHandler(msgHandler,nullptr);

	// initialize the default local geometry:
	geometryDecoder.decodeFromFile("assets/localGeometryCache/meshes/Hand.mesh_compressed",avs::GeometryPayloadType::Mesh,&localResourceCreator);
	geometryDecoder.decodeFromFile("assets/localGeometryCache/skins/Hand.skin",avs::GeometryPayloadType::Skin,&localResourceCreator);
	geometryDecoder.decodeFromFile("assets/localGeometryCache/animations/Point.anim",avs::GeometryPayloadType::Animation,&localResourceCreator);
	
	avs::uid wand_uid = 11;
	auto uids=localGeometryCache.mMeshManager.GetAllIDs();
	if (uids.size())
	{
		wand_uid = uids[0];
	}
	else
	{
		TELEPORT_BREAK_ONCE("Wand mesh not found");
	}
	uids=localGeometryCache.mSkinManager.GetAllIDs();
	hand_skin_uid=0;
	if (uids.size())
	{
		hand_skin_uid = uids[0];
	}
	else
	{
		TELEPORT_BREAK_ONCE("Skin not found");
	}
	uids=localGeometryCache.mAnimationManager.GetAllIDs();
	avs::uid point_anim_uid=0;
	if (uids.size())
	{
		point_anim_uid = uids[0];
	}
	else
	{
		TELEPORT_BREAK_ONCE("Anim not found");
	}
	{
		avs::Material avsMaterial;
		avsMaterial.name="local material";
		avsMaterial.pbrMetallicRoughness.metallicFactor=0.0f;
		avsMaterial.pbrMetallicRoughness.baseColorFactor={.5f,.5f,.5f,.5f};
		localResourceCreator.CreateMaterial(14,avsMaterial);// not used just now.
		avsMaterial.name="local blue glow";
		avsMaterial.emissiveFactor={0.0f,0.2f,0.5f};
		localResourceCreator.CreateMaterial(15,avsMaterial);
		avsMaterial.name="local red glow";
		avsMaterial.emissiveFactor={0.4f,0.1f,0.1f};
		localResourceCreator.CreateMaterial(16,avsMaterial);

		localGeometryCache.mMaterialManager.Get(14)->SetShaderOverride("local_hand");
		localGeometryCache.mMaterialManager.Get(15)->SetShaderOverride("local_hand");
		localGeometryCache.mMaterialManager.Get(16)->SetShaderOverride("local_hand");
	}
	avs::Node avsNode;
	avsNode.data_type=avs::NodeDataType::None;
	avsNode.data_uid=0;
	avsNode.name = "local Left Root";
	localResourceCreator.CreateNode(1,avsNode);
	avsNode.name = "local Right Root";
	localResourceCreator.CreateNode(2,avsNode);

	avsNode.data_type=avs::NodeDataType::Mesh;
	//avsNode.transform.scale = { 0.2f,0.2f,0.2f };
	avsNode.data_uid=wand_uid;
	avsNode.materials.push_back(15);
	avsNode.materials.push_back(14);
	
	avsNode.name = "local Left Hand";
	avsNode.skinID=hand_skin_uid;
	avsNode.animations.push_back(point_anim_uid);
	avsNode.materials[0]=15;
	avsNode.parentID=1;
	avsNode.localTransform.rotation={0.707f,0,0,0.707f};
	avsNode.localTransform.scale={-1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position={0.f,0.1f,0.f};
	local_left_hand_uid=23;
	localResourceCreator.CreateNode(local_left_hand_uid,avsNode);

	avsNode.name="local Right Hand";
	avsNode.materials[0]=16;
	avsNode.parentID=2;
	avsNode.localTransform.scale={1.f,1.f,1.f};
	// 10cm forward, because root of hand is at fingers.
	avsNode.localTransform.position={0.f,0.1f,0.f};
	local_right_hand_uid=24;
	localResourceCreator.CreateNode(local_right_hand_uid,avsNode);

	if(openXR)
	{
		openXR->SetFallbackBinding(client::LEFT_AIM_POSE,"left/input/aim/pose");
		openXR->SetFallbackBinding(client::RIGHT_AIM_POSE,"right/input/aim/pose");
		openXR->MapNodeToPose(local_server_uid,1,"left/input/aim/pose");
		openXR->MapNodeToPose(local_server_uid,2,"right/input/aim/pose");
		
		openXR->SetFallbackBinding(client::LEFT_GRIP_POSE,"left/input/grip/pose");
		openXR->SetFallbackBinding(client::RIGHT_GRIP_POSE,"right/input/grip/pose");
		
		openXR->SetFallbackBinding(client::MOUSE_LEFT_BUTTON,"mouse/left/click");
		openXR->SetFallbackBinding(client::MOUSE_RIGHT_BUTTON,"mouse/right/click");

	/*	openXR->MapNodeToPose(local_server_uid,1,"left/input/grip/pose");
		openXR->MapNodeToPose(local_server_uid,2,"right/input/grip/pose");*/

		// Hard-code the menu button
		openXR->SetHardInputMapping(local_server_uid,local_menu_input_id,avs::InputType::IntegerEvent,teleport::client::ActionId::SHOW_MENU);
		openXR->SetHardInputMapping(local_server_uid,local_cycle_osd_id,avs::InputType::IntegerEvent,teleport::client::ActionId::X);
		openXR->SetHardInputMapping(local_server_uid,local_cycle_shader_id,avs::InputType::IntegerEvent,teleport::client::ActionId::Y);
	}
	
	auto rightHand=localGeometryCache.mNodeManager->GetNode(local_right_hand_uid);
	palm_to_hand_r=rightHand->GetLocalTransform();
	auto leftHand=localGeometryCache.mNodeManager->GetNode(local_left_hand_uid);
	palm_to_hand_l=leftHand->GetLocalTransform();

	geometryDecoder.setCacheFolder(config.GetStorageFolder());
}

// This allows live-recompile of shaders. 
void Renderer::RecompileShaders()
{
	renderPlatform->RecompileShaders();
	hDRRenderer->RecompileShaders();
	gui.RecompileShaders();
	delete pbrEffect;
	delete cubemapClearEffect;
	pbrEffect = renderPlatform->CreateEffect("pbr");
	cubemapClearEffect = renderPlatform->CreateEffect("cubemap_clear");

	pbrEffect_solidTechnique=pbrEffect->GetTechniqueByName("solid");
	pbrEffect_solidTechnique_localPass=pbrEffect_solidTechnique->GetPass("local");
	pbrEffect_solid_multiviewTechnique = pbrEffect->GetTechniqueByName("solid_multiview");
	pbrEffect_solid_multiviewTechnique_localPass = pbrEffect_solid_multiviewTechnique->GetPass("local");
	_RWTagDataIDBuffer = cubemapClearEffect->GetShaderResource("RWTagDataIDBuffer");
	cubemapClearEffect_TagDataCubeBuffer	= cubemapClearEffect->GetShaderResource("TagDataCubeBuffer");
	_lights = pbrEffect->GetShaderResource("lights");
	plainTexture		=cubemapClearEffect->GetShaderResource("plainTexture");
	RWTextureTargetArray=cubemapClearEffect->GetShaderResource("RWTextureTargetArray");
	cubemapClearEffect_TagDataIDBuffer		=cubemapClearEffect->GetShaderResource("TagDataIDBuffer");
	pbrEffect_TagDataIDBuffer				=pbrEffect->GetShaderResource("TagDataIDBuffer");
	
	pbrEffect_diffuseCubemap				=pbrEffect->GetShaderResource("diffuseCubemap");
	pbrEffect_specularCubemap				=pbrEffect->GetShaderResource("specularCubemap");
	pbrEffect_diffuseTexture	=pbrEffect->GetShaderResource("diffuseTexture");
	pbrEffect_normalTexture		=pbrEffect->GetShaderResource("normalTexture");
	pbrEffect_combinedTexture	=pbrEffect->GetShaderResource("combinedTexture");
	pbrEffect_emissiveTexture	=pbrEffect->GetShaderResource("emissiveTexture");
	pbrEffect_globalIlluminationTexture=pbrEffect->GetShaderResource("globalIlluminationTexture");
}

void Renderer::InvalidateDeviceObjects()
{
	AVSTextureImpl *ti = (AVSTextureImpl*)avsTexture.get();
	if (ti)
	{
		SAFE_DELETE(ti->texture);
	}
	gui.InvalidateDeviceObjects();
	if(pbrEffect)
	{
		pbrEffect->InvalidateDeviceObjects();
		delete pbrEffect;
		pbrEffect=nullptr;
	}
	if(hDRRenderer)
		hDRRenderer->InvalidateDeviceObjects();
	if(renderPlatform)
		renderPlatform->InvalidateDeviceObjects();
	if(hdrFramebuffer)
		hdrFramebuffer->InvalidateDeviceObjects();
	SAFE_DELETE(diffuseCubemapTexture);
	SAFE_DELETE(specularCubemapTexture);
	SAFE_DELETE(lightingCubemapTexture);
	SAFE_DELETE(videoTexture);
	SAFE_DELETE(hDRRenderer);
	SAFE_DELETE(hdrFramebuffer);
	SAFE_DELETE(pbrEffect);
	SAFE_DELETE(cubemapClearEffect);
}

void Renderer::CreateTexture(clientrender::AVSTextureHandle &th,int width, int height)
{
	if (!(th))
		th.reset(new AVSTextureImpl(nullptr));
	clientrender::AVSTexture *t = th.get();
	AVSTextureImpl *ti=(AVSTextureImpl*)t;
	if(!ti->texture)
		ti->texture = renderPlatform->CreateTexture();

	// NVidia decoder needs a shared handle to the resource.
#if TELEPORT_CLIENT_USE_D3D12 && !TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER
	bool useSharedHeap = true;
#else
	bool useSharedHeap = false;
#endif

	ti->texture->ensureTexture2DSizeAndFormat(renderPlatform, width, height,1, crossplatform::RGBA_8_UNORM, true, true, 
		false, 1, 0, false, vec4(0.5f, 0.5f, 0.2f, 1.0f), 1.0f, 0, useSharedHeap);
}

void Renderer::FillInControllerPose(int index, float offset)
{
	if(!hdrFramebuffer->GetHeight())
		return;
	float x= mouseCameraInput.MouseX / (float)hdrFramebuffer->GetWidth();
	float y= mouseCameraInput.MouseY / (float)hdrFramebuffer->GetHeight();
	vec3 controller_dir	=camera.ScreenPositionToDirection(x, y, hdrFramebuffer->GetWidth() / static_cast<float>(hdrFramebuffer->GetHeight()));
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
	vec3 local_controller_dir = { 0,1.f,0 };
	crossplatform::Quaternionf q = (const float*)(&clientDeviceState->headPose.localPose.orientation);
	Multiply(local_controller_dir,q, local_controller_dir);
	float azimuth	= atan2f(-local_controller_dir.x, local_controller_dir.y);
	float elevation	= asin(local_controller_dir.z);
	q.Reset();
	q.Rotate(azimuth,vec3(0,0,1.0f));
	//q.Rotate(elevation, vec3(1.0f, 0, 0));
	vec3 point_dir=q*vec3(0, 1.0f, 0);
	static float roll=-1.3f;
	q.Rotate(roll*offset, point_dir);

	// convert from footspace to worldspace
	clientDeviceState->SetControllerPose( index,*((avs::vec3*)&footspace_pos),*((const clientrender::quat*)&q));
	avs::Pose pose;
	pose.position=*((avs::vec3*)&footspace_pos);
	pose.orientation=*((const avs::vec4*)&q);

	openXR->SetFallbackPoseState(index?client::RIGHT_GRIP_POSE:client::LEFT_GRIP_POSE,pose);
	pose.position.z-=0.1f;
	openXR->SetFallbackPoseState(index?client::RIGHT_AIM_POSE:client::LEFT_AIM_POSE,pose);
}

void Renderer::ConfigureVideo(const avs::VideoConfig& videoConfig)
{
	clientPipeline.videoConfig = videoConfig;
}

void Renderer::SetRenderPose(crossplatform::GraphicsDeviceContext& deviceContext, const avs::Pose& pose)
{
	deviceContext.viewStruct.view = openXR->CreateViewMatrixFromPose(pose);
	// MUST call init each frame.
	deviceContext.viewStruct.Init();

	crossplatform::MultiviewGraphicsDeviceContext* mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
	if (mvgdc)
	{
		vec3 deltaPosition = mvgdc->viewStructs[0].cam_pos - vec3((const float*)&pose.position);
		for (auto& viewStruct : mvgdc->viewStructs)
		{
			avs::Pose newPose = pose;
			newPose.position = ConvertVec3<avs::vec3>(viewStruct.cam_pos - deltaPosition);
			viewStruct.view = openXR->CreateViewMatrixFromPose(newPose);
			// MUST call init each frame.
			viewStruct.Init();
		}
	}	
}

void Renderer::RenderView(crossplatform::GraphicsDeviceContext& deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "RenderView");
	crossplatform::Viewport viewport = renderPlatform->GetViewport(deviceContext, 0);
	pbrEffect->UnbindTextures(deviceContext);
	// Init the viewstruct in global space - i.e. with the server offsets.
	SetRenderPose(deviceContext, clientDeviceState->headPose.globalPose);

	clientrender::AVSTextureHandle th = avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);

	if (ti)
	{
		// This will apply to both rendering methods
		cubemapClearEffect->SetTexture(deviceContext, plainTexture, ti->texture);
		tagDataIDBuffer.ApplyAsUnorderedAccessView(deviceContext, cubemapClearEffect, _RWTagDataIDBuffer);
		cubemapConstants.sourceOffset = int2(ti->texture->width - (32 * 4), ti->texture->length - 4);
		cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
		cubemapClearEffect->Apply(deviceContext, "extract_tag_data_id", 0);
		renderPlatform->DispatchCompute(deviceContext, 1, 1, 1);
		cubemapClearEffect->Unapply(deviceContext);
		cubemapClearEffect->UnbindTextures(deviceContext);

		tagDataIDBuffer.CopyToReadBuffer(deviceContext);
		const uint4* videoIDBuffer = tagDataIDBuffer.OpenReadBuffer(deviceContext);
		if (videoIDBuffer && videoIDBuffer[0].x < 32 && videoIDBuffer[0].w == 110) // sanity check
		{
			int tagDataID = videoIDBuffer[0].x;

			const auto& ct = videoTagDataCubeArray[tagDataID].coreData.cameraTransform;
			videoPos = vec3(ct.position.x, ct.position.y, ct.position.z);

			videoPosDecoded = true;
		}
		tagDataIDBuffer.CloseReadBuffer(deviceContext);
		UpdateTagDataBuffers(deviceContext);
		if (sessionClient->IsConnected())
		{
			if (lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::VIDEO)
			{
				if (videoTexture->IsCubemap())
				{
					const char* technique = clientPipeline.videoConfig.use_alpha_layer_decoding ? "recompose" : "recompose_with_depth_alpha";
					RecomposeVideoTexture(deviceContext, ti->texture, videoTexture, technique);
				}
				else
				{
					const char* technique = clientPipeline.videoConfig.use_alpha_layer_decoding ? "recompose_perspective" : "recompose_perspective_with_depth_alpha";
					RecomposeVideoTexture(deviceContext, ti->texture, videoTexture, technique);
				}
			}
		}
		RecomposeCubemap(deviceContext, ti->texture, diffuseCubemapTexture, diffuseCubemapTexture->mips, int2(lastSetupCommand.clientDynamicLighting.diffusePos[0], lastSetupCommand.clientDynamicLighting.diffusePos[1]));
		RecomposeCubemap(deviceContext, ti->texture, specularCubemapTexture, specularCubemapTexture->mips, int2(lastSetupCommand.clientDynamicLighting.specularPos[0], lastSetupCommand.clientDynamicLighting.specularPos[1]));
	}

	// Draw the background. If unconnected, we show a grid and horizon.
	// If connected, we show the server's chosen background: video, texture or colour.
	{
		if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
		{
			crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
			stereoCameraConstants.leftInvWorldViewProj = mgdc.viewStructs[0].invViewProj;
			stereoCameraConstants.rightInvWorldViewProj = mgdc.viewStructs[1].invViewProj;
			stereoCameraConstants.stereoViewPosition = mgdc.viewStruct.cam_pos;
			cubemapClearEffect->SetConstantBuffer(mgdc, &stereoCameraConstants);
		}
		//else
		{
			cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
			cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
			cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
		}
		if (sessionClient->IsConnected())
		{
			if (lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::COLOUR)
			{
				renderPlatform->Clear(deviceContext, ConvertVec4<vec4>(lastSetupCommand.backgroundColour));
			}
			else if (lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::VIDEO)
			{
				if (videoTexture->IsCubemap())
				{
					RenderVideoTexture(deviceContext, ti->texture, videoTexture, "use_cubemap", "cubemapTexture");
				}
				else
				{
					math::Matrix4x4 projInv;
					deviceContext.viewStruct.proj.Inverse(projInv);
					RenderVideoTexture(deviceContext, ti->texture, videoTexture, "use_perspective", "perspectiveTexture");
				}
			}
		}
		else
		{
			std::string passName = (int)config.options.lobbyView ? "neon" : "white";
			if (deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr)
				passName += "_multiview";

			cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());
			renderPlatform->DrawQuad(deviceContext);
			cubemapClearEffect->Unapply(deviceContext);
		}
	}

	vec4 white={1.f,1.f,1.f,1.f};
	pbrConstants.drawDistance = lastSetupCommand.draw_distance;
	if(specularCubemapTexture)
		pbrConstants.roughestMip=float(specularCubemapTexture->mips-1);
	if(lastSetupCommand.clientDynamicLighting.specularCubemapTexture!=0)
	{
		auto t = geometryCache.mTextureManager.Get(lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
		if(t&&t->GetSimulTexture())
		{
			pbrConstants.roughestMip=float(t->GetSimulTexture()->mips-1);
		}
	}
	if (sessionClient->IsConnected()||config.options.showGeometryOffline)
		RenderLocalNodes(deviceContext,server_uid,geometryCache);

	SIMUL_COMBINED_PROFILE_END(deviceContext);
	// Init the viewstruct in local space - i.e. with no server offsets.
	SetRenderPose(deviceContext,clientDeviceState->headPose.localPose);
	
	gui.Render(deviceContext);
	{
		crossplatform::MultiviewGraphicsDeviceContext* mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();

		const std::map<avs::uid,teleport::client::NodePoseState> &nodePoseStates
			=openXR->GetNodePoseStates(0,renderPlatform->GetFrameNumber());
		auto l=nodePoseStates.find(1);
		std::vector<vec4> hand_pos_press;
		if(l!=nodePoseStates.end())
		{
			avs::Pose handPose	= l->second.pose_footSpace.pose;
			avs::vec3 pos		= LocalToGlobal(handPose,*((avs::vec3*)&index_finger_offset));
			renderPlatform->PrintAt3dPos(mvgdc ? *mvgdc :  deviceContext, (const float*)&pos, "L", (const float*)&white);
			vec4 pos4;
			pos4.xyz			= (const float*)&pos;
			pos4.w				= 0.0f;
			hand_pos_press.push_back(pos4);
		}
		auto r=nodePoseStates.find(2);
		if(r!=nodePoseStates.end())
		{
			avs::Pose rightHand = r->second.pose_footSpace.pose;
			avs::vec3 pos = LocalToGlobal(rightHand,*((avs::vec3*)&index_finger_offset));
			renderPlatform->PrintAt3dPos(mvgdc ? *mvgdc : deviceContext, (const float*)&pos, "R", (const float*)&white);
			vec4 pos4;
			pos4.xyz = (const float*)&pos;
			pos4.w = 0.0f;
			hand_pos_press.push_back(pos4);
		}
		static bool override_have_vr_device=false;
		gui.Update(hand_pos_press, have_vr_device|override_have_vr_device);
	}
	if (!sessionClient->IsConnected()|| gui.HasFocus()||config.options.showGeometryOffline)
	{	
		pbrConstants.drawDistance = 1000.0f;
		RenderLocalNodes(deviceContext, 0,localGeometryCache);
	}
	// We must deactivate the depth buffer here, in order to use it as a texture:
	//hdrFramebuffer->DeactivateDepth(deviceContext);
	if (show_video)
	{
		int W = hdrFramebuffer->GetWidth();
		int H = hdrFramebuffer->GetHeight();
		renderPlatform->DrawTexture(deviceContext, 0, 0, W, H, ti->texture);
	}
	
	//if(show_textures)
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto& textures = geometryCache.mTextureManager.GetCache(cacheLock);
		static int tw = 128;
		int x = 0, y = 0;//hdrFramebuffer->GetHeight()-tw*2;
	/*	for (auto t : textures)
		{
			pc_client::PC_Texture* pct = static_cast<pc_client::PC_Texture*>(&(*t.second.resource));
			renderPlatform->DrawTexture(deviceContext, x, y, tw, tw, pct->GetSimulTexture());
			x += tw;
			if (x > hdrFramebuffer->GetWidth() - tw)
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
		*/
	}
	//hdrFramebuffer->Deactivate(deviceContext);
	//hDRRenderer->Render(deviceContext,hdrFramebuffer->GetTexture(),1.0f,gamma);

}

void Renderer::ChangePass(ShaderMode newShaderMode)
{
	shaderMode=newShaderMode;
	switch(newShaderMode)
	{
		case ShaderMode::PBR:
			overridePassName = "";
			break;
		case ShaderMode::ALBEDO:
			overridePassName = "albedo_only";
			break;
		case ShaderMode::NORMAL_UNSWIZZLED:
			overridePassName = "normal_unswizzled";
			break;
		case ShaderMode::DEBUG_ANIM:
			overridePassName = "debug_anim";
			break;
		case ShaderMode::LIGHTMAPS:
			overridePassName = "debug_lightmaps";
			break;
		case ShaderMode::NORMAL_VERTEXNORMALS:
			overridePassName = "normal_vertexnormals";
		default:
			overridePassName = "";
			break;
	}
}
void Renderer::Update(double timestamp_ms)
{
	double timeElapsed_s = (timestamp_ms - previousTimestamp) / 1000.0f;//ms to seconds

	teleport::client::ServerTimestamp::tick(timeElapsed_s);

	geometryCache.Update(static_cast<float>(timeElapsed_s));
	resourceCreator.Update(static_cast<float>(timeElapsed_s));

	localGeometryCache.Update(static_cast<float>(timeElapsed_s));
	localResourceCreator.Update(static_cast<float>(timeElapsed_s));

	previousTimestamp = timestamp_ms;
}


void Renderer::OnReceiveVideoTagData(const uint8_t* data, size_t dataSize)
{
	clientrender::SceneCaptureCubeTagData tagData;
	memcpy(&tagData.coreData, data, sizeof(clientrender::SceneCaptureCubeCoreTagData));
	avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, tagData.coreData.cameraTransform);

	tagData.lights.resize(tagData.coreData.lightCount);

	teleport::client::ServerTimestamp::setLastReceivedTimestampUTCUnixMs(tagData.coreData.timestamp_unix_ms);

	// We will check the received light tags agains the current list of lights - rough and temporary.
	/*
	Roderick: we will here ignore the cached lights (CPU-streamed node lights) as they are unordered so may be found in a different order
		to the tag lights. ALL light data will go into the tags, using uid lookup to get any needed data from the unordered cache.
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto &cachedLights=geometryCache.mLightManager.GetCache(cacheLock);
	auto &cachedLight=cachedLights.begin();*/
	////

	size_t index = sizeof(clientrender::SceneCaptureCubeCoreTagData);
	for (auto& light : tagData.lights)
	{
		memcpy(&light, &data[index], sizeof(clientrender::LightTagData));
		//avs::ConvertTransform(lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, light.worldTransform);
		index += sizeof(clientrender::LightTagData);
	}
	if(tagData.coreData.id>= videoTagDataCubeArray.size())
	{
		TELEPORT_CERR_BREAK("Bad tag id",1);
		return;
	}
	videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
}

void Renderer::UpdateTagDataBuffers(crossplatform::GraphicsDeviceContext& deviceContext)
{				
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto &cachedLights=geometryCache.mLightManager.GetCache(cacheLock);
	for (int i = 0; i < videoTagDataCubeArray.size(); ++i)
	{
		const auto& td = videoTagDataCubeArray[i];
		const auto& pos = td.coreData.cameraTransform.position;
		const auto& rot = td.coreData.cameraTransform.rotation;

		videoTagDataCube[i].cameraPosition = { pos.x, pos.y, pos.z };
		videoTagDataCube[i].cameraRotation = { rot.x, rot.y, rot.z, rot.w };
		videoTagDataCube[i].diffuseAmbientScale=td.coreData.diffuseAmbientScale;
		videoTagDataCube[i].lightCount = static_cast<int>(td.lights.size());
		if(td.lights.size() > 10)
		{
			TELEPORT_CERR_BREAK("Too many lights in tag.",10);
		}
		for(int j=0;j<td.lights.size()&&j<10;j++)
		{
			LightTag &t=videoTagDataCube[i].lightTags[j];
			const clientrender::LightTagData &l=td.lights[j];
			t.uid32=(unsigned)(((uint64_t)0xFFFFFFFF)&l.uid);
			t.colour=ConvertVec4<vec4>(l.color);
			// Convert from +-1 to [0,1]
			t.shadowTexCoordOffset.x=float(l.texturePosition[0])/float(lastSetupCommand.video_config.video_width);
			t.shadowTexCoordOffset.y=float(l.texturePosition[1])/float(lastSetupCommand.video_config.video_height);
			t.shadowTexCoordScale.x=float(l.textureSize)/float(lastSetupCommand.video_config.video_width);
			t.shadowTexCoordScale.y=float(l.textureSize)/float(lastSetupCommand.video_config.video_height);
			// Tag data has been properly transformed in advance:
			avs::vec3 position		=l.position;
			avs::vec4 orientation	=l.orientation;
			t.position=*((vec3*)&position);
			crossplatform::Quaternionf q((const float*)&orientation);
			t.direction=q*vec3(0,0,1.0f);
			t.worldToShadowMatrix	=ConvertMat4(l.worldToShadowMatrix);

			auto nodeLight=cachedLights.find(l.uid);
			if(nodeLight!=cachedLights.end()&& nodeLight->second.resource!=nullptr)
			{
				const clientrender::Light::LightCreateInfo &lc=nodeLight->second.resource->GetLightCreateInfo();
				t.is_point=float(lc.type!=clientrender::Light::Type::DIRECTIONAL);
				t.is_spot=float(lc.type==clientrender::Light::Type::SPOT);
				t.radius=lc.lightRadius;
				t.range=lc.lightRange;
				t.shadow_strength=0.0f;
			}
		}
	}	
	tagDataCubeBuffer.SetData(deviceContext, videoTagDataCube);
}

void Renderer::RecomposeVideoTexture(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, const char* technique)
{
	int W = targetTexture->width;
	int H = targetTexture->length;
	cubemapConstants.sourceOffset = { 0, 0 };
	cubemapConstants.targetSize.x = W;
	cubemapConstants.targetSize.y = H;

#if 0
	static crossplatform::Texture *testSourceTexture=nullptr;
	bool faceColour = true;
	if(!testSourceTexture)
	{
		static uint32_t whiteABGR = 0xFFFFFFFF;
		static uint32_t blueABGR = 0xFFFF7F7F;
		static uint32_t combinedABGR = 0xFFFFFFFF;
		static uint32_t blackABGR = 0x0;
		static uint32_t greenABGR = 0xFF337733;
		static uint32_t redABGR = 0xFF3333FF;
		static uint32_t testABGR []={redABGR,greenABGR,blueABGR,whiteABGR};
		crossplatform::TextureCreate textureCreate;
		textureCreate.w=textureCreate.l=2;
		textureCreate.initialData=testABGR;
		textureCreate.f=crossplatform::PixelFormat::RGBA_8_UNORM;
		testSourceTexture=renderPlatform->CreateTexture("testsourceTexture");
		testSourceTexture->EnsureTexture(renderPlatform,&textureCreate);
	}
	{
		cubemapClearEffect->SetTexture(deviceContext, "plainTexture",testSourceTexture);
		cubemapClearEffect->SetUnorderedAccessView(deviceContext, RWTextureTargetArray, targetTexture);
		cubemapClearEffect->Apply(deviceContext, technique, faceColour ? "test_face_colour" : "test");
		int zGroups = videoTexture->IsCubemap() ? 6 : 1;
		renderPlatform->DispatchCompute(deviceContext, targetTexture->width/16, targetTexture->length/16, zGroups);
		cubemapClearEffect->Unapply(deviceContext);
	}
#endif
	
	cubemapClearEffect->SetTexture(deviceContext, "plainTexture",srcTexture);
	cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
	cubemapClearEffect->SetConstantBuffer(deviceContext, &cameraConstants);
	cubemapClearEffect->SetUnorderedAccessView(deviceContext, RWTextureTargetArray, targetTexture);
	tagDataIDBuffer.Apply(deviceContext, cubemapClearEffect, cubemapClearEffect_TagDataIDBuffer);
	int zGroups = videoTexture->IsCubemap() ? 6 : 1;
	cubemapClearEffect->Apply(deviceContext, technique, 0);
	renderPlatform->DispatchCompute(deviceContext, W / 16, H / 16, zGroups);
	cubemapClearEffect->Unapply(deviceContext);
	cubemapClearEffect->SetUnorderedAccessView(deviceContext, RWTextureTargetArray, nullptr);
	cubemapClearEffect->UnbindTextures(deviceContext);
}

void Renderer::RenderVideoTexture(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture)
{
	bool multiview = deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr;

	tagDataCubeBuffer.Apply(deviceContext, cubemapClearEffect,cubemapClearEffect_TagDataCubeBuffer);
	cubemapConstants.depthOffsetScale = vec4(0, 0, 0, 0);
	cubemapConstants.offsetFromVideo = *((vec3*)&clientDeviceState->headPose.globalPose.position) - videoPos;
	cubemapConstants.cameraPosition = *((vec3*)&clientDeviceState->headPose.globalPose.position);
	cubemapConstants.cameraRotation = *((vec4*)&clientDeviceState->headPose.globalPose.orientation);
	cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
	cubemapClearEffect->SetTexture(deviceContext, shaderTexture, targetTexture);
	cubemapClearEffect->SetTexture(deviceContext, "plainTexture", srcTexture);
	cubemapClearEffect->Apply(deviceContext, technique, multiview ? "multiview" : "singleview");
	renderPlatform->DrawQuad(deviceContext);
	cubemapClearEffect->Unapply(deviceContext);
	cubemapClearEffect->UnbindTextures(deviceContext);
}

void Renderer::RecomposeCubemap(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, int mips, int2 sourceOffset)
{
	cubemapConstants.sourceOffset = sourceOffset;
	cubemapClearEffect->SetTexture(deviceContext, plainTexture, srcTexture);

	cubemapConstants.targetSize.x = targetTexture->width;
	cubemapConstants.targetSize.y = targetTexture->length;

	for (int m = 0; m < mips; m++)
	{
		cubemapClearEffect->SetUnorderedAccessView(deviceContext, RWTextureTargetArray, targetTexture, -1, m);
		cubemapClearEffect->SetConstantBuffer(deviceContext, &cubemapConstants);
		cubemapClearEffect->Apply(deviceContext, "recompose", 0);
		renderPlatform->DispatchCompute(deviceContext, targetTexture->width / 16, targetTexture->width / 16, 6);
		cubemapClearEffect->Unapply(deviceContext);
		cubemapConstants.sourceOffset.x += 3 * cubemapConstants.targetSize.x;
		cubemapConstants.targetSize /= 2;
	}
	cubemapClearEffect->SetUnorderedAccessView(deviceContext, RWTextureTargetArray, nullptr);
	cubemapClearEffect->UnbindTextures(deviceContext);
}


void Renderer::RenderLocalNodes(crossplatform::GraphicsDeviceContext& deviceContext, avs::uid this_server_uid, clientrender::GeometryCache& g)
{

	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
		mgdc.viewStructs[0].Init();
		mgdc.viewStructs[1].Init();
		stereoCameraConstants.leftInvWorldViewProj = mgdc.viewStructs[0].invViewProj;
		stereoCameraConstants.leftView = mgdc.viewStructs[0].view;
		stereoCameraConstants.leftProj = mgdc.viewStructs[0].proj;
		stereoCameraConstants.leftViewProj = mgdc.viewStructs[0].viewProj;
		stereoCameraConstants.rightInvWorldViewProj = mgdc.viewStructs[1].invViewProj;
		stereoCameraConstants.rightView = mgdc.viewStructs[1].view;
		stereoCameraConstants.rightProj = mgdc.viewStructs[1].proj;
		stereoCameraConstants.rightViewProj = mgdc.viewStructs[1].viewProj;
		// The following block renders to the hdrFramebuffer's rendertarget:
		stereoCameraConstants.stereoViewPosition = ((const float*)&clientDeviceState->headPose.globalPose.position);
	}
	//else
	{
		deviceContext.viewStruct.Init();
		cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
		cameraConstants.view = deviceContext.viewStruct.view;
		cameraConstants.proj = deviceContext.viewStruct.proj;
		cameraConstants.viewProj = deviceContext.viewStruct.viewProj;
		// The following block renders to the hdrFramebuffer's rendertarget:
		cameraConstants.viewPosition = ((const float*)&clientDeviceState->headPose.globalPose.position);
	}

	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto &cachedLights=g.mLightManager.GetCache(cacheLock);
		if(cachedLights.size()>lightsBuffer.count)
		{
			lightsBuffer.InvalidateDeviceObjects();
			lightsBuffer.RestoreDeviceObjects(renderPlatform, static_cast<int>(cachedLights.size()));
		}
		pbrConstants.lightCount = static_cast<int>(cachedLights.size());
	}
	// Now, any nodes bound to OpenXR poses will be updated. This may include hand objects, for example.
	if(openXR)
	{
		avs::uid root_node_uid=openXR->GetRootNode(this_server_uid);
		if(root_node_uid!=0)
		{
			std::shared_ptr<clientrender::Node> node=g.mNodeManager->GetNode(root_node_uid);
			if(node)
			{
				auto pose=sessionClient->GetOriginPose();
				node->SetLocalPosition(pose.position);
				node->SetLocalRotation(pose.orientation);
			}
		}
	// The node pose states are in the space whose origin is the VR device's playspace origin.
		const auto &nodePoseStates=openXR->GetNodePoseStates(this_server_uid,renderPlatform->GetFrameNumber());
		for(auto &n:nodePoseStates)
		{
			// TODO, we set LOCAL node pose from GLOBAL worldspace because we ASSUME no parent for these nodes.
			//clientDeviceState->SetLocalNodePose(n.first,n.second.pose_worldSpace);
			//auto &globalPose=clientDeviceState->GetGlobalNodePose(n.first);
			std::shared_ptr<clientrender::Node> node=g.mNodeManager->GetNode(n.first);
			if(node)
			{
			// TODO: Should be done as local child of an origin node, not setting local pos = globalPose.pos
				node->SetLocalPosition(n.second.pose_footSpace.pose.position);
				node->SetLocalRotation(n.second.pose_footSpace.pose.orientation);
				node->SetLocalVelocity(*((vec3*)&n.second.pose_footSpace.velocity));
				// force update of model matrices - should not be necessary, but is.
				node->UpdateModelMatrix();
			}
		}
	}
	const clientrender::NodeManager::nodeList_t& nodeList = g.mNodeManager->GetRootNodes();
	for(const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		if(show_only!=0&&show_only!=node->id)
			continue;
		RenderNode(deviceContext, node,g);
	}
	if(show_node_overlays)
	for (const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		RenderNodeOverlay(deviceContext, node,g);
	}
}

void Renderer::RenderNode(crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force)
{
	clientrender::AVSTextureHandle th = avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
	/*avs::uid node_select=gui.GetSelectedUid();
	if(!force&&(node_select > 0 && node_select != node->id))
		return;*/
	std::shared_ptr<clientrender::Texture> globalIlluminationTexture ;
	if(node->GetGlobalIlluminationTextureUid() )
		globalIlluminationTexture = g.mTextureManager.Get(node->GetGlobalIlluminationTextureUid());

	std::string passName = "pbr_nolightmap"; //Pass used for rendering geometry.
	if(node->IsStatic())
		passName="pbr_lightmap";
	if(overridePassName.length()>0)
		passName= overridePassName;
	bool force_highlight = force||(gui.GetSelectedUid() == node->id);
	//Only render visible nodes, but still render children that are close enough.
	if(node->GetPriority()>=0)
	if(node->IsVisible()&&(show_only == 0 || show_only == node->id))
	{
		const std::shared_ptr<clientrender::Mesh> mesh = node->GetMesh();
		if(mesh)
		{
			const auto& meshInfo = mesh->GetMeshCreateInfo();
			static int mat_select=-1;
			for(size_t element = 0; element < node->GetMaterials().size() && element < meshInfo.ib.size(); element++)
			{
				if(mat_select >= 0 && mat_select != element)
					continue;
				bool double_sided=false;
				auto* vb = meshInfo.vb[element].get();
				const auto* ib = meshInfo.ib[element].get();

				const crossplatform::Buffer* const v[] = {vb->GetSimulVertexBuffer()};
				crossplatform::Layout* layout = vb->GetLayout();

				mat4 model;
				const mat4& globalTransformMatrix = node->GetGlobalTransform().GetTransformMatrix();
				model = reinterpret_cast<const float*>(&globalTransformMatrix);
				static bool override_model=false;
				if(override_model)
				{
					model=mat4::identity();
				}

				if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
				{
					crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
					mat4::mul(stereoCameraConstants.leftWorldViewProj, *((mat4*)&mgdc.viewStructs[0].viewProj), model);
					stereoCameraConstants.leftWorld = model;
					mat4::mul(stereoCameraConstants.rightWorldViewProj, *((mat4*)&mgdc.viewStructs[1].viewProj), model);
					stereoCameraConstants.rightWorld = model;
				}
				//else
				{
					mat4::mul(cameraConstants.worldViewProj, *((mat4*)&deviceContext.viewStruct.viewProj), model);
					cameraConstants.world = model;
				}
				// TODO: Improve this.
				bool negative_scale=(node->GetGlobalScale().x<0.0f);
				std::shared_ptr<clientrender::Texture> gi = globalIlluminationTexture;
				std::shared_ptr<clientrender::Material> material = node->GetMaterials()[element];
				std::string usedPassName = passName;
				if(material->GetMaterialCreateInfo().shader.length())
				{
					usedPassName=material->GetMaterialCreateInfo().shader;
					double_sided=true;
				}
				std::shared_ptr<clientrender::SkinInstance> skinInstance = node->GetSkinInstance();
				if (skinInstance)
				{
					mat4* scr_matrices = skinInstance->GetBoneMatrices(globalTransformMatrix);
					memcpy(&boneMatrices.boneMatrices, scr_matrices, sizeof(mat4) * clientrender::Skin::MAX_BONES);

					pbrEffect->SetConstantBuffer(deviceContext, &boneMatrices);
					usedPassName = "anim_" + usedPassName;
				//	force_highlight=true;
				}

				crossplatform::MultiviewGraphicsDeviceContext* mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
				bool highlight=node->IsHighlighted()||force_highlight;
				crossplatform::EffectTechnique* pbrEffectTechnique = mvgdc ? pbrEffect_solid_multiviewTechnique : pbrEffect_solidTechnique;
				crossplatform::EffectPass *pass = pbrEffectTechnique->GetPass(usedPassName.c_str());
				if(material)
				{
					highlight|= (gui.GetSelectedUid() == material->id);
					const clientrender::Material::MaterialCreateInfo& matInfo = material->GetMaterialCreateInfo();
					const clientrender::Material::MaterialData& md = material->GetMaterialData();
					memcpy(&pbrConstants.diffuseOutputScalar, &md, sizeof(md));
					pbrConstants.lightmapScaleOffset=*(const vec4*)(&(node->GetLightmapScaleOffset()));
					std::shared_ptr<clientrender::Texture> diffuse	= matInfo.diffuse.texture;
					std::shared_ptr<clientrender::Texture> normal	= matInfo.normal.texture;
					std::shared_ptr<clientrender::Texture> combined = matInfo.combined.texture;
					std::shared_ptr<clientrender::Texture> emissive = matInfo.emissive.texture;
					
					pbrEffect->SetTexture(deviceContext, pbrEffect_diffuseTexture	, diffuse ? diffuse->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_normalTexture	, normal ? normal->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_combinedTexture	, combined ? combined->GetSimulTexture() : nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_emissiveTexture	, emissive ? emissive->GetSimulTexture() : nullptr);

				}
				else
				{
					pbrConstants.diffuseOutputScalar=vec4(1.0f,1.0f,1.0f,0.5f);
					pbrEffect->SetTexture(deviceContext, pbrEffect_diffuseTexture,  nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_normalTexture,  nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_combinedTexture,  nullptr);
					pbrEffect->SetTexture(deviceContext, pbrEffect_emissiveTexture,  nullptr);
					pass = mvgdc ? pbrEffect_solid_multiviewTechnique_localPass : pbrEffect_solidTechnique_localPass;
				}
				if (highlight)
				{
					pbrConstants.emissiveOutputScalar += vec4(0.2f, 0.2f, 0.2f, 0.f);
				}
				pbrEffect->SetTexture(deviceContext,pbrEffect_globalIlluminationTexture, gi ? gi->GetSimulTexture() : nullptr);

				pbrEffect->SetTexture(deviceContext,pbrEffect_diffuseCubemap, diffuseCubemapTexture);
				// If lighting is via static textures.
				if(lastSetupCommand.backgroundMode!=teleport::core::BackgroundMode::VIDEO&&lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture!=0)
				{
					auto t = g.mTextureManager.Get(lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture);
					if(t)
					{
						pbrEffect->SetTexture(deviceContext,pbrEffect_diffuseCubemap,t->GetSimulTexture());
					}
				}
				pbrEffect->SetTexture(deviceContext, pbrEffect_specularCubemap, specularCubemapTexture);
				if(lastSetupCommand.backgroundMode!=teleport::core::BackgroundMode::VIDEO&&lastSetupCommand.clientDynamicLighting.specularCubemapTexture!=0)
				{
					auto t = g.mTextureManager.Get(lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
					if(t)
					{
						pbrEffect->SetTexture(deviceContext,pbrEffect_specularCubemap,t->GetSimulTexture());
				}
				}
				//pbrEffect->SetTexture(deviceContext, "lightingCubemap", lightingCubemapTexture);
				//pbrEffect->SetTexture(deviceContext, "videoTexture", ti->texture);
				//pbrEffect->SetTexture(deviceContext, "lightingCubemap", lightingCubemapTexture);
				
				lightsBuffer.Apply(deviceContext, pbrEffect, _lights );
				tagDataCubeBuffer.Apply(deviceContext, pbrEffect, cubemapClearEffect_TagDataCubeBuffer);
				tagDataIDBuffer.Apply(deviceContext, pbrEffect, pbrEffect_TagDataIDBuffer);

				pbrEffect->SetConstantBuffer(deviceContext, &pbrConstants);
				if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
					pbrEffect->SetConstantBuffer(deviceContext, &stereoCameraConstants);
				//else
					pbrEffect->SetConstantBuffer(deviceContext, &cameraConstants);
				if(double_sided)
					renderPlatform->SetStandardRenderState(deviceContext,crossplatform::StandardRenderState::STANDARD_DOUBLE_SIDED);
				else if(negative_scale)
					renderPlatform->SetStandardRenderState(deviceContext,crossplatform::StandardRenderState::STANDARD_FRONTFACE_CLOCKWISE);
				else
					renderPlatform->SetStandardRenderState(deviceContext,crossplatform::StandardRenderState::STANDARD_FRONTFACE_COUNTERCLOCKWISE);
				renderPlatform->SetLayout(deviceContext, layout);
				renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);
				renderPlatform->SetVertexBuffers(deviceContext, 0, 1, v, layout);
				renderPlatform->SetIndexBuffer(deviceContext, ib->GetSimulIndexBuffer());
				renderPlatform->ApplyPass(deviceContext, pass);
				renderPlatform->DrawIndexed(deviceContext, (int)ib->GetIndexBufferCreateInfo().indexCount, 0, 0);
				pbrEffect->UnbindTextures(deviceContext);
				renderPlatform->UnapplyPass(deviceContext);
				layout->Unapply(deviceContext);
			}
		}
	}

	for(std::weak_ptr<clientrender::Node> childPtr : node->GetChildren())
	{
		std::shared_ptr<clientrender::Node> child = childPtr.lock();
		if(child)
		{
			RenderNode(deviceContext, child,g,false);
		}
	}
}


void Renderer::RenderNodeOverlay(crossplatform::GraphicsDeviceContext& deviceContext, const std::shared_ptr<clientrender::Node>& node,clientrender::GeometryCache &g,bool force)
{
	clientrender::AVSTextureHandle th = avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
	avs::uid node_select=gui.GetSelectedUid();

	std::shared_ptr<clientrender::Texture> globalIlluminationTexture;
	if (node->GetGlobalIlluminationTextureUid())
		globalIlluminationTexture = g.mTextureManager.Get(node->GetGlobalIlluminationTextureUid());

	//Only render visible nodes, but still render children that are close enough.
	if (node->IsVisible()&& (node_select == 0 || node_select == node->id))
	{
		const std::shared_ptr<clientrender::Mesh> mesh = node->GetMesh();
		const clientrender::AnimationComponent& anim = node->animationComponent;
		avs::vec3 pos = node->GetGlobalPosition();
		mat4 m=node->GetGlobalTransform().GetTransformMatrix();
		renderPlatform->DrawAxes(deviceContext,m,0.1f);
		const auto &nodePoses=openXR->GetNodePoses(server_uid,renderPlatform->GetFrameNumber());
		auto j=nodePoses.find(node->id);
		if(j!=nodePoses.end())
		{
			//const avs::PoseDynamic &poseDynamic=j->second;
			//poseDynamic.velocity;
		//renderPlatform->DrawLine(deviceContext,start,end);
		}
		vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
		if (node->GetSkinInstance().get())
		{
			std::string str;
			const clientrender::AnimationState* animationState = node->animationComponent.GetCurrentAnimationState();
			if (animationState)
			{
				//const clientrender::AnimationStateMap &animationStates= node->animationComponent.GetAnimationStates();
				static char txt[250];
				//for(const auto &s:animationStates)
				{
					const auto& a = animationState->getAnimation();
					if (a.get())
					{
						str +=fmt::format( "{0} {1} {2}\n", node->id, a->name.c_str(), node->animationComponent.GetCurrentAnimationTimeSeconds());
						
					}
				}
				renderPlatform->PrintAt3dPos(deviceContext, (const float*)(&pos), str.c_str(), (const float*)(&white));
			}
		}
		else if (mesh)
		{
			std::string str=fmt::format("{0} {1}: {2}", node->id,node->name.c_str(), mesh->GetMeshCreateInfo().name.c_str());
			renderPlatform->PrintAt3dPos(deviceContext, (const float*)(&pos), str.c_str(), (const float*)(&white), nullptr, 0, 0, false);
		}
		else
		{
			vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f); 
			std::string str=fmt::format("{0} {1}", node->id, node->name.c_str());
			renderPlatform->PrintAt3dPos(deviceContext, (const float*)(&pos), str.c_str(), (const float*)(&yellow), nullptr, 0, 0, false);
		}
	}

	for (std::weak_ptr<clientrender::Node> childPtr : node->GetChildren())
	{
		std::shared_ptr<clientrender::Node> child = childPtr.lock();
		if (child)
		{
			RenderNodeOverlay(deviceContext, child,g,true);
		}
	}
}

bool Renderer::OnDeviceRemoved()
{
	InvalidateDeviceObjects();
	return true;
}

void Renderer::OnFrameMove(double fTime,float time_step,bool have_headset)
{
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
		clientDeviceState->SetHeadPose_StageSpace(*((avs::vec3*)&cam_pos), *((clientrender::quat*)&q_rel));
		const teleport::core::Input& inputs = openXR->GetServerInputs(local_server_uid,renderPlatform->GetFrameNumber());
		clientDeviceState->SetInputs( inputs);

	}
	if (openXR)
	{
		const teleport::core::Input& local_inputs=openXR->GetServerInputs(local_server_uid,renderPlatform->GetFrameNumber());
		HandleLocalInputs(local_inputs);
		have_vr_device=openXR->HaveXRDevice();
		if (have_headset)
		{
			const avs::Pose &headPose_stageSpace=openXR->GetHeadPose_StageSpace();
			clientDeviceState->SetHeadPose_StageSpace(headPose_stageSpace.position, headPose_stageSpace.orientation);
		}
	}
	// Handle networked session.
	if (sessionClient->IsConnected())
	{
		//vec3 forward=-camera.Orientation.Tz();
		//vec3 right=camera.Orientation.Tx();
		//*((vec3*)&clientDeviceState->originPose.position)+=clientspace_input.y*time_step*forward;
		//*((vec3*)&clientDeviceState->originPose.position)+=clientspace_input.x*time_step*right;
		// std::cout << forward.x << " " << forward.y << " " << forward.z << "\n";
		// The camera has Z backward, X right, Y up.
		// But we want orientation relative to X right, Y forward, Z up.
		avs::DisplayInfo displayInfo = {static_cast<uint32_t>(hdrFramebuffer->GetWidth()), static_cast<uint32_t>(hdrFramebuffer->GetHeight())};
	

		const auto &nodePoses=openXR->GetNodePoses(server_uid,renderPlatform->GetFrameNumber());
		
		if (openXR)
		{
			const teleport::core::Input& inputs = openXR->GetServerInputs(server_uid,renderPlatform->GetFrameNumber());
			clientDeviceState->SetInputs(inputs);
		}
		sessionClient->Frame(displayInfo, clientDeviceState->headPose.localPose, nodePoses, receivedInitialPos, clientDeviceState->originPose,
			clientDeviceState->input, clientPipeline.decoder.idrRequired(),fTime, time_step);

		if(receivedInitialPos != sessionClient->receivedInitialPos)
		{
			clientDeviceState->originPose = sessionClient->GetOriginPose();
			receivedInitialPos = sessionClient->receivedInitialPos;
			clientDeviceState->UpdateGlobalPoses();
		}
		
		
		avs::Result result = clientPipeline.pipeline.process();
		if (result == avs::Result::Network_Disconnection 
			|| sessionClient->GetConnectionRequest() == client::SessionClient::ConnectionRequest::DISCONNECT_FROM_SERVER)
		{
			sessionClient->Disconnect(0);
			return;
		}

		static short c = 0;
		if (!(c--))
		{
			const avs::NetworkSourceCounters Counters = clientPipeline.source.getCounterValues();
			std::cout << "Network packets dropped: " << 100.0f*Counters.networkDropped << "%"
				<< "\nDecoder packets dropped: " << 100.0f*Counters.decoderDropped << "%"
				<< std::endl;
		}
	}
	else
	{
		ENetAddress remoteEndpoint; //192.168.3.42 45.132.108.84
		bool canConnect = sessionClient->GetConnectionRequest() == client::SessionClient::ConnectionRequest::CONNECT_TO_SERVER;
		if (canConnect && sessionClient->Discover("", TELEPORT_CLIENT_DISCOVERY_PORT, server_ip.c_str(), server_discovery_port, remoteEndpoint))
		{
			sessionClient->Connect(remoteEndpoint, TELEPORT_TIMEOUT);
			sessionClient->GetConnectionRequest() = client::SessionClient::ConnectionRequest::NO_CHANGE;
			gui.Hide();
		}
	}

	gui.SetConnecting(sessionClient->GetConnectionRequest() == client::SessionClient::ConnectionRequest::CONNECT_TO_SERVER);
	gui.SetConnected(sessionClient->GetWebspaceLocation() == client::SessionClient::WebspaceLocation::SERVER);
	gui.SetVideoDecoderStatus(GetVideoDecoderStatus());

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
	if (!bKeyDown)
	{
		switch (wParam)
		{
		case 'V':
			show_video = !show_video;
			break;
		case 'O':
			show_osd =(show_osd+1)%clientrender::NUM_OSDS;
			break;
		case 'C':
			render_from_video_centre = !render_from_video_centre;
			break;
		case 'U':
			show_cubemaps = !show_cubemaps;
			break;
		case 'H':
			WriteHierarchies();
			break;
		case 'N':
			show_node_overlays = !show_node_overlays;
			break;
		case 'K':
			if(sessionClient->IsConnected())
				sessionClient->Disconnect(0);
			sessionClient->GetConnectionRequest() = client::SessionClient::ConnectionRequest::NO_CHANGE;
			break;
		case 'M':
			RenderMode++;
			RenderMode = RenderMode % 2;
			break;
		case 'R':
			RecompileShaders();
			break;
		case 'Y':
			if (sessionClient->IsConnected())
				clientPipeline.decoder.toggleShowAlphaAsColor();
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
			ChangePass(clientrender::ShaderMode::NORMAL_UNSWIZZLED);
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
			#endif
		default:
			break;
		}
	}
}

void Renderer::ShowHideGui()
{
	gui.ShowHide();
	auto rightHand=localGeometryCache.mNodeManager->GetNode(local_right_hand_uid);
	auto leftHand=localGeometryCache.mNodeManager->GetNode(local_left_hand_uid);
	avs::uid point_anim_uid=localGeometryCache.mAnimationManager.GetUidByName("Point");
	rightHand->animationComponent.setAnimation(point_anim_uid);
	leftHand->animationComponent.setAnimation(point_anim_uid);
	AnimationState *leftAnimState=leftHand->animationComponent.GetAnimationState(point_anim_uid);
	AnimationState *rightAnimState=rightHand->animationComponent.GetAnimationState(point_anim_uid);
	if(gui.HasFocus())
	{
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
		openXR->MapNodeToPose(local_server_uid,1,"left/input/aim/pose");
		openXR->MapNodeToPose(local_server_uid,2,"right/input/aim/pose");
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
		openXR->MapNodeToPose(local_server_uid,1,"left/input/grip/pose");
		openXR->MapNodeToPose(local_server_uid,2,"right/input/grip/pose");
		rightHand->SetLocalTransform(palm_to_hand_r);
		leftHand->SetLocalTransform(palm_to_hand_l);
	}
	
	{
		auto rightHand=localGeometryCache.mNodeManager->GetNode(local_right_hand_uid);
		auto rSkin=localGeometryCache.mSkinManager.Get(hand_skin_uid);
		clientrender::Transform hand_to_finger=rSkin->GetBoneByName("IndexFinger4_R")->GetGlobalTransform();
		avs::vec3 v=rightHand->GetLocalTransform().LocalToGlobal(hand_to_finger.m_Translation);
		index_finger_offset=*((vec3*)&v);
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

void Renderer::WriteHierarchies()
{
	std::cout << "Node Tree\n----------------------------------\n";

	for(std::shared_ptr<clientrender::Node> node : geometryCache.mNodeManager->GetRootNodes())
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
	// In this case, we use hdrFramebuffer's depth buffer.
	return last_view_id++;
}

void Renderer::ResizeView(int view_id,int W,int H)
{
	if(hDRRenderer)
		hDRRenderer->SetBufferSize(W,H);
	if(hdrFramebuffer)
	{
		hdrFramebuffer->SetWidthAndHeight(W,H);
		hdrFramebuffer->SetAntialiasing(1);
	}
	//const float aspect = W / H;
	//cubemapConstants.localHorizFOV = HFOV * clientrender::DEG_TO_RAD;
	//cubemapConstants.localVertFOV = clientrender::GetVerticalFOVFromHorizontal(cubemapConstants.localHorizFOV, aspect);
}

bool Renderer::OnNodeEnteredBounds(avs::uid id)
{
	return geometryCache.mNodeManager->ShowNode(id);
}

bool Renderer::OnNodeLeftBounds(avs::uid id)
{
	return geometryCache.mNodeManager->HideNode(id);
}


void Renderer::UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &updateNodeStructureCommand)
{
	geometryCache.mNodeManager->ReparentNode(updateNodeStructureCommand);
}

void Renderer::UpdateNodeSubtype(const teleport::core::UpdateNodeSubtypeCommand &updateNodeStructureCommand,const std::string &regexPath)
{
	if(regexPath.size())
	{
		openXR->MapNodeToPose(server_uid,updateNodeStructureCommand.nodeID,regexPath);
	}
	else
	{
		TELEPORT_CERR << "Unrecognised node regexPath: "<<regexPath.c_str() << "!\n";
	}
}

void Renderer::SetVisibleNodes(const std::vector<avs::uid>& visibleNodes)
{
	geometryCache.mNodeManager->SetVisibleNodes(visibleNodes);
}

void Renderer::UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList)
{
	geometryCache.mNodeManager->UpdateNodeMovement(updateList);
}

void Renderer::UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList)
{
	geometryCache.mNodeManager->UpdateNodeEnabledState(updateList);
}

void Renderer::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	geometryCache.mNodeManager->SetNodeHighlighted(nodeID, isHighlighted);
}

void Renderer::UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate)
{
	geometryCache.mNodeManager->UpdateNodeAnimation(animationUpdate);
}

void Renderer::UpdateNodeAnimationControl(const teleport::core::NodeUpdateAnimationControl& animationControlUpdate)
{
	switch(animationControlUpdate.timeControl)
	{
	case teleport::core::AnimationTimeControl::ANIMATION_TIME:
		geometryCache.mNodeManager->UpdateNodeAnimationControl(animationControlUpdate.nodeID, animationControlUpdate.animationID);
		break;
	default:
		TELEPORT_CERR_BREAK("Failed to update node animation control! Time control was set to the invalid value" + std::to_string(static_cast<int>(animationControlUpdate.timeControl)) + "!", -1);
		break;
	}
}

void Renderer::SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	geometryCache.mNodeManager->SetNodeAnimationSpeed(nodeID, animationID, speed);
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

	hdrFramebuffer->Activate(deviceContext);
	hdrFramebuffer->Clear(deviceContext, 0.5f, 0.25f, 0.5f, 0.f, reverseDepth ? 0.f : 1.f);

	vec3 true_pos = camera.GetPosition();
	if (render_from_video_centre)
	{
		vec3 pos = videoPosDecoded ? videoPos : vec3(0, 0, 0);
		camera.SetPosition(pos);
	};
	float aspect = (float)viewport.w / (float)viewport.h;
	if (reverseDepth)
		deviceContext.viewStruct.proj = camera.MakeDepthReversedProjectionMatrix(aspect);
	else
		deviceContext.viewStruct.proj = camera.MakeProjectionMatrix(aspect);

	// Init the viewstruct in local space - i.e. with no server offsets.
	{
		math::SimulOrientation globalOrientation;
		// global pos/orientation:
		globalOrientation.SetPosition((const float*)&clientDeviceState->headPose.localPose.position);
		math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(-1.f, 0.0f, 0.0f));
		math::Quaternion q1 = (const float*)&clientDeviceState->headPose.localPose.orientation;
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

	vec4 white(1.f, 1.f, 1.f, 1.f);
	if (render_from_video_centre)
	{
		camera.SetPosition(true_pos);
		renderPlatform->Print(deviceContext, viewport.w - 16, viewport.h - 16, "C", white);
	}
	// We must deactivate the depth buffer here, in order to use it as a texture:
  	hdrFramebuffer->DeactivateDepth(deviceContext);
	//renderPlatform->DrawDepth(deviceContext, 0, 0, (256 * viewport.w)/ viewport.h, 256, hdrFramebuffer->GetDepthTexture());
	if (show_video)
	{
		clientrender::AVSTextureHandle th = avsTexture;
		clientrender::AVSTexture& tx = *th;
		AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
		int W = hdrFramebuffer->GetWidth();
		int H = hdrFramebuffer->GetHeight();
		if(ti)
			renderPlatform->DrawTexture(deviceContext, 0, 0, W, H, ti->texture);
	}
	static int lod = 0;
	static int tt = 1000;
	tt--;
	if (!tt)
	{
		lod++;
	}
	lod = lod % 8;
	if(show_cubemaps)
	{
		int x=50,y=50;
		static int r=100;
		renderPlatform->DrawCubemap(deviceContext, videoTexture,		  x+=r, y, r, 1.f, 1.f, 0.0f);
		renderPlatform->DrawCubemap(deviceContext, diffuseCubemapTexture, x+=r, y, r, 1.f, 1.f, static_cast<float>(lod));
		renderPlatform->DrawCubemap(deviceContext, specularCubemapTexture, x+=r,y, r, 1.f, 1.f, static_cast<float>(lod));
		renderPlatform->Print(deviceContext,x,y,fmt::format("mip {0}",lod).c_str());
		auto t = geometryCache.mTextureManager.Get(lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture);
		if(t&&t->GetSimulTexture())
		{
			renderPlatform->DrawCubemap(deviceContext, t->GetSimulTexture(), x+=r, y, r, 1.f, 1.f, static_cast<float>(lod));
		}
		auto s = geometryCache.mTextureManager.Get(lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
		if(s&&s->GetSimulTexture())
		{
			static int s_lod=0;
			if (!tt)
			{
				s_lod++;
				s_lod=s_lod%s->GetSimulTexture()->mips;
	}
			renderPlatform->Print(deviceContext,x,y,fmt::format("cubemaps mip {0}",s_lod).c_str());
			renderPlatform->DrawCubemap(deviceContext, s->GetSimulTexture(), x+=r, y, r, 1.f, 1.f, static_cast<float>(s_lod));
		}
	}
	if (!tt)
	{
		tt=1000;
	}
	//
	{
		static int tw = 128;
		int x = 0, y = 0;//hdrFramebuffer->GetHeight()-tw*2;
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
				if (x > hdrFramebuffer->GetWidth() - tw)
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

	hdrFramebuffer->Deactivate(deviceContext);
	hDRRenderer->Render(deviceContext, hdrFramebuffer->GetTexture(), 1.0f, gamma);
	
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

void Renderer::SetServer(const char *ip_port)
{
	std::string ip= ip_port;
	size_t pos=ip.find(":");
	if(pos>=ip.length())
	{
		server_discovery_port = TELEPORT_SERVER_DISCOVERY_PORT;
		server_ip=ip;
	}
	else
	{
		server_discovery_port =atoi(ip.substr(pos+1,ip.length()-pos-1).c_str());
		server_ip = ip.substr(0,pos);
	}
	config.StoreRecentURL(ip_port);
}

void Renderer::ConnectButtonHandler(const std::string& url)
{
	SetServer(url.c_str());
	sessionClient->GetConnectionRequest() = client::SessionClient::ConnectionRequest::CONNECT_TO_SERVER;
}

void Renderer::CancelConnectButtonHandler()
{
	sessionClient->GetConnectionRequest() = client::SessionClient::ConnectionRequest::DISCONNECT_FROM_SERVER;
}

void Renderer::RemoveView(int)
{
}

void Renderer::DrawOSD(crossplatform::GraphicsDeviceContext& deviceContext)
{
	if (!show_osd||gui.HasFocus())
		return;

	//Set up ViewStruct
	crossplatform::ViewStruct& viewStruct = deviceContext.viewStruct;
	viewStruct.proj = crossplatform::Camera::MakeDepthReversedProjectionMatrix(1.0f, 1.0f, 0.001f, 100.0f);
	math::SimulOrientation globalOrientation;
	// global pos/orientation:
	globalOrientation.SetPosition((const float*)&clientDeviceState->headPose.localPose.position);
	math::Quaternion q0(3.1415926536f / 2.0f, math::Vector3(-1.f, 0.0f, 0.0f));
	math::Quaternion q1 = (const float*)&clientDeviceState->headPose.localPose.orientation;
	auto q_rel = q1 / q0;
	globalOrientation.SetOrientation(q_rel);
	viewStruct.view = globalOrientation.GetInverseMatrix().RowPointer(0);
	viewStruct.Init();
	
	gui.setGeometryCache(&geometryCache);
	if(openXR)
	{
		avs::Pose p=openXR->GetActionPose(client::RIGHT_AIM_POSE);
		// from hand to overlay is diff:
		vec3 start		=*((vec3*)&p.position);
		vec3 normal		={0,-1.f,0};
		avs::Pose overlay_pose=openXR->ConvertGLStageSpacePoseToLocalSpacePose(openXR->overlay.pose) ;
		vec3 overlay_centre=*((vec3*)&overlay_pose.position);
		vec3 diff		=overlay_centre-start;
		float nf		=-dot(normal,diff);
		vec3 dir		=*((crossplatform::Quaternionf*)&p.orientation)*vec3(0,1.0f,0);
		float nr		=-dot(dir,normal);
		float distance	=nf/nr;
		hit		=start+distance*dir;

		vec3 h			=hit-overlay_centre;
		h.x				/=(float)openXR->overlay.size.width;
		h.z				/=(float)openXR->overlay.size.height;
		vec2 m(h.x,h.z);
		float rightTrigger=openXR->GetActionFloatState(client::RIGHT_TRIGGER);
		gui.SetDebugGuiMouse(m,rightTrigger>0.5f);
	}
	gui.BeginDebugGui(deviceContext);
	vec4 white(1.f, 1.f, 1.f, 1.f);
	vec4 text_colour={1.0f,1.0f,0.5f,1.0f};
	vec4 background={0.0f,0.0f,0.0f,0.5f};
	const avs::NetworkSourceCounters counters = clientPipeline.source.getCounterValues();
	const avs::DecoderStats vidStats = clientPipeline.decoder.GetStats();
	bool canConnect = sessionClient->GetConnectionRequest() == client::SessionClient::ConnectionRequest::CONNECT_TO_SERVER;

	deviceContext.framePrintX = 8;
	deviceContext.framePrintY = 8;
	gui.LinePrint(sessionClient->IsConnected()? platform::core::QuickFormat("Client %d connected to: %s, port %d"
		, sessionClient->GetClientID(),sessionClient->GetServerIP().c_str(),sessionClient->GetPort()):
		(canConnect?platform::core::QuickFormat("Not connected. Discovering %s port %d", server_ip.c_str(), server_discovery_port):"Offline"),white);
	gui.LinePrint(platform::core::QuickFormat("Framerate: %4.4f", framerate));

	if(show_osd== clientrender::NETWORK_OSD)
	{
		gui.LinePrint(platform::core::QuickFormat("Start timestamp: %d", clientPipeline.pipeline.GetStartTimestamp()));
		gui.LinePrint(platform::core::QuickFormat("Current timestamp: %d",clientPipeline.pipeline.GetTimestamp()));
		gui.LinePrint(platform::core::QuickFormat("Bandwidth KBs: %4.2f", counters.bandwidthKPS));
		gui.LinePrint(platform::core::QuickFormat("Network packets received: %d", counters.networkPacketsReceived));
		gui.LinePrint(platform::core::QuickFormat("Decoder packets received: %d", counters.decoderPacketsReceived));
		gui.LinePrint(platform::core::QuickFormat("Network packets dropped: %d", counters.networkPacketsDropped));
		gui.LinePrint(platform::core::QuickFormat("Decoder packets dropped: %d", counters.decoderPacketsDropped)); 
		gui.LinePrint(platform::core::QuickFormat("Decoder packets incomplete: %d", counters.incompleteDecoderPacketsReceived));
		gui.LinePrint(platform::core::QuickFormat("Decoder packets per sec: %4.2f", counters.decoderPacketsReceivedPerSec));
		gui.LinePrint(platform::core::QuickFormat("Video frames received per sec: %4.2f", vidStats.framesReceivedPerSec));
		gui.LinePrint(platform::core::QuickFormat("Video frames parseed per sec: %4.2f", vidStats.framesProcessedPerSec));
		gui.LinePrint(platform::core::QuickFormat("Video frames displayed per sec: %4.2f", vidStats.framesDisplayedPerSec));
	}
	else if(show_osd== clientrender::CAMERA_OSD)
	{
		vec3 offset=camera.GetPosition();
		gui.LinePrint(receivedInitialPos?(platform::core::QuickFormat("Origin: %4.4f %4.4f %4.4f", clientDeviceState->originPose.position.x, clientDeviceState->originPose.position.y, clientDeviceState->originPose.position.z)):"Origin:", white);
		gui.LinePrint(platform::core::QuickFormat(" Local: %4.4f %4.4f %4.4f", clientDeviceState->headPose.localPose.position.x, clientDeviceState->headPose.localPose.position.y, clientDeviceState->headPose.localPose.position.z),white);
		gui.LinePrint(platform::core::QuickFormat(" Final: %4.4f %4.4f %4.4f\n", clientDeviceState->headPose.globalPose.position.x, clientDeviceState->headPose.globalPose.position.y, clientDeviceState->headPose.globalPose.position.z),white);
		if (videoPosDecoded)
		{
			gui.LinePrint(platform::core::QuickFormat(" Video: %4.4f %4.4f %4.4f", videoPos.x, videoPos.y, videoPos.z), white);
		}	
		else
		{
			gui.LinePrint(platform::core::QuickFormat(" Video: -"), white);
		}
	}
	else if (show_osd == clientrender::VIDEO_OSD)
	{
		clientrender::AVSTextureHandle th = avsTexture;
		clientrender::AVSTexture& tx = *th;
		AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
		if(ti)
		{
			gui.LinePrint(platform::core::QuickFormat("Video Texture"), white);
			gui.DrawTexture(ti->texture);
		}
	}
	else if (show_osd == clientrender::DECODER_OSD)
	{
		gui.LinePrint("Decoder Status:", white);
		auto names = magic_enum::enum_names<avs::DecoderStatus>();
		avs::DecoderStatus status = gui.GetVideoDecoderStatus();
		if (status == avs::DecoderStatus::DecoderUnavailable)
		{
			gui.LinePrint(std::string(names[0]).c_str(), white);
		}
		else
		{
			for (size_t i = 0; i < 8; i++)
			{
				bool valid = uint32_t(status) & uint32_t(1 << i);
				std::string str = std::string(names[i + 1]) + ": %s";
				gui.LinePrint(platform::core::QuickFormat(str.c_str(), valid ? "true" : "false"), white);
			}
		}
	}
	else if (show_osd == clientrender::CUBEMAP_OSD)
	{
		gui.LinePrint(platform::core::QuickFormat("Cubemap Texture"), white);

		static crossplatform::Texture* debugCubemapTexture = nullptr;
		if (!debugCubemapTexture)
			debugCubemapTexture = renderPlatform->CreateTexture("debugCubemapTexture");
		debugCubemapTexture->ensureTexture2DSizeAndFormat(renderPlatform, 512, 512, 1, crossplatform::RGBA_8_UNORM, false, true);

		debugCubemapTexture->activateRenderTarget(deviceContext);
		renderPlatform->Clear(deviceContext, vec4(0.0f, 0.0f, 0.0f, 1.0f)); //Add in the alpha.
		renderPlatform->DrawCubemap(deviceContext, videoTexture, 0.0f, 0.0f, 1.9f, 1.0f, 1.0f, 0.0f);
		debugCubemapTexture->deactivateRenderTarget(deviceContext);

		gui.DrawTexture(debugCubemapTexture);
	}
	else if(show_osd== clientrender::GEOMETRY_OSD)
	{
		GeometryOSD(localGeometryCache);
	}
	else if(show_osd== clientrender::TAG_OSD)
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto& cachedLights = geometryCache.mLightManager.GetCache(cacheLock);
		const char *name="";
		gui.LinePrint("Tags\n");
		for(int i=0;i<videoTagDataCubeArray.size();i++)
		{
			auto &tag=videoTagDataCubeArray[i];
			gui.LinePrint(platform::core::QuickFormat("%d lights",tag.coreData.lightCount));

			auto *gpu_tag_buffer=videoTagDataCube;
			if(gpu_tag_buffer)
			for(int j=0;j<tag.lights.size();j++)
			{
				auto &l=tag.lights[j];
				auto &t=gpu_tag_buffer[j];
				const LightTag &lightTag=t.lightTags[j];
				vec4 clr={l.color.x,l.color.y,l.color.z,1.0f};
				 
				auto& c = cachedLights[l.uid];
				if (c.resource)
				{
					auto& lcr =c.resource->GetLightCreateInfo();
					name=lcr.name.c_str();
				}
				if(l.lightType==clientrender::LightType::Directional)
					gui.LinePrint(platform::core::QuickFormat("%llu: %s, Type: %s, dir: %3.3f %3.3f %3.3f clr: %3.3f %3.3f %3.3f",l.uid,name,ToString((clientrender::Light::Type)l.lightType)
						,lightTag.direction.x,lightTag.direction.y,lightTag.direction.z
						,l.color.x,l.color.y,l.color.z),clr);
				else
					gui.LinePrint(platform::core::QuickFormat("%llu: %s, Type: %s, pos: %3.3f %3.3f %3.3f clr: %3.3f %3.3f %3.3f",l.uid, name, ToString((clientrender::Light::Type)l.lightType)
						,lightTag.position.x
						,lightTag.position.y
						,lightTag.position.z
						,l.color.x,l.color.y,l.color.z),clr);

			}
		}
	}
	else if(show_osd== clientrender::CONTROLLER_OSD)
	{
		gui.LinePrint( "CONTROLS\n");
		if(openXR)
		{
			vec3 pos=gui.Get3DPos();
			gui.LinePrint(fmt::format("gui pos: {: .3f},{: .3f},{: .3f}",pos.x,pos.y,pos.z).c_str());
			gui.LinePrint(openXR->GetDebugString().c_str());
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
	}
	gui.EndDebugGui(deviceContext);

	//ImGui::PlotLines("Jitter buffer length", statJitterBuffer.data(), statJitterBuffer.count(), 0, nullptr, 0.0f, 100.0f);
	//ImGui::PlotLines("Jitter buffer push calls", statJitterPush.data(), statJitterPush.count(), 0, nullptr, 0.0f, 5.0f);
	//ImGui::PlotLines("Jitter buffer pop calls", statJitterPop.data(), statJitterPop.count(), 0, nullptr, 0.0f, 5.0f);
}

void Renderer::GeometryOSD(const clientrender::GeometryCache &geometryCache)
{
	vec4 white(1.f, 1.f, 1.f, 1.f);
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	gui.LinePrint(platform::core::QuickFormat("Nodes: %d",geometryCache.mNodeManager->GetNodeAmount()), white);

	static int nodeLimit = 5;
	auto& rootNodes = geometryCache.mNodeManager->GetRootNodes();
	static int lineLimit = 50;

	gui.LinePrint(platform::core::QuickFormat("Meshes: %d\nLights: %d", geometryCache.mMeshManager.GetCache(cacheLock).size(),
					geometryCache.mLightManager.GetCache(cacheLock).size()), white);
					
	gui.Scene();

	auto &missing=geometryCache.m_MissingResources;
	if(missing.size())
	{
		gui.LinePrint(platform::core::QuickFormat("Missing Resources"));
		for(const auto& missingPair : missing)
		{
			const clientrender::MissingResource& missingResource = missingPair.second;
			std::string txt= platform::core::QuickFormat("\t%s %d from ", stringof(missingResource.resourceType), missingResource.id);
			for(auto &u:missingResource.waitingResources)
			{
				auto type= u.get()->type;
				avs::uid id=u.get()->id;
				if(type==avs::GeometryPayloadType::Node)
				{
					txt+="Node ";
					auto n = geometryCache.mNodeManager->GetNode(id);
					if(n)
						txt += n->name;
				}
				txt+=platform::core::QuickFormat("%d, ",(uint64_t)id);
			}
			gui.LinePrint( txt.c_str());
		}
	}
	const auto &req=geometryCache.GetResourceRequests();
	gui.LinePrint(platform::core::QuickFormat("%d Requests",req.size()));
	if(req.size())
	{
		std::string lst;
		for(const auto &r:req)
		{
			lst+=fmt::format("%d ",r);
		}
		gui.LinePrint(lst.c_str());
	}
	const auto &sent_req=sessionClient->GetSentResourceRequests();
	gui.LinePrint(platform::core::QuickFormat("%d Requests Sent",sent_req.size()));
	if(sent_req.size())
	{
		std::string lst;
		for(const auto &r:sent_req)
		{
			lst+=fmt::format("{0} ",r.first);
		}
		gui.LinePrint(lst.c_str());
	}
}

std::vector<uid> Renderer::GetGeometryResources()
{
	return geometryCache.GetAllResourceIDs();
}

// This is called when we connect to a new server.
void Renderer::ClearGeometryResources()
{
	geometryCache.ClearAll();
	resourceCreator.Clear();
}

void Renderer::SetExternalTexture(crossplatform::Texture* t)
{
	externalTexture = t;
	have_vr_device = (externalTexture != nullptr);
}

void Renderer::PrintHelpText(crossplatform::GraphicsDeviceContext& deviceContext)
{
	deviceContext.framePrintY = 8;
	deviceContext.framePrintX = hdrFramebuffer->GetWidth() / 2;
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


void Renderer::OnLightingSetupChanged(const teleport::core::SetupLightingCommand &l)
{
	lastSetupLightingCommand=l;
}

void Renderer::OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions_)
{
	if (openXR)
		openXR->OnInputsSetupChanged(server_uid,inputDefinitions_);
}

avs::DecoderBackendInterface* Renderer::CreateVideoDecoder()
{
	AVSTextureHandle th = avsTexture;
	AVSTextureImpl* t = static_cast<AVSTextureImpl*>(th.get());
	return new VideoDecoderBackend(renderPlatform, t->texture);
	
}

bool Renderer::OnSetupCommandReceived(const char *server_ip,const teleport::core::SetupCommand &setupCommand,teleport::core::Handshake &handshake)
{
	ConfigureVideo(setupCommand.video_config);

	TELEPORT_CLIENT_WARN("SETUP COMMAND RECEIVED: server_streaming_port %d clr %d x %d dpth %d x %d\n", setupCommand.server_streaming_port, clientPipeline.videoConfig.video_width, clientPipeline.videoConfig.video_height
																	, clientPipeline.videoConfig.depth_width, clientPipeline.videoConfig.depth_height	);
	videoPosDecoded=false;

	videoTagDataCubeArray.clear();
	videoTagDataCubeArray.resize(maxTagDataSize);

	teleport::client::ServerTimestamp::setLastReceivedTimestampUTCUnixMs(setupCommand.startTimestamp_utc_unix_ms);
	sessionClient->SetPeerTimeout(setupCommand.idle_connection_timeout);

	const uint32_t geoStreamID = 80;
	std::vector<avs::NetworkSourceStream> streams = { { 20 }, { 40 } };
	if (AudioStream)
	{
		streams.push_back({ 60 });
	}
	if (GeoStream)
	{
		streams.push_back({ geoStreamID });
	}

	avs::NetworkSourceParams sourceParams;
	sourceParams.connectionTimeout = setupCommand.idle_connection_timeout;
	sourceParams.remoteIP = server_ip;
	sourceParams.remotePort = setupCommand.server_streaming_port;
	sourceParams.remoteHTTPPort = setupCommand.server_http_port;
	sourceParams.maxHTTPConnections = 10;
	sourceParams.httpStreamID = geoStreamID;
	sourceParams.useSSL = setupCommand.using_ssl;

	// Configure for video stream, tag data stream, audio stream and geometry stream.
	if (!clientPipeline.source.configure(std::move(streams), sourceParams))
	{
		TELEPORT_BREAK_ONCE("Failed to configure network source node\n");
		return false;
	}

	clientPipeline.source.setDebugStream(setupCommand.debug_stream);
	clientPipeline.source.setDoChecksums(setupCommand.do_checksums);
	clientPipeline.source.setDebugNetworkPackets(setupCommand.debug_network_packets);

	//test
	//avs::HTTPPayloadRequest req;
	//req.fileName = "meshes/engineering/Cube_Cube.mesh";
	//req.type = avs::FilePayloadType::Mesh;
	//clientPipeline.source.GetHTTPRequestQueue().emplace(std::move(req));

	bodyOffsetFromHead = setupCommand.bodyOffsetFromHead;
	avs::ConvertPosition(setupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, bodyOffsetFromHead);
	
	clientPipeline.decoderParams.deferDisplay = false;
	clientPipeline.decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	clientPipeline.decoderParams.codec = clientPipeline.videoConfig.videoCodec;
	clientPipeline.decoderParams.use10BitDecoding = clientPipeline.videoConfig.use_10_bit_decoding;
	clientPipeline.decoderParams.useYUV444ChromaFormat = clientPipeline.videoConfig.use_yuv_444_decoding;
	clientPipeline.decoderParams.useAlphaLayerDecoding = clientPipeline.videoConfig.use_alpha_layer_decoding;

	avs::DeviceHandle dev;
	
#if TELEPORT_CLIENT_USE_D3D12
	dev.handle = renderPlatform->AsD3D12Device();
	dev.type = avs::DeviceType::Direct3D12;
#elif TELEPORT_CLIENT_USE_D3D11
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type = avs::DeviceType::Direct3D11;
#else
	dev.handle = renderPlatform->AsVulkanDevice();
	dev.type = avs::DeviceType::Vulkan;
#endif

	clientPipeline.pipeline.reset();
	// Top of the clientPipeline.pipeline, we have the network clientPipeline.source.
	clientPipeline.pipeline.add(&clientPipeline.source);

	AVSTextureImpl* ti = (AVSTextureImpl*)(avsTexture.get());
	if (ti)
	{
		SAFE_DELETE(ti->texture);
	}
	
	/* Now for each stream, we add both a DECODER and a SURFACE node. e.g. for two streams:
					 /->decoder -> surface
			source -<
					 \->decoder	-> surface
	*/
	size_t stream_width = clientPipeline.videoConfig.video_width;
	size_t stream_height = clientPipeline.videoConfig.video_height;

	if (clientPipeline.videoConfig.use_cubemap)
	{
		if(clientPipeline.videoConfig.colour_cubemap_size)
			videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.colour_cubemap_size, clientPipeline.videoConfig.colour_cubemap_size, 1, 1,
				crossplatform::PixelFormat::RGBA_16_FLOAT, true, false, false, true);
	}
	else
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.perspective_width, clientPipeline.videoConfig.perspective_height, 1, 1,
			crossplatform::PixelFormat::RGBA_16_FLOAT, true, false, false, false);
	}
	specularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, setupCommand.clientDynamicLighting.specularCubemapSize, setupCommand.clientDynamicLighting.specularCubemapSize, 1, setupCommand.clientDynamicLighting.specularMips, crossplatform::PixelFormat::RGBA_8_UNORM, true, false, false, true);
	diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, setupCommand.clientDynamicLighting.diffuseCubemapSize, setupCommand.clientDynamicLighting.diffuseCubemapSize, 1, 1,crossplatform::PixelFormat::RGBA_8_UNORM, true, false, false, true);

	const float aspect = setupCommand.video_config.perspective_width / static_cast<float>(setupCommand.video_config.perspective_height);
	const float horzFOV = setupCommand.video_config.perspective_fov * clientrender::DEG_TO_RAD;
	const float vertFOV = clientrender::GetVerticalFOVFromHorizontal(horzFOV, aspect);

	cubemapConstants.serverProj = crossplatform::Camera::MakeDepthReversedProjectionMatrix(horzFOV, vertFOV, 0.01f, 0);

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(clientPipeline.videoConfig.video_height) / float(stream_height);

	CreateTexture(avsTexture, int(stream_width), int(stream_height));

// Set to a custom backend that uses platform api video decoder if using D3D12 and non NVidia card. 
#if TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER
	clientPipeline.decoder.setBackend(CreateVideoDecoder());
#endif

	// Video streams are 0+...
	if (!clientPipeline.decoder.configure(dev, (int)stream_width, (int)stream_height, clientPipeline.decoderParams, 20))
	{
		TELEPORT_CERR << "Failed to configure decoder node!\n";
	}
	if (!clientPipeline.surface.configure(avsTexture->createSurface()))
	{
		TELEPORT_CERR << "Failed to configure output surface node!\n";
	}

	clientPipeline.videoQueue.configure(300000, 16, "VideoQueue");

	avs::PipelineNode::link(clientPipeline.source, clientPipeline.videoQueue);
	avs::PipelineNode::link(clientPipeline.videoQueue, clientPipeline.decoder);
	clientPipeline.pipeline.link({ &clientPipeline.decoder, &clientPipeline.surface });
	
	// Tag Data
	{
		auto f = std::bind(&Renderer::OnReceiveVideoTagData, this, std::placeholders::_1, std::placeholders::_2);
		if (!clientPipeline.tagDataDecoder.configure(40, f))
		{
			TELEPORT_CERR << "Failed to configure video tag data decoder node!\n";
		}

		clientPipeline.tagDataQueue.configure(200, 16, "clientPipeline.tagDataQueue");

		avs::PipelineNode::link(clientPipeline.source, clientPipeline.tagDataQueue);
		clientPipeline.pipeline.link({ &clientPipeline.tagDataQueue, &clientPipeline.tagDataDecoder });
	}

	// Audio
	if (AudioStream)
	{
		clientPipeline.avsAudioDecoder.configure(60);
		sca::AudioSettings audioSettings;
		audioSettings.codec = sca::AudioCodec::PCM;
		audioSettings.numChannels = 1;
		audioSettings.sampleRate = 48000;
		audioSettings.bitsPerSample = 32;
		// This will be deconfigured automatically when the clientPipeline.pipeline is deconfigured.
		#ifdef _MSC_VER
		audioPlayer.configure(audioSettings);
		audioStreamTarget.reset(new sca::AudioStreamTarget(&audioPlayer));
		#endif
		clientPipeline.avsAudioTarget.configure(audioStreamTarget.get());

		clientPipeline.audioQueue.configure(4096, 120, "AudioQueue");

		avs::PipelineNode::link(clientPipeline.source, clientPipeline.audioQueue);
		avs::PipelineNode::link(clientPipeline.audioQueue, clientPipeline.avsAudioDecoder);
		clientPipeline.pipeline.link({ &clientPipeline.avsAudioDecoder, &clientPipeline.avsAudioTarget });

		// Audio Input
		if (setupCommand.audio_input_enabled)
		{
			sca::NetworkSettings networkSettings =
			{
					setupCommand.server_streaming_port + 1, server_ip, setupCommand.server_streaming_port
					, static_cast<int32_t>(handshake.maxBandwidthKpS)
					, static_cast<int32_t>(handshake.udpBufferSize)
					, setupCommand.requiredLatencyMs
					, (int32_t)setupCommand.idle_connection_timeout
			};

			audioInputNetworkPipeline.reset(new sca::NetworkPipeline());
			audioInputQueue.configure(4096, 120, "AudioInputQueue");
			audioInputNetworkPipeline->initialise(networkSettings, &audioInputQueue);

			// The callback will be called when audio input is received.
			auto f = [this](const uint8_t* data, size_t dataSize) -> void
			{
				size_t bytesWritten;
				if (audioInputQueue.write(nullptr, data, dataSize, bytesWritten))
				{
					audioInputNetworkPipeline->process();
				}
			};
		#ifdef _MSC_VER
			// The audio player will stop recording automatically when deconfigured. 
			audioPlayer.startRecording(f);
		#endif
		}
	}

	// We will add a GEOMETRY PIPE
	if(GeoStream)
	{
		clientPipeline.avsGeometryDecoder.configure(80, &geometryDecoder);
		clientPipeline.avsGeometryTarget.configure(&resourceCreator);

		clientPipeline.geometryQueue.configure(10000, 200, "clientPipeline.geometryQueue");

		avs::PipelineNode::link(clientPipeline.source, clientPipeline.geometryQueue);
		avs::PipelineNode::link(clientPipeline.geometryQueue, clientPipeline.avsGeometryDecoder);
		clientPipeline.pipeline.link({ &clientPipeline.avsGeometryDecoder, &clientPipeline.avsGeometryTarget });
	}

	handshake.startDisplayInfo.width = hdrFramebuffer->GetWidth();
	handshake.startDisplayInfo.height = hdrFramebuffer->GetHeight();
	handshake.axesStandard = avs::AxesStandard::EngineeringStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.FOV = 90.0f;
	handshake.isVR = false;
	handshake.framerate = 60;
	handshake.udpBufferSize = static_cast<uint32_t>(clientPipeline.source.getSystemBufferSize());
	handshake.maxBandwidthKpS = handshake.udpBufferSize * handshake.framerate;
	handshake.maxLightsSupported=10;
	handshake.clientStreamingPort = setupCommand.server_streaming_port + 1;
	lastSetupCommand = setupCommand;

	//java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
	return true;
}

void Renderer::OnVideoStreamClosed()
{
	TELEPORT_CLIENT_WARN("VIDEO STREAM CLOSED\n");
	clientPipeline.pipeline.deconfigure();
	clientPipeline.videoQueue.deconfigure();
	clientPipeline.audioQueue.deconfigure();
	clientPipeline.geometryQueue.deconfigure();

	//const ovrJava* java = app->GetJava();
	//java->Env->CallVoidMethod(java->ActivityObject, jni.closeVideoStreamMethod);

	receivedInitialPos = 0;
}

void Renderer::OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand& reconfigureVideoCommand)
{
	clientPipeline.videoConfig = reconfigureVideoCommand.video_config;

	TELEPORT_CLIENT_WARN("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d", clientPipeline.videoConfig.video_width, clientPipeline.videoConfig.video_height
		, clientPipeline.videoConfig.depth_width, clientPipeline.videoConfig.depth_height);

	clientPipeline.decoderParams.deferDisplay = false;
	clientPipeline.decoderParams.decodeFrequency = avs::DecodeFrequency::NALUnit;
	clientPipeline.decoderParams.codec = clientPipeline.videoConfig.videoCodec;
	clientPipeline.decoderParams.use10BitDecoding = clientPipeline.videoConfig.use_10_bit_decoding;
	clientPipeline.decoderParams.useYUV444ChromaFormat = clientPipeline.videoConfig.use_yuv_444_decoding;
	clientPipeline.decoderParams.useAlphaLayerDecoding = clientPipeline.videoConfig.use_alpha_layer_decoding;

	avs::DeviceHandle dev;
#if TELEPORT_CLIENT_USE_D3D12
	dev.handle = renderPlatform->AsD3D12Device();;
	dev.type = avs::DeviceType::Direct3D12;
#elif TELEPORT_CLIENT_USE_D3D11
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type = avs::DeviceType::Direct3D11;
#else
	dev.handle = renderPlatform->AsVulkanDevice();
	dev.type = avs::DeviceType::Vulkan;
#endif

	size_t stream_width = clientPipeline.videoConfig.video_width;
	size_t stream_height = clientPipeline.videoConfig.video_height;

	if (clientPipeline.videoConfig.use_cubemap)
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.colour_cubemap_size, clientPipeline.videoConfig.colour_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false, true);
	}
	else
	{
		videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.perspective_width, clientPipeline.videoConfig.perspective_height, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false, false);
	}

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(clientPipeline.videoConfig.video_height) / float(stream_height);

	AVSTextureImpl* ti = (AVSTextureImpl*)(avsTexture.get());
	// Only create new texture and register new surface if resolution has changed
	if (ti && ti->texture->GetWidth() != stream_width || ti->texture->GetLength() != stream_height)
	{
		SAFE_DELETE(ti->texture);

		if (!clientPipeline.decoder.unregisterSurface())
		{
			throw std::runtime_error("Failed to unregister decoder surface");
		}

		CreateTexture(avsTexture, int(stream_width), int(stream_height));
	}

	if (!clientPipeline.decoder.reconfigure((int)stream_width, (int)stream_height, clientPipeline.decoderParams))
	{
		throw std::runtime_error("Failed to reconfigure decoder");
	}
	
	lastSetupCommand.video_config = reconfigureVideoCommand.video_config;
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
				show_osd =(show_osd+1)%clientrender::NUM_OSDS;
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