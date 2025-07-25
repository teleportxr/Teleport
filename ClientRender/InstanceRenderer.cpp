#ifndef _MSC_VER
#pragma clang optimize off
#endif
#include "InstanceRenderer.h"
#include "Platform/CrossPlatform/Framebuffer.h"
#include "Renderer.h"
#include "TeleportClient/Log.h"
#include <fmt/core.h>
#if TELEPORT_CLIENT_USE_VULKAN
#include "Platform/Vulkan/RenderPlatform.h"
#endif
#include "ClientRender/AnimationInstance.h"
#include "ClientRender/NodeComponents/SubSceneComponent.h"
#include "TeleportClient/ClientTime.h"
#include <Platform/CrossPlatform/GpuProfiler.h>

using namespace teleport;
using namespace clientrender;
using namespace platform;
#pragma optimize("", off)
// TODO: Implement Vector, Matrix and Quaternion conversions between avs:: <-> math:: <-> CppSl.h - AJR
template <typename T1, typename T2> T1 ConvertVec2(const T2 &value)
{
	return T1(value.x, value.y);
}
template <typename T1, typename T2> T1 ConvertVec3(const T2 &value)
{
	return T1(value.x, value.y, value.z);
}
template <typename T1, typename T2> T1 ConvertVec4(const T2 &value)
{
	return T1(value.x, value.y, value.z, value.w);
}

mat4 ConvertMat4(const float value[4][4])
{
	mat4 result;
	result.M[0][0] = value[0][0];
	result.M[0][1] = value[0][1];
	result.M[0][2] = value[0][2];
	result.M[0][3] = value[0][3];
	result.M[1][0] = value[1][0];
	result.M[1][1] = value[1][1];
	result.M[1][2] = value[1][2];
	result.M[1][3] = value[1][3];
	result.M[2][0] = value[2][0];
	result.M[2][1] = value[2][1];
	result.M[2][2] = value[2][2];
	result.M[2][3] = value[2][3];
	result.M[3][0] = value[3][0];
	result.M[3][1] = value[3][1];
	result.M[3][2] = value[3][2];
	result.M[3][3] = value[3][3];
	return result;
}

uint64_t MakeNodeElementHash(avs::uid root_id, avs::uid node_id, avs::uid cache_uid, int element)
{
	return (root_id << uint64_t(32)) + (node_id << uint64_t(12)) + (cache_uid << uint16_t(6)) + element;
}

void CreateTexture(platform::crossplatform::RenderPlatform *renderPlatform, clientrender::AVSTextureHandle &th, int width, int height)
{
	if (!(th))
	{
		th.reset(new AVSTextureImpl(nullptr));
	}
	clientrender::AVSTexture *t	 = th.get();
	AVSTextureImpl			 *ti = (AVSTextureImpl *)t;
	if (!ti->texture)
	{
		ti->texture = renderPlatform->CreateTexture();
	}

	// NVidia decoder needs a shared handle to the resource.
#if TELEPORT_CLIENT_USE_D3D12 && !TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER
	bool useSharedHeap = true;
#else
	bool useSharedHeap = false;
#endif

	ti->texture->ensureTexture2DSizeAndFormat(
		renderPlatform, width, height, 1, crossplatform::RGBA_8_UNORM, true, true, false, 1, 0, false, vec4(0.5f, 0.5f, 0.2f, 1.0f), 1.0f, 0, useSharedHeap);
}
uint64_t InstanceHash(uint64_t cache_uid, const Mesh *mesh, const Node *node, int element)
{
	return (mesh->GetMeshCreateInfo().id << uint16_t(20)) + (node->GetGlobalIlluminationTextureUid() << uint64_t(12)) + (cache_uid << uint16_t(6)) + element;
}

InstanceRenderer::InstanceRenderer(avs::uid server, teleport::client::Config &c, GeometryDecoder &g, RenderState &rs, teleport::client::SessionClient *sc)
	: sessionClient(sc), renderState(rs), config(c), geometryDecoder(g)
{
	server_uid = server;
	audioPlayer.initializeAudioDevice();
	sessionClient->SetSessionCommandInterface(this);
}

InstanceRenderer::~InstanceRenderer()
{
	InvalidateDeviceObjects();
}

void InstanceRenderer::RestoreDeviceObjects(platform::crossplatform::RenderPlatform *r)
{
	renderPlatform			   = r;
	std::string url_root	   = server_uid ? sessionClient->GetConnectionURL() : "Local";
	auto		domainPortPath = core::GetDomainPortPath(url_root);
	GeometryCache::CreateGeometryCache(server_uid, -1, domainPortPath.domain);
	geometryCache							   = GeometryCache::GetGeometryCache(server_uid);
	instanceRenderState.videoTexture		   = renderPlatform->CreateTexture();
	instanceRenderState.specularCubemapTexture = renderPlatform->CreateTexture();
	instanceRenderState.diffuseCubemapTexture  = renderPlatform->CreateTexture();
	instanceRenderState.lightingCubemapTexture = renderPlatform->CreateTexture();
}

void InstanceRenderer::InvalidateDeviceObjects()
{
	GeometryCache::DestroyGeometryCache(server_uid);
	AVSTextureImpl *ti = (AVSTextureImpl *)instanceRenderState.avsTexture.get();
	if (ti)
	{
		SAFE_DELETE(ti->texture);
	}
	SAFE_DELETE(instanceRenderState.diffuseCubemapTexture);
	SAFE_DELETE(instanceRenderState.specularCubemapTexture);
	SAFE_DELETE(instanceRenderState.lightingCubemapTexture);
	SAFE_DELETE(instanceRenderState.videoTexture);
}

void InstanceRenderer::RenderBackgroundTexture(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	platform::crossplatform::Texture *t = nullptr;
	auto							  d = geometryCache->mTextureManager.Get(sessionClient->GetSetupCommand().backgroundTexture);
	if (d)
	{
		t = d->GetSimulTexture();
	}
	if (!t)
	{
		return;
	}
	bool multiview								  = deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr;
	renderState.cubemapConstants.depthOffsetScale = vec4(0, 0, 0, 0);
	auto &clientServerState						  = sessionClient->GetClientServerState();
	renderState.cubemapConstants.cameraPosition	  = *((vec3 *)&clientServerState.headPose.position);
	renderState.cubemapConstants.cameraRotation	  = *((vec4 *)&clientServerState.headPose.orientation);
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
	if (t->IsCubemap())
	{
		renderState.cubemapClearEffect->SetTexture(deviceContext, "cubemapTexture", t);
	}
	else
	{
		renderState.cubemapClearEffect->SetTexture(deviceContext, "plainTexture", t);
	}
	renderState.cubemapClearEffect->Apply(deviceContext, t->IsCubemap() ? "far_cubemap" : "far_perspective", multiview ? "multiview" : "singleview");
	deviceContext.renderPlatform->DrawQuad(deviceContext);
	renderState.cubemapClearEffect->Unapply(deviceContext);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RenderVideoTexture(crossplatform::GraphicsDeviceContext &deviceContext,
										  crossplatform::Texture			   *srcTexture,
										  crossplatform::Texture			   *targetTexture,
										  const char						   *technique,
										  const char						   *shaderTexture)
{
	bool  multiview			= deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr;

	auto &clientServerState = sessionClient->GetClientServerState();
	renderState.tagDataCubeBuffer.Apply(deviceContext, renderState.cubemapClearEffect_TagDataCubeBuffer);
	renderState.cubemapConstants.depthOffsetScale = vec4(0, 0, 0, 0);
	renderState.cubemapConstants.offsetFromVideo  = *((vec3 *)&clientServerState.headPose.position) - videoPos;
	renderState.cubemapConstants.cameraPosition	  = *((vec3 *)&clientServerState.headPose.position);
	renderState.cubemapConstants.cameraRotation	  = *((vec4 *)&clientServerState.headPose.orientation);
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
	renderState.cubemapClearEffect->SetTexture(deviceContext, shaderTexture, targetTexture);
	renderState.cubemapClearEffect->SetTexture(deviceContext, "plainTexture", srcTexture);
	renderState.cubemapClearEffect->Apply(deviceContext, technique, multiview ? "multiview" : "singleview");
	deviceContext.renderPlatform->DrawQuad(deviceContext);
	renderState.cubemapClearEffect->Unapply(deviceContext);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RecomposeVideoTexture(crossplatform::GraphicsDeviceContext &deviceContext,
											 crossplatform::Texture				  *srcTexture,
											 crossplatform::Texture				  *targetTexture,
											 const char							  *technique)
{
	int W									  = targetTexture->width;
	int H									  = targetTexture->length;
	renderState.cubemapConstants.sourceOffset = {0, 0};
	renderState.cubemapConstants.targetSize.x = W;
	renderState.cubemapConstants.targetSize.y = H;

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
		cubemapClearEffect->Apply(deviceContext, "test", faceColour ? "test_face_colour" : "test");
		int zGroups = videoTexture->IsCubemap() ? 6 : 1;
		renderPlatform->DispatchCompute(deviceContext, targetTexture->width/16, targetTexture->length/16, zGroups);
		cubemapClearEffect->Unapply(deviceContext);
	}
#endif

	renderState.cubemapClearEffect->SetTexture(deviceContext, "plainTexture", srcTexture);
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cameraConstants);
	renderPlatform->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, targetTexture);
	renderState.tagDataIDBuffer.Apply(deviceContext, renderState.cubemapClearEffect_TagDataIDBuffer);
	int zGroups = instanceRenderState.videoTexture->IsCubemap() ? 6 : 1;
	renderState.cubemapClearEffect->Apply(deviceContext, technique, 0);
	deviceContext.renderPlatform->DispatchCompute(deviceContext, W / 16, H / 16, zGroups);
	renderState.cubemapClearEffect->Unapply(deviceContext);
	renderPlatform->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, nullptr);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RecomposeCubemap(crossplatform::GraphicsDeviceContext &deviceContext,
										crossplatform::Texture				 *srcTexture,
										crossplatform::Texture				 *targetTexture,
										int									  mips,
										int2								  sourceOffset)
{
	if (targetTexture->width == 0 || targetTexture->length == 0)
	{
		return;
	}
	renderState.cubemapConstants.sourceOffset = sourceOffset;
	deviceContext.renderPlatform->SetTexture(deviceContext, renderState.plainTexture, srcTexture);

	renderState.cubemapConstants.targetSize.x = targetTexture->width;
	renderState.cubemapConstants.targetSize.y = targetTexture->length;

	for (int m = 0; m < mips; m++)
	{
		crossplatform::SubresourceLayers srl;
		srl = {crossplatform::TextureAspectFlags::COLOUR, uint8_t(m), 0, 1};
		renderPlatform->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, targetTexture, srl);
		deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
		renderState.cubemapClearEffect->Apply(deviceContext, "recompose", 0);
		deviceContext.renderPlatform->DispatchCompute(deviceContext, targetTexture->width / 16, targetTexture->width / 16, 6);
		renderState.cubemapClearEffect->Unapply(deviceContext);
		renderState.cubemapConstants.sourceOffset.x += 3 * renderState.cubemapConstants.targetSize.x;
		renderState.cubemapConstants.targetSize /= 2;
	}
	renderPlatform->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, nullptr);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RenderView(crossplatform::GraphicsDeviceContext &deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "InstanceRenderer::RenderView");
	auto						   renderPlatform = deviceContext.renderPlatform;

	clientrender::AVSTextureHandle th			  = instanceRenderState.avsTexture;
	clientrender::AVSTexture	  &tx			  = *th;
	AVSTextureImpl				  *ti			  = static_cast<AVSTextureImpl *>(&tx);

	if (ti)
	{
		// This will apply to both rendering methods
		deviceContext.renderPlatform->SetTexture(deviceContext, renderState.plainTexture, ti->texture);
		renderState.tagDataIDBuffer.ApplyAsUnorderedAccessView(deviceContext, renderState._RWTagDataIDBuffer);
		renderState.cubemapConstants.sourceOffset = int2(ti->texture->width - (32 * 4), ti->texture->length - 4);
		deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
		renderState.cubemapClearEffect->Apply(deviceContext, "extract_tag_data_id", 0);
		renderPlatform->DispatchCompute(deviceContext, 1, 1, 1);
		renderState.cubemapClearEffect->Unapply(deviceContext);
		renderState.cubemapClearEffect->UnbindTextures(deviceContext);

		renderState.tagDataIDBuffer.CopyToReadBuffer(deviceContext);
		const uint4 *videoIDBuffer = renderState.tagDataIDBuffer.OpenReadBuffer(deviceContext);
		if (videoIDBuffer && videoIDBuffer[0].x < 32 && videoIDBuffer[0].w == 110) // sanity check
		{
			int			tagDataID = videoIDBuffer[0].x;
			const auto &ct		  = videoTagDataCubeArray[tagDataID].coreData.cameraTransform;
			videoPos			  = vec3(ct.position.x, ct.position.y, ct.position.z);
			videoPosDecoded		  = true;
		}
		renderState.tagDataIDBuffer.CloseReadBuffer(deviceContext);
		UpdateTagDataBuffers(deviceContext);
		if (sessionClient->IsConnected())
		{
			if (sessionClient->GetSetupCommand().backgroundMode == teleport::core::BackgroundMode::VIDEO)
			{
				if (instanceRenderState.videoTexture->IsCubemap())
				{
					const char *technique = sessionClient->GetSetupCommand().video_config.use_alpha_layer_decoding ? "recompose" : "recompose_with_depth_alpha";
					RecomposeVideoTexture(deviceContext, ti->texture, instanceRenderState.videoTexture, technique);
				}
				else
				{
					const char *technique = sessionClient->GetSetupCommand().video_config.use_alpha_layer_decoding ? "recompose_perspective"
																												   : "recompose_perspective_with_depth_alpha";
					RecomposeVideoTexture(deviceContext, ti->texture, instanceRenderState.videoTexture, technique);
				}
			}
		}
		RecomposeCubemap(deviceContext,
						 ti->texture,
						 instanceRenderState.diffuseCubemapTexture,
						 instanceRenderState.diffuseCubemapTexture->mips,
						 int2(sessionClient->GetDynamicLighting().diffusePos[0], sessionClient->GetDynamicLighting().diffusePos[1]));
		RecomposeCubemap(deviceContext,
						 ti->texture,
						 instanceRenderState.specularCubemapTexture,
						 instanceRenderState.specularCubemapTexture->mips,
						 int2(sessionClient->GetDynamicLighting().specularPos[0], sessionClient->GetDynamicLighting().specularPos[1]));
	}

	// Draw the background. If unconnected, we show a grid and horizon.
	// If connected, we show the server's chosen background: video, texture or colour.
	{
		ApplyCameraMatrices(deviceContext, renderState);
		ApplySceneMatrices(deviceContext);

		if (sessionClient->IsConnected())
		{
			auto backgroundMode = sessionClient->GetSetupCommand().backgroundMode;
			if (backgroundMode == teleport::core::BackgroundMode::COLOUR)
			{
				renderPlatform->Clear(deviceContext, sessionClient->GetSetupCommand().backgroundColour);
			}
			else if (backgroundMode == teleport::core::BackgroundMode::TEXTURE)
			{
				RenderBackgroundTexture(deviceContext);
			}
			else if (backgroundMode == teleport::core::BackgroundMode::VIDEO)
			{
				if (instanceRenderState.videoTexture->IsCubemap())
				{
					RenderVideoTexture(deviceContext, ti->texture, instanceRenderState.videoTexture, "use_cubemap", "cubemapTexture");
				}
				else
				{
					math::Matrix4x4 projInv;
					deviceContext.viewStruct.proj.Inverse(projInv);
					RenderVideoTexture(deviceContext, ti->texture, instanceRenderState.videoTexture, "use_perspective", "perspectiveTexture");
				}
			}
		}
	}
	// Per-frame stuff:
	if (sessionClient->IsConnected() || config.options.showGeometryOffline)
	{
		RenderLocalNodes(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
}

void InstanceRenderer::ApplyCameraMatrices(crossplatform::GraphicsDeviceContext &deviceContext, RenderState &renderState)
{
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		crossplatform::MultiviewGraphicsDeviceContext &mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
		mgdc.viewStructs[0].Init();
		mgdc.viewStructs[1].Init();
		renderState.stereoCameraConstants.leftInvViewProj		= mgdc.viewStructs[0].invViewProj;
		renderState.stereoCameraConstants.leftView				= mgdc.viewStructs[0].view;
		renderState.stereoCameraConstants.leftProj				= mgdc.viewStructs[0].proj;
		renderState.stereoCameraConstants.leftViewProj			= mgdc.viewStructs[0].viewProj;
		renderState.stereoCameraConstants.rightInvViewProj		= mgdc.viewStructs[1].invViewProj;
		renderState.stereoCameraConstants.rightView				= mgdc.viewStructs[1].view;
		renderState.stereoCameraConstants.rightProj				= mgdc.viewStructs[1].proj;
		renderState.stereoCameraConstants.rightViewProj			= mgdc.viewStructs[1].viewProj;
		renderState.stereoCameraConstants.stereoViewPosition[0] = (const float *)&mgdc.viewStructs[0].cam_pos;
		renderState.stereoCameraConstants.stereoViewPosition[1] = (const float *)&mgdc.viewStructs[1].cam_pos;

		renderState.stereoCameraConstants.SetHasChanged();
	}
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.stereoCameraConstants);
	// else
	{
		deviceContext.viewStruct.Init();
		renderState.cameraConstants.invViewProj	 = deviceContext.viewStruct.invViewProj;
		renderState.cameraConstants.view		 = deviceContext.viewStruct.view;
		renderState.cameraConstants.proj		 = deviceContext.viewStruct.proj;
		renderState.cameraConstants.viewProj	 = deviceContext.viewStruct.viewProj;
		renderState.cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
		renderState.cameraConstants.frameNumber	 = (int)deviceContext.renderPlatform->GetFrameNumber();
		renderState.cameraConstants.SetHasChanged();
	}
	deviceContext.renderPlatform->SetConstantBuffer(deviceContext, &renderState.cameraConstants);
}

void InstanceRenderer::ApplySceneMatrices(platform::crossplatform::GraphicsDeviceContext &deviceContext)
{
	if (instanceRenderState.specularCubemapTexture)
	{
		renderState.teleportSceneConstants.roughestMip = float(instanceRenderState.specularCubemapTexture->mips - 1);
	}
	if (sessionClient->GetDynamicLighting().specular_cubemap_texture_uid != 0)
	{
		auto t = geometryCache->mTextureManager.Get(sessionClient->GetDynamicLighting().specular_cubemap_texture_uid);
		if (t && t->GetSimulTexture())
		{
			renderState.teleportSceneConstants.roughestMip = float(t->GetSimulTexture()->mips - 1);
		}
	}
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto										&cachedLights = geometryCache->mLightManager.GetCache(cacheLock);
		if (cachedLights.size() > renderState.lightsBuffer.count)
		{
			renderState.lightsBuffer.InvalidateDeviceObjects();
			renderState.lightsBuffer.RestoreDeviceObjects(renderPlatform, static_cast<int>(cachedLights.size()));
		}
		renderState.teleportSceneConstants.lightCount = static_cast<int>(cachedLights.size());
	}
	if (renderState.shaderMode == ShaderMode::DEBUG_ANIM)
	{
// TODO: find skeleton. We want to do this, but while skeleton is stored in the mesh node, and not the root of the hierarchy, it won't work/
#if 0
		auto cache=GeometryCache::GetGeometryCache(renderState.selected_cache);
		if(cache)
		{
			auto node = cache->mNodeManager.GetNode(renderState.selected_uid);
			while(node&&!node->GetSkeleton())
			{
				node=node->GetParent().lock();
			}
			if(node)
			{
				const auto &boneids=node->GetSkeleton()->GetExternalBoneIds();
				renderState.teleportSceneConstants.debugHighlightBone= (int)(std::find(boneids.begin(),boneids.end(),renderState.selected_uid)-boneids.begin());
			}
		}
#else

#endif
	}
	renderState.teleportSceneConstants.drawDistance = sessionClient->GetSetupCommand().draw_distance;
	renderPlatform->SetConstantBuffer(deviceContext, &renderState.teleportSceneConstants);
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		renderPlatform->SetConstantBuffer(deviceContext, &renderState.stereoCameraConstants);
	}
	renderState.teleportSceneConstants.SetHasChanged();
	// The following are not in group 0, but probably should be:
	if (renderState._lights.valid)
	{
		renderState.lightsBuffer.Apply(deviceContext, renderState._lights);
	}
	renderState.tagDataCubeBuffer.Apply(deviceContext, renderState.cubemapClearEffect_TagDataCubeBuffer);
	if (renderState.pbrEffect_TagDataIDBuffer.valid)
	{
		renderState.tagDataIDBuffer.Apply(deviceContext, renderState.pbrEffect_TagDataIDBuffer);
	}

	renderPlatform->ApplyResourceGroup(deviceContext, 0);
}

void InstanceRenderer::ApplyMaterialConstants(crossplatform::GraphicsDeviceContext &deviceContext, std::shared_ptr<clientrender::Material> material)
{
	renderPlatform->SetConstantBuffer(deviceContext, &material->pbrMaterialConstants);
	const clientrender::Material::MaterialCreateInfo &matInfo = material->GetMaterialCreateInfo();
	// TODO: this is not ideal. But we must check for null textures.
	platform::crossplatform::Texture *diffuse				  = matInfo.diffuse.texture ? matInfo.diffuse.texture->GetSimulTexture() : nullptr;
	platform::crossplatform::Texture *normal				  = matInfo.normal.texture ? matInfo.normal.texture->GetSimulTexture() : nullptr;
	platform::crossplatform::Texture *combined				  = matInfo.combined.texture ? matInfo.combined.texture->GetSimulTexture() : nullptr;
	platform::crossplatform::Texture *emissive				  = matInfo.emissive.texture ? matInfo.emissive.texture->GetSimulTexture() : nullptr;
	auto							 &resourceCreator		  = ResourceCreator::GetInstance();
	renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_diffuseTexture, diffuse ? diffuse : resourceCreator.m_DummyWhite->GetSimulTexture());
	renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_normalTexture, normal ? normal : resourceCreator.m_DummyNormal->GetSimulTexture());
	renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_combinedTexture, combined ? combined : resourceCreator.m_DummyCombined->GetSimulTexture());
	renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_emissiveTexture, emissive ? emissive : resourceCreator.m_DummyBlack->GetSimulTexture());
	renderPlatform->ApplyResourceGroup(deviceContext, 2);
}

struct RenderStateTracker
{
	platform::crossplatform::Texture *globalIlluminationTexture = nullptr;
	avs::uid						  gi_texture_id				= 0;
	void							  reset()
	{
		globalIlluminationTexture = nullptr;
		gi_texture_id			  = 0;
	}
};
RenderStateTracker renderStateTracker;

void			   InstanceRenderer::RenderLocalNodes(crossplatform::GraphicsDeviceContext &deviceContext)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "initial");
	ApplyCameraMatrices(deviceContext, renderState);
	ApplySceneMatrices(deviceContext);
	double serverTimeS =
		client::ClientTime::GetInstance().ClientToServerTimeS(sessionClient->GetSetupCommand().startTimestamp_utc_unix_us, deviceContext.predictedDisplayTimeS);
	geometryCache->mNodeManager.UpdateExtrapolatedPositions(serverTimeS);
	auto  renderPlatform	= deviceContext.renderPlatform;
	auto &clientServerState = sessionClient->GetClientServerState();
	// Now, any nodes bound to OpenXR poses will be updated. This may include hand objects, for example.
	if (renderState.openXR)
	{
		avs::uid root_node_uid = renderState.openXR->GetRootNode(server_uid);
	}
	// Now, any nodes bound to OpenXR poses will be updated. This may include hand objects, for example.
	if (renderState.openXR)
	{
		// The node pose states are in the space whose origin is the VR device's playspace origin.
		const auto &nodePoseStates = renderState.openXR->GetNodePoseStates(server_uid, renderPlatform->GetFrameNumber());
		for (auto &n : nodePoseStates)
		{
			// TODO, we set LOCAL node pose from GLOBAL worldspace because we ASSUME no parent for these nodes.
			std::shared_ptr<clientrender::Node> node = geometryCache->mNodeManager.GetNode(n.first);
			if (node)
			{
				// TODO: Should be done as local child of an origin node, not setting local pos = globalPose.pos
				node->SetLocalPosition(n.second.pose_footSpace.pose.position);
				node->SetLocalRotation(*((quat *)&n.second.pose_footSpace.pose.orientation));
				node->SetLocalVelocity(*((vec3 *)&n.second.pose_footSpace.velocity));
				// force update of model matrices - should not be necessary, but is.
				node->UpdateModelMatrix();
			}
		}
	}

	renderStateTracker.reset();
	SubSceneNodeStates &subSceneNodeStates = subSceneStatesMap[0];
	// Accumulate the meshes to render:
	UpdateGeometryCacheForRendering(deviceContext, subSceneNodeStates, geometryCache);
	// general lighting:
	{
		platform::crossplatform::Texture *diffuseCubemapTexture	 = instanceRenderState.diffuseCubemapTexture;
		platform::crossplatform::Texture *specularCubemapTexture = instanceRenderState.specularCubemapTexture;
		// If lighting is via static textures.
		if (sessionClient->GetDynamicLighting().lightingMode == teleport::core::LightingMode::TEXTURE)
		{
			auto d = geometryCache->mTextureManager.Get(sessionClient->GetDynamicLighting().diffuse_cubemap_texture_uid);
			if (d)
			{
				diffuseCubemapTexture = d->GetSimulTexture();
			}
			auto s = geometryCache->mTextureManager.Get(sessionClient->GetDynamicLighting().specular_cubemap_texture_uid);
			if (s)
			{
				specularCubemapTexture = s->GetSimulTexture();
			}
		}
		renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_diffuseCubemap, diffuseCubemapTexture);
		renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_specularCubemap, specularCubemapTexture);
		renderPlatform->ApplyResourceGroup(deviceContext, 1);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
	SIMUL_COMBINED_PROFILE_START(deviceContext, "passes");
	// TODO: Find a way to protect this without locks.
	std::lock_guard<std::mutex> passRenders_lock(passRenders_mutex);
	for (auto p : passRenders)
	{
		RenderPass(deviceContext, *p.second.get(), p.first);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
	SIMUL_COMBINED_PROFILE_START(deviceContext, "links");
	for (const auto &l : linkRenders)
	{
		RenderLink(deviceContext, *l.second.get());
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);

	SIMUL_COMBINED_PROFILE_START(deviceContext, "canvases");
	for (const auto &c : canvasRenders)
	{
		RenderTextCanvas(deviceContext, c.second.get());
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
	if (config.debugOptions.showOverlays)
	{
		auto &rootNodes = geometryCache->mNodeManager.GetRootNodes();
		for (const auto &m : rootNodes)
		{
			RenderNodeOverlay(deviceContext, geometryCache, subSceneNodeStates, m.lock(), false);
		}
		if (renderState.selected_cache == geometryCache->GetCacheUid())
		{
			auto node = geometryCache->mNodeManager.GetNode(renderState.selected_uid);
			RenderNodeOverlay(deviceContext, geometryCache, subSceneNodeStates, node, true);
		}
	}
}

void InstanceRenderer::RenderLink(platform::crossplatform::GraphicsDeviceContext &deviceContext, LinkRender &l)
{
	// Are we pointing at the link?
	{
		vec3  cam_to_link = l.position - deviceContext.viewStruct.cam_pos;
		float d			  = length(cam_to_link);
		if (d > 0 && d < renderState.next_nearest_link_distance)
		{
			vec3  cam_to_link_dir = cam_to_link / d;
			float cosine		  = dot(cam_to_link_dir, renderState.current_controller_dir);
			if (cosine > 0)
			{
				float along_controller_dir = d / cosine;
				vec3  nearest			   = deviceContext.viewStruct.cam_pos + along_controller_dir * renderState.current_controller_dir;
				vec3  diff				   = nearest - l.position;
				float nearest_dist		   = length(diff);
				if (nearest_dist < 0.5f)
				{
					renderState.next_nearest_link_distance	= d;
					// TODO: what about prefabs/subcaches?
					renderState.next_nearest_link_cache_uid = this->server_uid;
					renderState.next_nearest_link_uid		= l.uid;
				}
			}
		}
	}
	bool highlight = renderState.nearest_link_uid == l.uid;
	ApplyModelMatrix(deviceContext, *l.model);
	renderPlatform->SetConstantBuffer(deviceContext, &renderState.perNodeConstants);
	renderState.linkRenderer.RenderLink(deviceContext, l, highlight);
	vec4	   colour				= {0.2f, 0.4f, 0.7f, 1.f};
	vec4	   highlight_colour		= {1.0f, 0.8f, 0.5f, 1.f};
	const vec4 background			= {0, 0, 0, 0.5f};
	const vec4 highlight_background = {0, 0, 0, 0.8f};
	float	   width				= 1.8f;
	float	   height				= 0.2f;
	vec4	   canvas				= {-width / 2.0f, height / 2.0f, width, -height};
	if (l.url.length() > l.fontChars.count)
	{
		l.fontChars.RestoreDeviceObjects(renderPlatform, (int)l.url.length(), false, false, nullptr, "fontChars");
	}
	renderState.canvasTextRenderer.Render(deviceContext,
										  renderState.commonFontAtlas.get(),
										  64,
										  l.url,
										  highlight ? highlight_colour : colour,
										  canvas,
										  highlight ? highlight_background : background,
										  0.1f,
										  l.fontChars,
										  true);
}

void InstanceRenderer::UpdateMouse(vec3 orig, vec3 dir, float &distance, std::string &url)
{
	dir = normalize(dir);
	for (auto l : linkRenders)
	{
		vec3  diff	= l.second->position - orig;
		float along = dot(diff, dir);
	}
}
static uint64_t MakeNodeHash(avs::uid root_node_uid, avs::uid cache_uid, avs::uid node_id)
{
	uint64_t node_hash = (root_node_uid << uint64_t(48)) + (node_id << uint64_t(12)) + (cache_uid << uint16_t(6));
	return node_hash;
}

void InstanceRenderer::RenderPass(platform::crossplatform::GraphicsDeviceContext &deviceContext, PassRender &p, platform::crossplatform::EffectPass *pass)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, pass->name.c_str());
	renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);

	p.layout->Apply(deviceContext);
	renderPlatform->ApplyPass(deviceContext, pass);
	for (auto m : p.materialRenders)
	{
		RenderMaterial(deviceContext, *m.second.get());
	}
	p.layout->Unapply(deviceContext);
	renderPlatform->SetLayout(deviceContext, nullptr);
	renderPlatform->UnapplyPass(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext);
}

void InstanceRenderer::AddNodeToInstanceRender(avs::uid cache_uid, SubSceneNodeStates &subSceneNodeStates, avs::uid node_uid)
{
	subSceneNodeStates.nodeStates[node_uid];
	std::lock_guard<std::mutex> passRenders_lock(passRenders_mutex);
	auto						g = GeometryCache::GetGeometryCache(cache_uid);
	if (!g)
	{
		TELEPORT_CERR << "AddNodeToInstanceRender: no cache found.\n";
		return;
	}
	auto node = g->mNodeManager.GetNode(node_uid);
	if (!node)
	{
		TELEPORT_CERR << "AddNodeToInstanceRender: no node found.\n";
		return;
	}
	//	TELEPORT_COUT << "AddNodeToInstanceRender: cache " << cache_uid << ", node " << node_uid << node->name.c_str() << "\n";
	if (!node->IsVisible())
	{
		TELEPORT_CERR << "AddNodeToInstanceRender:  node not visible.\n";
		return;
	}
	// Only render visible nodes, but still render children that are close enough.
	if (node->GetPriority() >= 0)
	{
		const std::shared_ptr<clientrender::Mesh> mesh		 = node->GetMesh();
		const std::shared_ptr<TextCanvas>		  textCanvas = node->GetTextCanvas();
		if (mesh)
		{
			std::shared_ptr<NodeRender> nodeRender = nodeRenders[node.get()];
			if (!nodeRender)
			{
				nodeRender = std::make_shared<NodeRender>();
			}
			AddNodeMeshToInstanceRender(cache_uid, subSceneNodeStates, node, mesh);
		}
		if (textCanvas)
		{
			NodeState					 &nodeState										 = subSceneNodeStates.nodeStates[node->id];
			std::shared_ptr<CanvasRender> cr											 = std::make_shared<CanvasRender>();
			cr->textCanvas																 = textCanvas;
			cr->model																	 = &(nodeState.renderModelMatrix);
			canvasRenders[MakeNodeHash(subSceneNodeStates.root_id, cache_uid, node_uid)] = cr;
			// TODO: Ensure canvasRender is removed when node is deleted.
		}
		if (node->GetURL().length() > 0)
		{
			NodeState				   &nodeState									   = subSceneNodeStates.nodeStates[node->id];
			std::shared_ptr<LinkRender> lr											   = std::make_shared<LinkRender>();
			lr->url																	   = node->GetURL();
			lr->position															   = node->GetGlobalTransform().m_Translation;
			lr->model																   = &(nodeState.renderModelMatrix);
			lr->uid																	   = node->id;
			linkRenders[MakeNodeHash(subSceneNodeStates.root_id, cache_uid, node_uid)] = lr;
			// nodeRender->linkRenders.insert(lr);
		}
	}
	else
	{
		TELEPORT_CERR << "AddNodeToInstanceRender:  node is low priority.\n";
	}
}

void InstanceRenderer::UpdateNodeRenders()
{
#if 0
	struct CacheNode
	{
		std::set<avs::uid> nodes;
	};
	std::map<avs::uid, CacheNode> cacheNodes;
	for (auto p : passRenders)
	{
		for (auto r : p.second->materialRenders)
		{
			for (auto m : r.second->meshRenders)
			{
				cacheNodes[m.second->cache_uid];
			}
			for (auto i : r.second->instancedRenders)
			{
				for (auto m : i.second->meshInstanceRenders)
					cacheNodes[i.second->cache_uid];
			}
		}
	}
	for (auto p : passRenders)
	{
		for (auto r : p.second->materialRenders)
		{
			for (auto m : r.second->meshRenders)
			{
				auto &cacheNode = cacheNodes[m.second->cache_uid];
				cacheNode.nodes.insert(m.second->node->id);
			}
			for (auto i : r.second->instancedRenders)
			{
				auto &cacheNode = cacheNodes[i.second->cache_uid];
				for (auto m : i.second->meshInstanceRenders)
				{
					cacheNode.nodes.insert(m.second->id);
				}
			}
		}
	}
#endif
	passRenders.clear();
	subSceneStatesMap.clear();
#if 0
	auto &subSceneNodeStates = subSceneStatesMap[0];
	for (auto c : cacheNodes)
	{
		for (auto n : c.second.nodes)
		{
			AddNodeToInstanceRender(c.first, subSceneNodeStates, n);
		}
	}
#endif
}

void InstanceRenderer::AddNodeMeshToInstanceRender(avs::uid									 cache_uid,
												   SubSceneNodeStates						&subSceneNodeStates,
												   std::shared_ptr<Node>					 node,
												   const std::shared_ptr<clientrender::Mesh> mesh)
{
	auto		geometrySubCache = GeometryCache::GetGeometryCache(cache_uid);
	const auto &meshInfo		 = mesh->GetMeshCreateInfo();
	uint64_t	node_hash		 = MakeNodeHash(subSceneNodeStates.root_id, cache_uid, node->id);
	// iterate through the submeshes.
	auto &materials				 = node->GetMaterials();
	bool  rezzing				 = false;
	bool  instanced				 = false;
	if (node->IsStatic())
	{
		instanced = true;
	}
	// We will instance-render groups of draws that have the same mesh, pass and material, and which are static.
	// An instance group will have an additional vertexbuffer containing the perNodeConstants model and lightmapScaleOffset
	for (uint32_t element = 0; element < meshInfo.indexBuffers.size(); element++)
	{
		if (element >= materials.size())
		{
			TELEPORT_CERR << "AddNodeMeshToInstanceRender: element out of range.\n";
			continue;
		}
		std::shared_ptr<clientrender::Material> material = materials[element];
		if (!material)
		{
			material = mesh->GetInternalMaterials()[element];
			if (!material)
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no material found.\n";
				continue;
			}
		}
		const clientrender::Material::MaterialCreateInfo &matInfo		 = material->GetMaterialCreateInfo();
		bool											  transparent	 = (matInfo.materialMode == avs::MaterialMode::TRANSPARENT_MATERIAL);

		const vec3										 &sc			 = node->GetGlobalScale();
		bool											  negative_scale = (sc.x * sc.y * sc.z) < 0.0f;
		bool											  clockwise		 = mesh->GetMeshCreateInfo().clockwiseFaces ^ negative_scale;
		bool											  anim			 = false;
		bool											  highlight		 = node->IsHighlighted();

		highlight |= (renderState.selected_uid == material->id);

		// Pass used for rendering geometry.
		const clientrender::PassCache *passCache = node->GetCachedEffectPass(element);
		crossplatform::EffectPass	  *pass		 = passCache ? passCache->pass : nullptr;
		if (pass && node->GetCachedEffectPassValidity(element) != renderState.shaderValidity)
		{
			pass = nullptr;
		}
		std::vector<crossplatform::LayoutDesc> meshLayoutDesc;
		if (!pass)
		{
			auto *vb = meshInfo.vertexBuffers[element].get();
			if (!vb)
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no vb found.\n";
				continue;
			}
			auto *meshLayout = vb->GetLayout();
			// The layout of the mesh:
			meshLayoutDesc	 = meshLayout->GetDesc();
			if (instanced)
			{
				static std::vector<crossplatform::LayoutDesc> instanceLayoutDesc;
				if (!instanceLayoutDesc.size())
				{
					crossplatform::LayoutDesc lightmapScaleOffsetLayoutDesc;
					lightmapScaleOffsetLayoutDesc.semanticName		   = "LIGHTMAP";
					lightmapScaleOffsetLayoutDesc.semanticIndex		   = 0;
					lightmapScaleOffsetLayoutDesc.format			   = platform::crossplatform::RGBA_32_FLOAT;
					lightmapScaleOffsetLayoutDesc.inputSlot			   = 0;
					lightmapScaleOffsetLayoutDesc.alignedByteOffset	   = 0;
					lightmapScaleOffsetLayoutDesc.perInstance		   = true;
					lightmapScaleOffsetLayoutDesc.instanceDataStepRate = 1;
					instanceLayoutDesc.push_back(lightmapScaleOffsetLayoutDesc);
					for (int i = 0; i < 4; i++)
					{
						crossplatform::LayoutDesc modelMatrixLayoutDesc;
						modelMatrixLayoutDesc.semanticName		   = "MATRIX";
						modelMatrixLayoutDesc.semanticIndex		   = i;
						modelMatrixLayoutDesc.format			   = platform::crossplatform::RGBA_32_FLOAT;
						modelMatrixLayoutDesc.inputSlot			   = 0;
						modelMatrixLayoutDesc.alignedByteOffset	   = 16 + 16 * i;
						modelMatrixLayoutDesc.perInstance		   = true;
						modelMatrixLayoutDesc.instanceDataStepRate = 1;
						instanceLayoutDesc.push_back(modelMatrixLayoutDesc);
					}
				}
				// combine the layouts.
				size_t origSize		  = meshLayoutDesc.size();
				size_t origStructSize = meshLayout->GetStructSize();
				meshLayoutDesc.resize(origSize + instanceLayoutDesc.size());
				for (int i = 0; i < instanceLayoutDesc.size(); i++)
				{
					meshLayoutDesc[origSize + i] = instanceLayoutDesc[i];
					//	meshLayoutDesc[origSize + i].alignedByteOffset+=(int)origStructSize;
				}
			}
			crossplatform::EffectVariantPass *variantPass = transparent ? renderState.transparentVariantPass : renderState.solidVariantPass;
			if (!variantPass)
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no variantPass found.\n";
				continue;
			}
			const auto &matInfo	   = material->GetMaterialCreateInfo();
			auto		layoutHash = platform::crossplatform::GetLayoutHash(meshLayoutDesc);
			//  To render with normal maps, we must have normal and tangent vertex attributes, and we must have a normal map!
			bool normal_map		   = meshLayout->HasSemantic(platform::crossplatform::LayoutSemantic::NORMAL) &&
							  meshLayout->HasSemantic(platform::crossplatform::LayoutSemantic::TANGENT) && (matInfo.normal.texture_uid != 0);
			bool		emissive		  = (matInfo.emissive.texture_uid != 0 || length(matInfo.emissive.textureOutputScalar.xyz) > 0);
			bool		combined_map	  = matInfo.combined.texture_uid != 0;
			std::string base_pixel_shader = transparent ? "ps_transparent" : "ps_solid";
			std::string vertex_shader	  = "vs_variants";
			if (renderState.multiview)
			{
				vertex_shader += "_mv";
			}
			bool		uses_lightmap = node->IsStatic() && node->GetGlobalIlluminationTextureUid() != 0;
			std::string pixel_shader  = fmt::format("{base}({lightmap}_{ambient}_{normal_map}_{emissive}_{combined_map}_{max_lights})",
													fmt::arg("base", base_pixel_shader),
													fmt::arg("lightmap", uses_lightmap),
													fmt::arg("ambient", !uses_lightmap),
													fmt::arg("normal_map", normal_map),
													fmt::arg("emissive", emissive),
													fmt::arg("combined_map", combined_map),
													fmt::arg("max_lights", 0));
			if (rezzing)
			{
				pixel_shader = "ps_digitizing";
			}
			if (config.debugOptions.useDebugShader)
			{
				pixel_shader = config.debugOptions.debugShader.c_str();
			}
			else if (matInfo.shader.length())
			{
				pixel_shader = matInfo.shader.c_str();
			}
			pass = variantPass->GetPass(vertex_shader.c_str(), layoutHash, pixel_shader.c_str());
			if (!pass)
			{
				meshLayout->GetHash();
				pass = variantPass->GetPass(vertex_shader.c_str(), layoutHash, nullptr);
				if (pass)
				{
					TELEPORT_INTERNAL_CERR("Failed to find pass with pixel shader {0}", pixel_shader);
				}
			}
			if (!pass)
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no pass found.\n";
				continue;
			}
			// Check if the layout is ok.
			auto *vertexShader = pass->shaders[crossplatform::ShaderType::SHADERTYPE_VERTEX];
			if (!vertexShader)
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no vertexShader found.\n";
				continue;
			}
			if (!crossplatform::LayoutMatches(vertexShader->layout.GetDesc(), meshLayoutDesc))
			{
				TELEPORT_CERR << "AddNodeMeshToInstanceRender: no matching layout found.\n";
				continue;
			}
			auto have_anim = vertexShader->variantValues.find("have_anim");
			bool anim	   = false;
			if (have_anim != vertexShader->variantValues.end())
			{
				if (have_anim->second == "true")
				{
					anim = true;
				}
			}
			node->SetCachedEffectPass(element, pass, anim, renderState.shaderValidity);
			passCache = node->GetCachedEffectPass(element);
		}
		if (!passCache || !pass)
		{
			TELEPORT_CERR << "AddNodeMeshToInstanceRender: no pass found.\n";
			continue;
		}
		bool						setBoneConstantBuffer = (passCache->anim);
		std::shared_ptr<PassRender> passRender;
		passRender = passRenders[pass];
		if (!passRender)
		{
			passRenders[pass] = passRender = std::make_shared<PassRender>();
			passRender->layout.reset(renderPlatform->CreateLayout((int)meshLayoutDesc.size(), meshLayoutDesc.data(), true));
		}

		uint64_t						node_element_hash = MakeNodeElementHash(subSceneNodeStates.root_id, node->id, cache_uid, element);
		std::shared_ptr<MaterialRender> materialRender	  = passRender->materialRenders[material->id];
		if (!materialRender)
		{
			passRender->materialRenders[material->id] = materialRender = std::make_shared<MaterialRender>();
			materialRender->material								   = material;
		}
		if (instanced)
		{
			auto &nodeState = subSceneNodeStates.nodeStates[node->id];
			nodeState.elementStates.resize(materials.size());
			uint64_t						 instance_hash = InstanceHash(cache_uid, mesh.get(), node.get(), element);
			auto							 ir			   = materialRender->instancedRenders.find(instance_hash);
			std::shared_ptr<InstancedRender> instancedRender;
			if (ir == materialRender->instancedRenders.end())
			{
				materialRender->instancedRenders[instance_hash] = instancedRender = std::make_shared<InstancedRender>();
				instancedRender->instanceBuffer.reset(renderPlatform->CreateBuffer());
			}
			else
			{
				instancedRender = materialRender->instancedRenders[instance_hash];
			}
			// initialize or add to the instancedRender.
			instancedRender->valid									 = false;
			uint64_t node_instance_hash								 = MakeNodeElementHash(subSceneNodeStates.root_id, node->id, cache_uid, element);
			instancedRender->meshInstanceRenders[node_instance_hash] = {node, &(nodeState.renderModelMatrix)};
			nodeState.elementStates[element].materialRender			 = materialRender;
			nodeState.elementStates[element].hash					 = instance_hash;
			instancedRender->instanceBuffer->EnsureVertexBuffer(
				renderPlatform, (int)instancedRender->meshInstanceRenders.size(), sizeof(mat4) + sizeof(vec4), nullptr);
			instancedRender->cache_uid	   = cache_uid;
			instancedRender->material	   = material;
			instancedRender->mesh		   = node->GetMesh();
			instancedRender->gi_texture_id = node->GetGlobalIlluminationTextureUid();
			instancedRender->clockwise	   = clockwise;
			instancedRender->element	   = element;
		}
		else
		{
			auto &nodeState = subSceneNodeStates.nodeStates[node->id];
			nodeState.elementStates.resize(materials.size());
			std::shared_ptr<MeshRender> meshRender = materialRender->meshRenders[node_element_hash];
			if (!meshRender)
			{
				materialRender->meshRenders[node_element_hash] = meshRender = std::make_shared<MeshRender>();
			}
			meshRender->skeleton.reset();
			if (passCache->anim)
			{
				anim = true;
				{
					// The bone matrices transform from the original local position of a vertex
					//								to its current animated local position.
					// For each bone matrix,
					//				pos_local= (bone_matrix_j) * pos_original_local
					auto skelNode = node->GetSkeletonNode().lock();
					if (skelNode)
					{
						meshRender->skeleton = skelNode->GetSkeleton();
					}
				}
			}
			// std::cout << "AddNodeMeshToInstanceRender: cache " << cache_uid << ", node " << node->id<<", element "<<element<< "\n";
			meshRender->material							= material;
			meshRender->model								= &(nodeState.renderModelMatrix);
			nodeState.elementStates[element].materialRender = materialRender;
			nodeState.elementStates[element].hash			= node_element_hash;
			meshRender->cache_uid							= cache_uid;
			meshRender->gi_texture_id						= node->GetGlobalIlluminationTextureUid();
			meshRender->mesh								= node->GetMesh();
			meshRender->node								= node;
			meshRender->setBoneConstantBuffer				= setBoneConstantBuffer;
			meshRender->clockwise							= clockwise;

			meshRender->element								= element;
			if (!meshRender->material)
			{
				TELEPORT_BREAK_ONCE("No material found.");
			}
			if (meshRender->setBoneConstantBuffer && !meshRender->skeleton.use_count())
			{
				TELEPORT_BREAK_ONCE("No skeleton found.");
			}
		}
	}
}

void InstanceRenderer::RemoveGeometryCacheFromInstanceRender(avs::uid cache_uid, SubSceneNodeStates &subSceneNodeStates)
{
	auto g = GeometryCache::GetGeometryCache(cache_uid);
	if (!g)
	{
		return;
	}
	const auto rootNodes = g->mNodeManager.GetRootNodes();
	for (auto st : subSceneNodeStates.nodeStates)
	{
		RemoveNodeFromInstanceRender(cache_uid, subSceneNodeStates, st.first);
	}
}

void InstanceRenderer::RemoveNodeFromInstanceRender(avs::uid cache_uid, SubSceneNodeStates &subSceneNodeStates, avs::uid node_uid)
{
	TELEPORT_COUT << "RemoveNodeFromInstanceRender: " << cache_uid << ", " << node_uid << "\n";
	auto g = GeometryCache::GetGeometryCache(cache_uid);

	if (!g)
	{
		return;
	}
	auto node = g->mNodeManager.GetNode(node_uid);
	if (!node)
	{
		return;
	}

	node->ResetCachedPasses();
	auto n = subSceneNodeStates.nodeStates.find(node_uid);
	if (n != subSceneNodeStates.nodeStates.end())
	{
		for (int i = 0; i < n->second.elementStates.size(); i++)
		{
			std::shared_ptr<MaterialRender> m = n->second.elementStates[i].materialRender.lock();
			if (m)
			{
				m->meshRenders.erase(n->second.elementStates[i].hash);
				auto instancedRenderer = m->instancedRenders.find(n->second.elementStates[i].hash);
				if (instancedRenderer != m->instancedRenders.end())
				{
					uint64_t node_instance_hash = MakeNodeElementHash(subSceneNodeStates.root_id, node->id, cache_uid, i);
					instancedRenderer->second->meshInstanceRenders.erase(node_instance_hash);
					if (instancedRenderer->second->meshInstanceRenders.size() == 0)
					{
						m->instancedRenders.erase(instancedRenderer);
					}
				}
			}
		}
		subSceneNodeStates.nodeStates.erase(node_uid);
	}
	if (!node)
	{
		return;
	}
	uint64_t node_hash = MakeNodeHash(subSceneNodeStates.root_id, cache_uid, node_uid);
	auto	 l		   = linkRenders.find(node_hash);
	if (l != linkRenders.end())
	{
		linkRenders.erase(l);
	}
	auto c = canvasRenders.find(node_hash);
	if (c != canvasRenders.end())
	{
		canvasRenders.erase(c);
	}
	const std::shared_ptr<clientrender::Mesh> mesh = node->GetMesh();
	if (mesh)
	{
	}
	else
	{
		auto s = node->GetComponent<clientrender::SubSceneComponent>();
		if (s)
		{
			if (s->mesh_uid)
			{
				auto ss = g->mMeshManager.Get(s->mesh_uid);
				if (ss)
				{
					avs::uid subscene_cache_uid = ss->GetMeshCreateInfo().subscene_cache_uid;
					// We remove all render states for the subScene for this node alone.
					auto g						= GeometryCache::GetGeometryCache(subscene_cache_uid);
					if (subscene_cache_uid && g)
					{
						SubSceneNodeStates &subSceneNodeStates = subSceneStatesMap[node_uid];
						subSceneNodeStates.root_id			   = node->id;
						RemoveGeometryCacheFromInstanceRender(subscene_cache_uid, subSceneNodeStates);
					}
				}
			}
		}
	}
}

void InstanceRenderer::UpdateNodeInInstanceRender(avs::uid cache_uid, avs::uid root_id, avs::uid node_uid)
{
	TELEPORT_COUT << "UpdateNodeInInstanceRender: " << cache_uid << ", " << node_uid << "\n";
	auto g = GeometryCache::GetGeometryCache(cache_uid);
	if (!g)
	{
		return;
	}
	auto node = g->mNodeManager.GetNode(node_uid);
	if (!node)
	{
		return;
	}
	const std::shared_ptr<clientrender::Mesh> mesh		 = node->GetMesh();
	const std::shared_ptr<TextCanvas>		  textCanvas = node->GetTextCanvas();
	if (mesh)
	{
		const auto &meshInfo = mesh->GetMeshCreateInfo();
		// iterate through the submeshes.
		auto &materials		 = node->GetMaterials();
		bool  rezzing		 = false;
		for (uint32_t element = 0; element < meshInfo.indexBuffers.size(); element++)
		{
			const clientrender::PassCache *passCache = node->GetCachedEffectPass(element);
			crossplatform::EffectPass	  *pass		 = passCache ? passCache->pass : nullptr;
			auto						   p		 = passRenders.find(pass);
			if (p == passRenders.end())
			{
				continue;
			}
			PassRender &passRender	 = *p->second.get();
			avs::uid	material_uid = node->GetMaterial(element)->id;
			auto		m			 = passRender.materialRenders.find(material_uid);
			if (m == passRender.materialRenders.end())
			{
				continue;
			}
			MaterialRender *materialRender = m->second.get();
			uint64_t		instance_hash  = InstanceHash(cache_uid, mesh.get(), node.get(), element);
			auto			ir			   = materialRender->instancedRenders.find(instance_hash);
			if (ir != materialRender->instancedRenders.end())
			{
				InstancedRender *instancedRender	= ir->second.get();
				uint64_t		 node_instance_hash = MakeNodeElementHash(root_id, node->id, cache_uid, element);
				auto			 mir				= instancedRender->meshInstanceRenders.find(node_instance_hash);
				if (mir != instancedRender->meshInstanceRenders.end())
				{
					instancedRender->meshInstanceRenders.erase(node_instance_hash);
					instancedRender->valid = false;
				}
			}
			uint64_t node_element_hash = MakeNodeElementHash(root_id, node->id, cache_uid, element);
			auto	 mr				   = materialRender->meshRenders.find(node_element_hash);
			m->second->meshRenders.erase(node_element_hash);
			if (m->second->meshRenders.size() == 0)
			{
				p->second->materialRenders.erase(material_uid);
			}
		}
	}
}

void InstanceRenderer::UpdateGeometryCacheForRendering(platform::crossplatform::GraphicsDeviceContext &deviceContext,
													   SubSceneNodeStates							  &subSceneNodeStates,
													   std::shared_ptr<clientrender::GeometryCache>	   g)
{
	const std::vector<std::weak_ptr<Node>> &nodeList = g->mNodeManager.GetSortedRootNodes();
	for (size_t i = 0; i < nodeList.size(); i++)
	{
		std::shared_ptr<clientrender::Node> node = nodeList[i].lock();
		if (!node)
		{
			continue;
		}
		if (renderState.show_only != 0 && renderState.show_only != node->id)
		{
			continue;
		}
		UpdateNodeForRendering(deviceContext, g, subSceneNodeStates, node, true, false);
	}
	const std::vector<std::weak_ptr<clientrender::Node>> &transparentList = g->mNodeManager.GetSortedTransparentNodes();
	for (size_t i = 0; i < transparentList.size(); i++)
	{
		const std::shared_ptr<clientrender::Node> node = transparentList[i].lock();
		if (!node)
		{
			continue;
		}
		if (renderState.show_only != 0 && renderState.show_only != node->id)
		{
			continue;
		}
		UpdateNodeForRendering(deviceContext, g, subSceneNodeStates, node, false, true);
	}
}

//[thread=RenderThread]
void InstanceRenderer::UpdateNodeForRendering(crossplatform::GraphicsDeviceContext				 &deviceContext,
											  const std::shared_ptr<clientrender::GeometryCache> &geometrySubCache,
											  SubSceneNodeStates								 &subSceneNodeStates,
											  const std::shared_ptr<clientrender::Node>			  node,
											  bool												  include_children,
											  bool												  transparent_pass)
{
	if (!node->IsVisible())
	{
		return;
	}
	const std::shared_ptr<clientrender::Mesh> mesh		 = node->GetMesh();
	const std::shared_ptr<TextCanvas>		  textCanvas = transparent_pass ? node->GetTextCanvas() : nullptr;
	// Q: has this node been added to the render caches for this instance?
	if (subSceneNodeStates.nodeStates.find(node->id) == subSceneNodeStates.nodeStates.end())
	{
		AddNodeToInstanceRender(geometrySubCache->GetCacheUid(), subSceneNodeStates, node->id);
	}

	if (std::shared_ptr<clientrender::Skeleton> skeleton = node->GetSkeleton())
	{
		auto &nodeState			= subSceneNodeStates.nodeStates[node->id];

		// The bone matrices transform from the original local position of a vertex
		//								to its current animated local position.
		// For each bone matrix, pos_local= (bone_matrix_j) * pos_original_local
		auto animationComponent = node->GetComponent<AnimationComponent>();
		// static size_t match_joint_count=22;
		if (animationComponent && animationComponent->update(renderState.timestampUs.count(), subSceneNodeStates.root_id))
		{
			// We want to update the instance of this animation component associated with this instance of the submesh.
		}

		// Here we must update the matrix that corresponds to the parent cache's instance of this node.
		// i.e. if the parent cache owns the node direct, it's the node's own renderableState.
		// But if we're rendering a subscene, we need to have a renderableState for the combo of parent cache and node.
		nodeState.renderModelMatrix = *((const mat4 *)(&deviceContext.viewStruct.model));
	}
	else if (mesh || textCanvas || node->GetURL().length() > 0)
	{
		auto	   &nodeState			  = subSceneNodeStates.nodeStates[node->id];
		const mat4 &globalTransformMatrix = node->GetGlobalTransform().GetTransformMatrix();
		nodeState.renderModelMatrix		  = mul(*((const mat4 *)(&deviceContext.viewStruct.model)), globalTransformMatrix);
		if (mesh)
		{
			const auto &jointIndices = node->GetJointIndices();
			if (jointIndices.size())
			{
				auto skelNode = node->GetSkeletonNode().lock();
				if (skelNode)
				{
					auto			  animationComponent = skelNode->GetComponent<AnimationComponent>();
					std::vector<mat4> boneMatrices;
					for (int i = 0; i < boneMatrices.size(); i++)
					{
						boneMatrices[i] = mat4::identity();
					}
					skeleton			   = skelNode->GetSkeleton();
					auto animationInstance = animationComponent->GetOrCreateAnimationInstance(subSceneNodeStates.root_id);
					animationInstance->GetBoneMatrices(
						boneMatrices, node->GetJointIndices(), node->GetInverseBindMatrices(), skeleton->GetSkeletonToAnimMapping());
					avs::uid sk_id = node->id;
					if (skeletonRenders.find(sk_id) == skeletonRenders.end())
					{
						skeletonRenders[sk_id] = std::make_shared<SkeletonRender>();
						skeletonRenders[sk_id]->boneMatrices.RestoreDeviceObjects(renderPlatform);
					}
					BoneMatrices *b = &skeletonRenders[sk_id]->boneMatrices;
					memcpy(b, boneMatrices.data(), sizeof(mat4) * boneMatrices.size());
				}
			}
		}
	}

	if (!include_children)
	{
		return;
	}
	const auto &children = node->GetChildren();
	for (std::weak_ptr<clientrender::Node> childPtr : children)
	{
		std::shared_ptr<clientrender::Node> child = childPtr.lock();
		if (child)
		{
			UpdateNodeForRendering(deviceContext, geometrySubCache, subSceneNodeStates, child, include_children, transparent_pass);
		}
	}
	// what about subscenes?
	auto s = node->GetComponent<clientrender::SubSceneComponent>();
	if (s)
	{
		if (s->mesh_uid)
		{
			auto ss = geometrySubCache->mMeshManager.Get(s->mesh_uid);
			if (ss)
			{
				if (ss->GetMeshCreateInfo().subscene_cache_uid)
				{
					auto g = GeometryCache::GetGeometryCache(ss->GetMeshCreateInfo().subscene_cache_uid);
					if (g)
					{
						auto oldview	= deviceContext.viewStruct.view;
						// transform the view matrix by the local space.
						mat4 node_model = node->GetGlobalTransform().GetTransformMatrix();
						deviceContext.viewStruct.PushModelMatrix(*((platform::math::Matrix4x4 *)&node_model));

						SubSceneNodeStates &subSceneNodeStates = subSceneStatesMap[node->id];
						subSceneNodeStates.root_id			   = node->id;
						UpdateGeometryCacheForRendering(deviceContext, subSceneNodeStates, g);
						deviceContext.viewStruct.PopModelMatrix();
					}
				}
			}
		}
	}
}

void InstanceRenderer::ApplyModelMatrix(crossplatform::GraphicsDeviceContext &deviceContext, const mat4 &model)
{
	renderState.perNodeConstants.model = model;
	renderState.perNodeConstants.SetHasChanged();
}

void InstanceRenderer::RenderMaterial(crossplatform::GraphicsDeviceContext &deviceContext, const MaterialRender &materialRender)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, materialRender.material->getName().c_str());
	ApplyMaterialConstants(deviceContext, materialRender.material);
	for (auto r : materialRender.meshRenders)
	{
		RenderMesh(deviceContext, *(r.second.get()));
	}
	for (auto i : materialRender.instancedRenders)
	{
		RenderInstancedMeshes(deviceContext, *(i.second.get()));
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext);
}

void InstanceRenderer::RenderMesh(crossplatform::GraphicsDeviceContext &deviceContext, const MeshRender &meshRender)
{
	if (!meshRender.model)
	{
		return;
	}
	ApplyModelMatrix(deviceContext, *meshRender.model);
	const auto &meshInfo = meshRender.mesh->GetMeshCreateInfo();
	if (meshRender.setBoneConstantBuffer)
	{
		auto sk = meshRender.skeleton.lock();
		if (!sk)
		{
			return;
		}
		avs::uid sk_id = meshRender.node->id;
		if (skeletonRenders.find(sk_id) == skeletonRenders.end())
		{
			return;
		}
		renderPlatform->SetConstantBuffer(deviceContext, &(skeletonRenders[sk_id]->boneMatrices));
	}
	const clientrender::Material::MaterialCreateInfo &matInfo = meshRender.material->GetMaterialCreateInfo();

	if (renderStateTracker.gi_texture_id != meshRender.gi_texture_id)
	{
		const auto geometrySubCache = GeometryCache::GetGeometryCache(meshRender.cache_uid);
		if (geometrySubCache)
		{
			renderStateTracker.gi_texture_id								 = meshRender.gi_texture_id;
			std::shared_ptr<clientrender::Texture> globalIlluminationTexture = geometrySubCache->mTextureManager.Get(meshRender.gi_texture_id);
			renderStateTracker.globalIlluminationTexture = globalIlluminationTexture ? globalIlluminationTexture->GetSimulTexture() : nullptr;
			renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_globalIlluminationTexture, renderStateTracker.globalIlluminationTexture);
			renderPlatform->ApplyResourceGroup(deviceContext, 1);
		}
	}
	renderState.perNodeConstants.lightmapScaleOffset = *(const vec4 *)(&(meshRender.node->GetLightmapScaleOffset()));
	renderState.perNodeConstants.rezzing			 = meshRender.node->countdown;
	renderPlatform->SetConstantBuffer(deviceContext, &renderState.perNodeConstants);
	if (matInfo.doubleSided)
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_DOUBLE_SIDED);
	}
	else if (meshRender.clockwise)
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_FRONTFACE_CLOCKWISE);
	}
	else
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_FRONTFACE_COUNTERCLOCKWISE);
	}
	auto *vb = meshInfo.vertexBuffers[meshRender.element].get();
	if (!vb)
	{
		return;
	}
	const auto *ib = meshInfo.indexBuffers[meshRender.element].get();
	if (!ib)
	{
		return;
	}

	const crossplatform::Buffer *const v[] = {vb->GetSimulVertexBuffer()};
	if (!v[0])
	{
		return;
	}
	SIMUL_COMBINED_PROFILE_START(deviceContext, meshInfo.name.c_str());
	renderPlatform->SetVertexBuffers(deviceContext, 0, 1, v, nullptr);
	renderPlatform->SetIndexBuffer(deviceContext, ib->GetSimulIndexBuffer());
	renderPlatform->DrawIndexed(deviceContext, (int)ib->GetIndexBufferCreateInfo().indexCount, 0, 0);
	SIMUL_COMBINED_PROFILE_END(deviceContext);
}

void InstanceRenderer::RenderInstancedMeshes(crossplatform::GraphicsDeviceContext &deviceContext, const InstancedRender &instancedRender)
{
	if (!instancedRender.meshInstanceRenders.size())
	{
		return;
	}
	const auto										 &meshInfo = instancedRender.mesh->GetMeshCreateInfo();
	const clientrender::Material::MaterialCreateInfo &matInfo  = instancedRender.material->GetMaterialCreateInfo();

	if (renderStateTracker.gi_texture_id != instancedRender.gi_texture_id)
	{
		const auto geometrySubCache = GeometryCache::GetGeometryCache(instancedRender.cache_uid);
		if (geometrySubCache)
		{
			renderStateTracker.gi_texture_id								 = instancedRender.gi_texture_id;
			std::shared_ptr<clientrender::Texture> globalIlluminationTexture = geometrySubCache->mTextureManager.Get(instancedRender.gi_texture_id);
			renderStateTracker.globalIlluminationTexture = globalIlluminationTexture ? globalIlluminationTexture->GetSimulTexture() : nullptr;
			renderPlatform->SetTexture(deviceContext, renderState.pbrEffect_globalIlluminationTexture, renderStateTracker.globalIlluminationTexture);
			renderPlatform->ApplyResourceGroup(deviceContext, 1);
		}
	}
	renderPlatform->SetConstantBuffer(deviceContext, &renderState.perNodeConstants);
	if (matInfo.doubleSided)
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_DOUBLE_SIDED);
	}
	else if (instancedRender.clockwise)
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_FRONTFACE_CLOCKWISE);
	}
	else
	{
		renderPlatform->SetStandardRenderState(deviceContext, crossplatform::StandardRenderState::STANDARD_FRONTFACE_COUNTERCLOCKWISE);
	}
	auto *vb = meshInfo.vertexBuffers[instancedRender.element].get();
	if (!vb)
	{
		return;
	}
	const auto *ib = meshInfo.indexBuffers[instancedRender.element].get();
	if (!ib)
	{
		return;
	}
	// Fill instance buffer if it has been modified.
	if (!instancedRender.valid)
	{
		struct InstanceData
		{
			vec4 lightmapScaleOffset;
			mat4 model;
		};
		InstanceData *inst = (InstanceData *)(instancedRender.instanceBuffer->Map(deviceContext));
		if (!inst)
		{
			return;
		}
		int i = 0;
		for (auto &m : instancedRender.meshInstanceRenders)
		{
			inst[i].lightmapScaleOffset = m.second.node->GetLightmapScaleOffset();
			inst[i].model				= *(m.second.model);
			// inst[i].model.transpose();
			i++;
		}
		instancedRender.instanceBuffer->Unmap(deviceContext);
		instancedRender.valid = true;
	}
	const crossplatform::Buffer *const v[] = {vb->GetSimulVertexBuffer(), instancedRender.instanceBuffer.get()};
	if (!v[0])
	{
		return;
	}
	SIMUL_COMBINED_PROFILE_START(deviceContext, meshInfo.name.c_str());
	renderPlatform->SetVertexBuffers(deviceContext, 0, 2, v, nullptr);
	renderPlatform->SetIndexBuffer(deviceContext, ib->GetSimulIndexBuffer());
	renderPlatform->DrawIndexedInstanced(
		deviceContext, (int)instancedRender.meshInstanceRenders.size(), 0, (int)ib->GetIndexBufferCreateInfo().indexCount, 0, 0);
	SIMUL_COMBINED_PROFILE_END(deviceContext);
	renderPlatform->SetVertexBuffers(deviceContext, 1, 1, nullptr, nullptr);
}

void InstanceRenderer::RenderTextCanvas(crossplatform::GraphicsDeviceContext &deviceContext, const CanvasRender *canvasRender)
{
	ApplyModelMatrix(deviceContext, *canvasRender->model);
	renderPlatform->SetConstantBuffer(deviceContext, &renderState.perNodeConstants);
	if (!renderState.commonFontAtlas)
	{
		renderState.commonFontAtlas = canvasRender->textCanvas->fontAtlas;
	}
	renderState.canvasTextRenderer.Render(deviceContext, canvasRender);
}

void InstanceRenderer::RenderNodeOverlay(crossplatform::GraphicsDeviceContext				&deviceContext,
										 const std::shared_ptr<clientrender::GeometryCache> &geometrySubCache,
										 SubSceneNodeStates									&subSceneNodeStates,
										 const std::shared_ptr<clientrender::Node>			 node,
										 bool												 include_children)
{
	if (!geometrySubCache)
	{
		return;
	}
	if (!node)
	{
		return;
	}
	auto &nodeState = subSceneNodeStates.nodeStates[node->id];
	ApplyModelMatrix(deviceContext, nodeState.renderModelMatrix);
	auto	 renderPlatform = deviceContext.renderPlatform;

	avs::uid node_select	= renderState.selected_uid;

	{
		const std::shared_ptr<clientrender::Mesh> mesh					= node->GetMesh();
		const auto								  anim					= node->GetComponent<clientrender::AnimationComponent>();
		vec3									  pos					= node->GetGlobalPosition();
		mat4									  globalTransformMatrix = node->GetGlobalTransform().GetTransformMatrix();
		mat4									  m						= mul(*((const mat4 *)(&deviceContext.viewStruct.model)), globalTransformMatrix);
		static float							  sz					= 0.1f;
		renderPlatform->DrawAxes(deviceContext, m, sz);

		static std::string str;
		vec4			   white(1.0f, 1.0f, 1.0f, 1.0f);
		vec4			   bkg		= {0, 0, 0, 0.5f};
		auto			   skeleton = node->GetSkeleton();
		if (skeleton.get())
		{
			str = "";
			/*auto		animC				 = node->GetOrCreateComponent<AnimationComponent>();
			const auto &animationLayerStates = animC->GetAnimationLayerStates();
			/if (animationLayerStates.size())
			{
				// const clientrender::AnimationStateMap &animationStates= node->animationComponent.GetAnimationStates();
				static char txt[250];
				for (const auto &s : animationLayerStates)
				{
					const auto &a = s.getState();
					str += fmt::format("{0} anim {1}\n", node->id, a.animationState.animationId);
				}
				renderPlatform->PrintAt3dPos(deviceContext, (const float *)(&pos), str.c_str(), (const float *)(&white), bkg);
			}*/
		}
		else if (mesh)
		{
			str = fmt::format("{0} {1}: {2}", node->id, node->name.c_str(), mesh->GetMeshCreateInfo().name.c_str());
			renderPlatform->PrintAt3dPos(deviceContext, (const float *)(&pos), str.c_str(), (const float *)(&white), bkg, 0, 0, false);
		}
		else
		{
			vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
			str = fmt::format("{0} {1}", node->id, node->name.c_str());
			renderPlatform->PrintAt3dPos(deviceContext, (const float *)(&pos), str.c_str(), (const float *)(&yellow), bkg, 0, 0, false);
		}
	}
	if (!include_children)
	{
		return;
	}
	const auto &children = node->GetChildren();
	for (std::weak_ptr<clientrender::Node> childPtr : children)
	{
		std::shared_ptr<clientrender::Node> child = childPtr.lock();
		if (child)
		{
			RenderNodeOverlay(deviceContext, geometrySubCache, subSceneNodeStates, child, include_children);
		}
	}
}

bool InstanceRenderer::OnNodeEnteredBounds(avs::uid id)
{
	return geometryCache->mNodeManager.ShowNode(id);
}

bool InstanceRenderer::OnNodeLeftBounds(avs::uid id)
{
	return geometryCache->mNodeManager.HideNode(id);
}

void InstanceRenderer::UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &cmd)
{
	geometryCache->mNodeManager.ReparentNode(cmd);
}

void InstanceRenderer::AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &cmd, const std::string &regexPath)
{
	renderState.openXR->MapNodeToPose(server_uid, cmd.nodeID, regexPath);
}

void InstanceRenderer::SetVisibleNodes(const std::vector<avs::uid> &visibleNodes)
{
	geometryCache->mNodeManager.SetVisibleNodes(visibleNodes);
}

void InstanceRenderer::UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate> &updateList)
{
	geometryCache->mNodeManager.UpdateNodeMovement(updateList);
}

void InstanceRenderer::UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState> &updateList)
{
	geometryCache->mNodeManager.UpdateNodeEnabledState(updateList);
}

void InstanceRenderer::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	geometryCache->mNodeManager.SetNodeHighlighted(nodeID, isHighlighted);
}

void InstanceRenderer::UpdateNodeAnimation(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &animationUpdate)
{
	static uint8_t ctr = 1;
	ctr--;
	if (!ctr)
	{
		std::shared_ptr<Node> node = geometryCache->mNodeManager.GetNode(animationUpdate.nodeID);
		if (node)
		{
			TELEPORT_COUT << "Animation: node " << animationUpdate.nodeID << " " << node->name << ", animation " << animationUpdate.animationID
						  << ", timestamp " << animationUpdate.timestampUs << "\n";
		}
	}
	geometryCache->mNodeManager.UpdateNodeAnimation(timestampUs, animationUpdate);
}

void InstanceRenderer::UpdateTagDataBuffers(crossplatform::GraphicsDeviceContext &deviceContext)
{
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto										&cachedLights = geometryCache->mLightManager.GetCache(cacheLock);
		for (int i = 0; i < videoTagDataCubeArray.size(); ++i)
		{
			const auto &td							= videoTagDataCubeArray[i];
			const auto &pos							= td.coreData.cameraTransform.position;
			const auto &rot							= td.coreData.cameraTransform.rotation;

			videoTagDataCube[i].cameraPosition		= {pos.x, pos.y, pos.z};
			videoTagDataCube[i].cameraRotation		= {rot.x, rot.y, rot.z, rot.w};
			videoTagDataCube[i].diffuseAmbientScale = td.coreData.diffuseAmbientScale;
			videoTagDataCube[i].lightCount			= static_cast<int>(td.lights.size());
			if (td.lights.size() > 10)
			{
				TELEPORT_CERR_BREAK("Too many lights in tag.", 10);
			}
			for (int j = 0; j < td.lights.size() && j < 10; j++)
			{
				LightTag						 &t = videoTagDataCube[i].lightTags[j];
				const clientrender::LightTagData &l = td.lights[j];
				t.uid32								= (unsigned)(((uint64_t)0xFFFFFFFF) & l.uid);
				t.colour							= ConvertVec4<vec4>(l.color);
				// Convert from +-1 to [0,1]
				t.shadowTexCoordOffset.x			= float(l.texturePosition[0]) / float(sessionClient->GetSetupCommand().video_config.video_width);
				t.shadowTexCoordOffset.y			= float(l.texturePosition[1]) / float(sessionClient->GetSetupCommand().video_config.video_height);
				t.shadowTexCoordScale.x				= float(l.textureSize) / float(sessionClient->GetSetupCommand().video_config.video_width);
				t.shadowTexCoordScale.y				= float(l.textureSize) / float(sessionClient->GetSetupCommand().video_config.video_height);
				// Tag data has been properly transformed in advance:
				vec3 position						= l.position;
				vec4 orientation					= l.orientation;
				t.position							= *((vec3 *)&position);
				crossplatform::Quaternionf q((const float *)&orientation);
				t.direction			  = q * vec3(0, 0, 1.0f);
				t.worldToShadowMatrix = ConvertMat4(l.worldToShadowMatrix);

				auto nodeLight		  = cachedLights.find(l.uid);
				if (nodeLight != cachedLights.end() && nodeLight->second.resource != nullptr)
				{
					const clientrender::Light::LightCreateInfo &lc = nodeLight->second.resource->GetLightCreateInfo();
					t.is_point									   = float(lc.type != clientrender::Light::Type::DIRECTIONAL);
					t.is_spot									   = float(lc.type == clientrender::Light::Type::SPOT);
					t.radius									   = lc.lightRadius;
					t.range										   = lc.lightRange;
					t.shadow_strength							   = 0.0f;
				}
			}
		}
		renderState.tagDataCubeBuffer.SetData(deviceContext, videoTagDataCube);
	}
}

void InstanceRenderer::OnReceiveVideoTagData(const uint8_t *data, size_t dataSize)
{
	clientrender::SceneCaptureCubeTagData tagData;
	memcpy(&tagData.coreData, data, sizeof(clientrender::SceneCaptureCubeCoreTagData));
	avs::ConvertTransform(sessionClient->GetSetupCommand().axesStandard, avs::AxesStandard::EngineeringStyle, tagData.coreData.cameraTransform);

	tagData.lights.resize(tagData.coreData.lightCount);

	// We will check the received light tags agains the current list of lights - rough and temporary.
	/*
	Roderick: we will here ignore the cached lights (CPU-streamed node lights) as they are unordered so may be found in a different order
		to the tag lights. ALL light data will go into the tags, using uid lookup to get any needed data from the unordered cache.
	std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
	auto &cachedLights=geometryCache->mLightManager.GetCache(cacheLock);
	auto &cachedLight=cachedLights.begin();*/
	////

	size_t index = sizeof(clientrender::SceneCaptureCubeCoreTagData);
	for (auto &light : tagData.lights)
	{
		memcpy(&light, &data[index], sizeof(clientrender::LightTagData));
		// avs::ConvertTransform(renderState.GetSetupCommand().axesStandard, avs::AxesStandard::EngineeringStyle, light.worldTransform);
		index += sizeof(clientrender::LightTagData);
	}
	if (tagData.coreData.id >= videoTagDataCubeArray.size())
	{
		TELEPORT_CERR_BREAK("Bad tag id", 1);
		return;
	}
	videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
}

std::vector<avs::uid> InstanceRenderer::GetGeometryResources()
{
	return geometryCache->GetAllResourceIDs();
}

// This is called when we connect to a new server.
void InstanceRenderer::ClearGeometryResources()
{
	geometryCache->ClearAll();
	renderState.openXR->ResetServer(server_uid);
}

void InstanceRenderer::OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions_)
{
	sessionClient->SetInputDefinitions(inputDefinitions_);
	if (renderState.openXR)
	{
		renderState.openXR->OnInputsSetupChanged(server_uid, inputDefinitions_);
	}
}

void InstanceRenderer::SetOrigin(unsigned long long ctr, avs::uid origin_uid)
{
	auto &clientServerState			  = sessionClient->GetClientServerState();
	clientServerState.origin_node_uid = origin_uid;
	receivedInitialPos				  = ctr;
}

bool InstanceRenderer::OnSetupCommandReceived(const char *server_ip, const teleport::core::SetupCommand &setupCommand)
{
	videoPosDecoded = false;

	videoTagDataCubeArray.clear();
	videoTagDataCubeArray.resize(RenderState::maxTagDataSize);

	AVSTextureImpl *ti = (AVSTextureImpl *)(instanceRenderState.avsTexture.get());
	if (ti)
	{
		SAFE_DELETE(ti->texture);
	}

	/* Now for each stream, we add both a DECODER and a SURFACE node. e.g. for two streams:
					 /->decoder -> surface
			source -<
					 \->decoder	-> surface
	*/
	size_t											   stream_width	 = setupCommand.video_config.video_width;
	size_t											   stream_height = setupCommand.video_config.video_height;

	std::shared_ptr<std::vector<std::vector<uint8_t>>> empty_data;
	if (setupCommand.video_config.use_cubemap)
	{
		if (setupCommand.video_config.colour_cubemap_size)
		{
			instanceRenderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																			  setupCommand.video_config.colour_cubemap_size,
																			  setupCommand.video_config.colour_cubemap_size,
																			  1,
																			  1,
																			  crossplatform::PixelFormat::RGBA_16_FLOAT,
																			  empty_data,
																			  true,
																			  false,
																			  false,
																			  true);
		}
	}
	else
	{
		if (setupCommand.video_config.perspective_width * setupCommand.video_config.perspective_height > 0)
		{
			instanceRenderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																			  setupCommand.video_config.perspective_width,
																			  setupCommand.video_config.perspective_height,
																			  1,
																			  1,
																			  crossplatform::PixelFormat::RGBA_16_FLOAT,
																			  empty_data,
																			  true,
																			  false,
																			  false,
																			  false);
		}
	}
	const auto &dynamicLighting = sessionClient->GetDynamicLighting();
	if (dynamicLighting.specularCubemapSize > 0)
	{
		instanceRenderState.specularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																					dynamicLighting.specularCubemapSize,
																					dynamicLighting.specularCubemapSize,
																					1,
																					dynamicLighting.specularMips,
																					crossplatform::PixelFormat::RGBA_8_UNORM,
																					empty_data,
																					true,
																					false,
																					false,
																					true);
	}
	if (dynamicLighting.diffuseCubemapSize > 0)
	{
		instanceRenderState.diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																				   dynamicLighting.diffuseCubemapSize,
																				   dynamicLighting.diffuseCubemapSize,
																				   1,
																				   1,
																				   crossplatform::PixelFormat::RGBA_8_UNORM,
																				   empty_data,
																				   true,
																				   false,
																				   false,
																				   true);
	}

	const float aspect						= setupCommand.video_config.perspective_width / static_cast<float>(setupCommand.video_config.perspective_height);
	const float horzFOV						= setupCommand.video_config.perspective_fov * clientrender::DEG_TO_RAD;
	const float vertFOV						= clientrender::GetVerticalFOVFromHorizontal(horzFOV, aspect);

	renderState.cubemapConstants.serverProj = crossplatform::Camera::MakeDepthReversedProjectionMatrix(horzFOV, vertFOV, 0.01f, 0);

	colourOffsetScale.x						= 0;
	colourOffsetScale.y						= 0;
	colourOffsetScale.z						= 1.0f;
	colourOffsetScale.w						= float(setupCommand.video_config.video_height) / float(stream_height);

	CreateTexture(renderPlatform, instanceRenderState.avsTexture, int(stream_width), int(stream_height));

	auto &clientPipeline = sessionClient->GetClientPipeline();
// Set to a custom backend that uses platform api video decoder if using D3D12 and non NVidia card.
#if TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER
	clientPipeline.decoder.setBackend(CreateVideoDecoder());
#endif

	avs::DeviceHandle dev;

#if TELEPORT_CLIENT_USE_D3D12
	dev.handle = renderPlatform->AsD3D12Device();
	dev.type   = avs::DeviceType::Direct3D12;
#elif TELEPORT_CLIENT_USE_D3D11
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type   = avs::DeviceType::Direct3D11;
#else
	dev.handle = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	dev.type   = avs::DeviceType::Vulkan;
#endif
	// Video streams are 0+...
	bool got_video = false;
	if (clientPipeline.decoder.configure(dev, (int)stream_width, (int)stream_height, clientPipeline.decoderParams, 20))
	{
		got_video = true;
	}
	if (got_video)
	{
		if (!clientPipeline.surface.configure(instanceRenderState.avsTexture->createSurface()))
		{
			TELEPORT_WARN("Failed to configure output surface node!\n");
		}
		clientPipeline.videoQueue.configure(300000, 16, "VideoQueue");

		avs::PipelineNode::link(*(clientPipeline.source.get()), clientPipeline.videoQueue);
		avs::PipelineNode::link(clientPipeline.videoQueue, clientPipeline.decoder);
		clientPipeline.pipeline.link({&clientPipeline.decoder, &clientPipeline.surface});

		// Tag Data
		{
			auto f = std::bind(&InstanceRenderer::OnReceiveVideoTagData, this, std::placeholders::_1, std::placeholders::_2);
			if (!clientPipeline.tagDataDecoder.configure(40, f))
			{
				TELEPORT_CERR << "Failed to configure video tag data decoder node!\n";
			}

			clientPipeline.tagDataQueue.configure(200, 16, "VideoTagQueue");

			avs::PipelineNode::link(*(clientPipeline.source.get()), clientPipeline.tagDataQueue);
			clientPipeline.pipeline.link({&clientPipeline.tagDataQueue, &clientPipeline.tagDataDecoder});
		}
	}
	// Audio
	{
		clientPipeline.avsAudioDecoder.configure(60);
		audio::AudioSettings audioSettings;
		audioSettings.codec			= audio::AudioCodec::PCM;
		audioSettings.numChannels	= 1;
		audioSettings.sampleRate	= 48000;
		audioSettings.bitsPerSample = 32;
		audioPlayer.configure(audioSettings);
		audioStreamTarget.reset(new audio::AudioStreamTarget(&audioPlayer));
		clientPipeline.avsAudioTarget.configure(audioStreamTarget.get());

		clientPipeline.audioQueue.configure(4096, 120, "AudioQueue");

		avs::PipelineNode::link(*(clientPipeline.source.get()), clientPipeline.audioQueue);
		avs::PipelineNode::link(clientPipeline.audioQueue, clientPipeline.avsAudioDecoder);
		clientPipeline.pipeline.link({&clientPipeline.avsAudioDecoder, &clientPipeline.avsAudioTarget});

		// Audio Input
		if (setupCommand.audio_input_enabled)
		{
			int					   framerate	   = 60;
			uint32_t			   udpBufferSize   = static_cast<uint32_t>(clientPipeline.source->getSystemBufferSize());
			int					   maxBandwidthKpS = udpBufferSize * framerate;
			audio::NetworkSettings networkSettings = {static_cast<int32_t>(maxBandwidthKpS),
													  static_cast<int32_t>(udpBufferSize),
													  setupCommand.requiredLatencyMs,
													  (int32_t)setupCommand.idle_connection_timeout};

			audioInputNetworkPipeline.reset(new audio::NetworkPipeline());
			audioInputQueue.configure(4096, 120, "AudioInputQueue");
			audioInputNetworkPipeline->initialise(networkSettings, &audioInputQueue);

			// The callback will be called when audio input is received.
			auto f = [this](const uint8_t *data, size_t dataSize) -> void
			{
				size_t bytesWritten;
				if (audioInputQueue.write(nullptr, data, dataSize, bytesWritten))
				{
					audioInputNetworkPipeline->process();
				}
			};
			// The audio player will stop recording automatically when deconfigured.
			audioPlayer.startRecording(f);
		}
	}
	auto &resourceCreator = ResourceCreator::GetInstance();

	// We will add a GEOMETRY PIPE
	{
		clientPipeline.avsGeometryDecoder.configure(80, server_uid, &geometryDecoder);
		clientPipeline.avsGeometryTarget.configure(&resourceCreator);
		// TODO: should not really be handling chunks of this size through queues.
		clientPipeline.geometryQueue.configure(16000000, 20, "GeometryQueue");

		avs::PipelineNode::link(*(clientPipeline.source.get()), clientPipeline.geometryQueue);
		avs::PipelineNode::link(clientPipeline.geometryQueue, clientPipeline.avsGeometryDecoder);
		clientPipeline.pipeline.link({&clientPipeline.avsGeometryDecoder, &clientPipeline.avsGeometryTarget});
	}
	{
		clientPipeline.reliableOutQueue.configure(3000 * 64, "Reliable out");
		clientPipeline.commandDecoder.configure(sessionClient, "Reliable Decoder");
		avs::PipelineNode::link(*(clientPipeline.source.get()), clientPipeline.reliableOutQueue);
		clientPipeline.pipeline.link({&clientPipeline.reliableOutQueue, &clientPipeline.commandDecoder});
	}
	// And the generic queue for messages TO the server:
	{
		clientPipeline.unreliableToServerQueue.configure(3000 * 64, "Unreliable in");
		avs::PipelineNode::link(clientPipeline.unreliableToServerQueue, *(clientPipeline.source.get()));
	}
	// Tow special-purpose queues for time-sensitive messages TO the server:
	{
		// TODO: better default buffer sizes, esp input state buffer.
		clientPipeline.nodePosesQueue.configure(3000, "Unreliable in");
		clientPipeline.inputStateQueue.configure(3000, "Unreliable in");
		// Both connect to the source as inputs, and both feed directly to the "unreliable in" stream.
		avs::PipelineNode::link(clientPipeline.nodePosesQueue, *(clientPipeline.source.get()));
		avs::PipelineNode::link(clientPipeline.inputStateQueue, *(clientPipeline.source.get()));
	}

	// java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
	return true;
}
bool InstanceRenderer::GetHandshake(teleport::core::Handshake &handshake)
{
	handshake.startDisplayInfo.width  = renderState.hdrFramebuffer->GetWidth();
	handshake.startDisplayInfo.height = renderState.hdrFramebuffer->GetHeight();
	handshake.axesStandard			  = avs::AxesStandard::EngineeringStyle;
	handshake.MetresPerUnit			  = 1.0f;
	handshake.FOV					  = 90.0f;
	handshake.isVR					  = false;
	handshake.framerate				  = 60;
	auto &clientPipeline			  = sessionClient->GetClientPipeline();
	handshake.udpBufferSize			  = static_cast<uint32_t>(clientPipeline.source->getSystemBufferSize());
	handshake.maxBandwidthKpS		  = handshake.udpBufferSize * handshake.framerate;
	handshake.maxLightsSupported	  = 10;
	return true;
}

void InstanceRenderer::OnVideoStreamClosed()
{
	auto &clientPipeline = sessionClient->GetClientPipeline();
	TELEPORT_LOG("VIDEO STREAM CLOSED\n");
	clientPipeline.pipeline.deconfigure();
	clientPipeline.videoQueue.deconfigure();
	clientPipeline.audioQueue.deconfigure();
	clientPipeline.geometryQueue.deconfigure();

	receivedInitialPos = 0;
}

void InstanceRenderer::OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand &reconfigureVideoCommand)
{
	auto	   &clientPipeline = sessionClient->GetClientPipeline();
	const auto &videoConfig	   = reconfigureVideoCommand.video_config;
	TELEPORT_LOG("VIDEO STREAM RECONFIGURED: clr %d x %d dpth %d x %d",
				 videoConfig.video_width,
				 videoConfig.video_height,
				 videoConfig.depth_width,
				 videoConfig.depth_height);

	clientPipeline.decoderParams.deferDisplay		   = false;
	clientPipeline.decoderParams.decodeFrequency	   = avs::DecodeFrequency::NALUnit;
	clientPipeline.decoderParams.codec				   = videoConfig.videoCodec;
	clientPipeline.decoderParams.use10BitDecoding	   = videoConfig.use_10_bit_decoding;
	clientPipeline.decoderParams.useYUV444ChromaFormat = videoConfig.use_yuv_444_decoding;
	clientPipeline.decoderParams.useAlphaLayerDecoding = videoConfig.use_alpha_layer_decoding;

	avs::DeviceHandle dev;
#if TELEPORT_CLIENT_USE_D3D12
	dev.handle = renderPlatform->AsD3D12Device();
	;
	dev.type = avs::DeviceType::Direct3D12;
#elif TELEPORT_CLIENT_USE_D3D11
	dev.handle = renderPlatform->AsD3D11Device();
	dev.type   = avs::DeviceType::Direct3D11;
#else
	dev.handle = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	dev.type   = avs::DeviceType::Vulkan;
#endif

	size_t											   stream_width	 = videoConfig.video_width;
	size_t											   stream_height = videoConfig.video_height;

	std::shared_ptr<std::vector<std::vector<uint8_t>>> empty_data;
	if (videoConfig.use_cubemap)
	{
		instanceRenderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																		  videoConfig.colour_cubemap_size,
																		  videoConfig.colour_cubemap_size,
																		  1,
																		  1,
																		  crossplatform::PixelFormat::RGBA_32_FLOAT,
																		  empty_data,
																		  true,
																		  false,
																		  false,
																		  true);
	}
	else
	{
		instanceRenderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform,
																		  videoConfig.perspective_width,
																		  videoConfig.perspective_height,
																		  1,
																		  1,
																		  crossplatform::PixelFormat::RGBA_32_FLOAT,
																		  empty_data,
																		  true,
																		  false,
																		  false,
																		  false);
	}

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(videoConfig.video_height) / float(stream_height);

	AVSTextureImpl *ti	= (AVSTextureImpl *)(instanceRenderState.avsTexture.get());
	// Only create new texture and register new surface if resolution has changed
	if (ti && ti->texture->GetWidth() != stream_width || ti->texture->GetLength() != stream_height)
	{
		SAFE_DELETE(ti->texture);

		if (!clientPipeline.decoder.unregisterSurface())
		{
			throw std::runtime_error("Failed to unregister decoder surface");
		}

		CreateTexture(renderPlatform, instanceRenderState.avsTexture, int(stream_width), int(stream_height));
	}

	if (!clientPipeline.decoder.reconfigure((int)stream_width, (int)stream_height, clientPipeline.decoderParams))
	{
		throw std::runtime_error("Failed to reconfigure decoder");
	}
}

void InstanceRenderer::OnStreamingControlMessage(const std::string &str)
{
}