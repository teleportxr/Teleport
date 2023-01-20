#include "InstanceRenderer.h"
#include "Renderer.h"
#include <fmt/core.h>
#include "TeleportClient/Log.h"
#include "TeleportClient/ServerTimestamp.h"
#include "Platform/CrossPlatform/BaseFramebuffer.h"

using namespace teleport;
using namespace clientrender;
using namespace platform;

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

void CreateTexture(platform::crossplatform::RenderPlatform *renderPlatform,clientrender::AVSTextureHandle &th,int width, int height)
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

InstanceRenderer::InstanceRenderer(avs::uid server,teleport::client::Config &c,GeometryDecoder &g,RenderState &rs
		,teleport::client::SessionClient *sc)
	:sessionClient(sc)
	,renderState(rs)
	,config(c)
	,geometryDecoder(g)
	,geometryCache(new clientrender::NodeManager)
{
	server_uid=server;
	resourceCreator.SetGeometryCache(&geometryCache);
#ifdef _MSC_VER
	audioPlayer.initializeAudioDevice();
#endif
}

InstanceRenderer::~InstanceRenderer()
{
	clientPipeline.pipeline.deconfigure();
}

void InstanceRenderer::RestoreDeviceObjects(clientrender::RenderPlatform *r)
{
	clientRenderPlatform=r;
	renderPlatform=r->GetSimulRenderPlatform();
	resourceCreator.Initialize(clientRenderPlatform, clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);
}

void InstanceRenderer::InvalidateDeviceObjects()
{}



void InstanceRenderer::RenderVideoTexture(crossplatform::GraphicsDeviceContext& deviceContext,avs::uid server_uid, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, const char* technique, const char* shaderTexture)
{
	bool multiview = deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr;
	
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	renderState.tagDataCubeBuffer.Apply(deviceContext, renderState.cubemapClearEffect,renderState.cubemapClearEffect_TagDataCubeBuffer);
	renderState.cubemapConstants.depthOffsetScale = vec4(0, 0, 0, 0);
	renderState.cubemapConstants.offsetFromVideo = *((vec3*)&clientServerState.headPose.globalPose.position) - videoPos;
	renderState.cubemapConstants.cameraPosition = *((vec3*)&clientServerState.headPose.globalPose.position);
	renderState.cubemapConstants.cameraRotation = *((vec4*)&clientServerState.headPose.globalPose.orientation);
	renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
	renderState.cubemapClearEffect->SetTexture(deviceContext, shaderTexture, targetTexture);
	renderState.cubemapClearEffect->SetTexture(deviceContext, "plainTexture", srcTexture);
	renderState.cubemapClearEffect->Apply(deviceContext, technique, multiview ? "multiview" : "singleview");
	deviceContext.renderPlatform->DrawQuad(deviceContext);
	renderState.cubemapClearEffect->Unapply(deviceContext);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}
void InstanceRenderer::RecomposeVideoTexture(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, const char* technique)
{
	int W = targetTexture->width;
	int H = targetTexture->length;
	renderState.cubemapConstants.sourceOffset = { 0, 0 };
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
	
	renderState.cubemapClearEffect->SetTexture(deviceContext, "plainTexture",srcTexture);
	renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
	renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cameraConstants);
	renderState.cubemapClearEffect->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, targetTexture);
	renderState.tagDataIDBuffer.Apply(deviceContext, renderState.cubemapClearEffect, renderState.cubemapClearEffect_TagDataIDBuffer);
	int zGroups = renderState.videoTexture->IsCubemap() ? 6 : 1;
	renderState.cubemapClearEffect->Apply(deviceContext, technique, 0);
	deviceContext.renderPlatform->DispatchCompute(deviceContext, W / 16, H / 16, zGroups);
	renderState.cubemapClearEffect->Unapply(deviceContext);
	renderState.cubemapClearEffect->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, nullptr);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RecomposeCubemap(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* srcTexture, crossplatform::Texture* targetTexture, int mips, int2 sourceOffset)
{
	renderState.cubemapConstants.sourceOffset = sourceOffset;
	renderState.cubemapClearEffect->SetTexture(deviceContext, renderState.plainTexture, srcTexture);

	renderState.cubemapConstants.targetSize.x = targetTexture->width;
	renderState.cubemapConstants.targetSize.y = targetTexture->length;

	for (int m = 0; m < mips; m++)
	{
		renderState.cubemapClearEffect->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, targetTexture, -1, m);
		renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
		renderState.cubemapClearEffect->Apply(deviceContext, "recompose", 0);
		deviceContext.renderPlatform->DispatchCompute(deviceContext, targetTexture->width / 16, targetTexture->width / 16, 6);
		renderState.cubemapClearEffect->Unapply(deviceContext);
		renderState.cubemapConstants.sourceOffset.x += 3 * renderState.cubemapConstants.targetSize.x;
		renderState.cubemapConstants.targetSize /= 2;
	}
	renderState.cubemapClearEffect->SetUnorderedAccessView(deviceContext, renderState.RWTextureTargetArray, nullptr);
	renderState.cubemapClearEffect->UnbindTextures(deviceContext);
}

void InstanceRenderer::RenderView(crossplatform::GraphicsDeviceContext& deviceContext)
{
	auto renderPlatform=deviceContext.renderPlatform;

		clientrender::AVSTextureHandle th = renderState.avsTexture;
		clientrender::AVSTexture& tx = *th;
		AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);

		if (ti)
		{
			// This will apply to both rendering methods
			renderState.cubemapClearEffect->SetTexture(deviceContext,renderState.plainTexture, ti->texture);
			renderState.tagDataIDBuffer.ApplyAsUnorderedAccessView(deviceContext, renderState.cubemapClearEffect, renderState._RWTagDataIDBuffer);
			renderState.cubemapConstants.sourceOffset = int2(ti->texture->width - (32 * 4), ti->texture->length - 4);
			renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cubemapConstants);
			renderState.cubemapClearEffect->Apply(deviceContext, "extract_tag_data_id", 0);
			renderPlatform->DispatchCompute(deviceContext, 1, 1, 1);
			renderState.cubemapClearEffect->Unapply(deviceContext);
			renderState.cubemapClearEffect->UnbindTextures(deviceContext);

			renderState.tagDataIDBuffer.CopyToReadBuffer(deviceContext);
			const uint4* videoIDBuffer = renderState.tagDataIDBuffer.OpenReadBuffer(deviceContext);
			if (videoIDBuffer && videoIDBuffer[0].x < 32 && videoIDBuffer[0].w == 110) // sanity check
			{
				int tagDataID = videoIDBuffer[0].x;

				const auto& ct = videoTagDataCubeArray[tagDataID].coreData.cameraTransform;
				videoPos = vec3(ct.position.x, ct.position.y, ct.position.z);

				videoPosDecoded = true;
			}
			renderState.tagDataIDBuffer.CloseReadBuffer(deviceContext);
			UpdateTagDataBuffers(deviceContext);
			if (sessionClient->IsConnected())
			{
				if (renderState.lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::VIDEO)
				{
					if (renderState.videoTexture->IsCubemap())
					{
						const char* technique = clientPipeline.videoConfig.use_alpha_layer_decoding ? "recompose" : "recompose_with_depth_alpha";
						RecomposeVideoTexture(deviceContext, ti->texture, renderState.videoTexture, technique);
					}
					else
					{
						const char* technique = clientPipeline.videoConfig.use_alpha_layer_decoding ? "recompose_perspective" : "recompose_perspective_with_depth_alpha";
						RecomposeVideoTexture(deviceContext, ti->texture, renderState.videoTexture, technique);
					}
				}
			}
			RecomposeCubemap(deviceContext, ti->texture, renderState.diffuseCubemapTexture, renderState.diffuseCubemapTexture->mips, int2(renderState.lastSetupCommand.clientDynamicLighting.diffusePos[0], renderState.lastSetupCommand.clientDynamicLighting.diffusePos[1]));
			RecomposeCubemap(deviceContext, ti->texture, renderState.specularCubemapTexture, renderState.specularCubemapTexture->mips, int2(renderState.lastSetupCommand.clientDynamicLighting.specularPos[0], renderState.lastSetupCommand.clientDynamicLighting.specularPos[1]));
		}

		// Draw the background. If unconnected, we show a grid and horizon.
		// If connected, we show the server's chosen background: video, texture or colour.
		{
			if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
			{
				crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
				renderState.stereoCameraConstants.leftInvWorldViewProj = mgdc.viewStructs[0].invViewProj;
				renderState.stereoCameraConstants.rightInvWorldViewProj = mgdc.viewStructs[1].invViewProj;
				renderState.stereoCameraConstants.stereoViewPosition = mgdc.viewStruct.cam_pos;
				renderState.cubemapClearEffect->SetConstantBuffer(mgdc, &renderState.stereoCameraConstants);
			}
			//else
			{
				renderState.cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
				renderState.cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
				renderState.cubemapClearEffect->SetConstantBuffer(deviceContext, &renderState.cameraConstants);
			}
			if (sessionClient->IsConnected())
			{
				if (renderState.lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::COLOUR)
				{
					renderPlatform->Clear(deviceContext, ConvertVec4<vec4>(renderState.lastSetupCommand.backgroundColour));
				}
				else if (renderState.lastSetupCommand.backgroundMode == teleport::core::BackgroundMode::VIDEO)
				{
					if (renderState.videoTexture->IsCubemap())
					{
						RenderVideoTexture(deviceContext, server_uid,ti->texture, renderState.videoTexture, "use_cubemap", "cubemapTexture");
					}
					else
					{
						math::Matrix4x4 projInv;
						deviceContext.viewStruct.proj.Inverse(projInv);
						RenderVideoTexture(deviceContext, server_uid,ti->texture, renderState.videoTexture, "use_perspective", "perspectiveTexture");
					}
				}
			}
			else
			{
				std::string passName = (int)config.options.lobbyView ? "neon" : "white";
				if (deviceContext.AsMultiviewGraphicsDeviceContext() != nullptr)
					passName += "_multiview";

				renderState.cubemapClearEffect->Apply(deviceContext, "unconnected", passName.c_str());
				renderPlatform->DrawQuad(deviceContext);
				renderState.cubemapClearEffect->Unapply(deviceContext);
			}
		}
		vec4 white={1.f,1.f,1.f,1.f};
		renderState.pbrConstants.drawDistance = renderState.lastSetupCommand.draw_distance;
		if(renderState.specularCubemapTexture)
			renderState.pbrConstants.roughestMip=float(renderState.specularCubemapTexture->mips-1);
		if(renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture!=0)
		{
			auto t = geometryCache.mTextureManager.Get(renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
			if(t&&t->GetSimulTexture())
			{
				renderState.pbrConstants.roughestMip=float(t->GetSimulTexture()->mips-1);
			}
		}
		if (sessionClient->IsConnected()||config.options.showGeometryOffline)
			RenderLocalNodes(deviceContext,server_uid);
}

void InstanceRenderer::RenderLocalNodes(crossplatform::GraphicsDeviceContext& deviceContext
			,avs::uid this_server_uid)
{
	auto renderPlatform=deviceContext.renderPlatform;
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(this_server_uid);
		// Now, any nodes bound to OpenXR poses will be updated. This may include hand objects, for example.
	if(renderState.openXR)
	{
		avs::uid root_node_uid=renderState.openXR->GetRootNode(this_server_uid);
		if(root_node_uid!=0)
		{
			std::shared_ptr<clientrender::Node> node=geometryCache.mNodeManager->GetNode(root_node_uid);
			if(node)
			{
				auto pose=clientServerState.originPose;
				node->SetLocalPosition(pose.position);
				node->SetLocalRotation(pose.orientation);
			}
		}
	}
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
		renderState.stereoCameraConstants.stereoViewPosition = ((const float*)&clientServerState.headPose.globalPose.position);
	renderState.cameraConstants.viewPosition = ((const float*)&clientServerState.headPose.globalPose.position);
	
	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto &cachedLights=geometryCache.mLightManager.GetCache(cacheLock);
		if(cachedLights.size()>renderState.lightsBuffer.count)
		{
			renderState.lightsBuffer.InvalidateDeviceObjects();
			renderState.lightsBuffer.RestoreDeviceObjects(renderPlatform, static_cast<int>(cachedLights.size()));
		}
		renderState.pbrConstants.lightCount = static_cast<int>(cachedLights.size());
	}
	if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
	{
		crossplatform::MultiviewGraphicsDeviceContext& mgdc = *deviceContext.AsMultiviewGraphicsDeviceContext();
		mgdc.viewStructs[0].Init();
		mgdc.viewStructs[1].Init();
		renderState.stereoCameraConstants.leftInvWorldViewProj = mgdc.viewStructs[0].invViewProj;
		renderState.stereoCameraConstants.leftView = mgdc.viewStructs[0].view;
		renderState.stereoCameraConstants.leftProj = mgdc.viewStructs[0].proj;
		renderState.stereoCameraConstants.leftViewProj = mgdc.viewStructs[0].viewProj;
		renderState.stereoCameraConstants.rightInvWorldViewProj = mgdc.viewStructs[1].invViewProj;
		renderState.stereoCameraConstants.rightView = mgdc.viewStructs[1].view;
		renderState.stereoCameraConstants.rightProj = mgdc.viewStructs[1].proj;
		renderState.stereoCameraConstants.rightViewProj = mgdc.viewStructs[1].viewProj;
	}
	//else
	{
		deviceContext.viewStruct.Init();
		renderState.cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
		renderState.cameraConstants.view = deviceContext.viewStruct.view;
		renderState.cameraConstants.proj = deviceContext.viewStruct.proj;
		renderState.cameraConstants.viewProj = deviceContext.viewStruct.viewProj;
	}

	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto &cachedLights=geometryCache.mLightManager.GetCache(cacheLock);
		if(cachedLights.size()>renderState.lightsBuffer.count)
		{
			renderState.lightsBuffer.InvalidateDeviceObjects();
			renderState.lightsBuffer.RestoreDeviceObjects(renderPlatform, static_cast<int>(cachedLights.size()));
		}
		renderState.pbrConstants.lightCount = static_cast<int>(cachedLights.size());
	}
	// Now, any nodes bound to OpenXR poses will be updated. This may include hand objects, for example.
	if(renderState.openXR)
	{
	/*	avs::uid root_node_uid=renderState.openXR->GetRootNode(this_server_uid);
		if(root_node_uid!=0)
		{
			std::shared_ptr<clientrender::Node> node=g.mNodeManager->GetNode(root_node_uid);
			if(node)
			{
				auto pose=sessionClient->GetOriginPose();
				node->SetLocalPosition(pose.position);
				node->SetLocalRotation(pose.orientation);
			}
		}*/
	// The node pose states are in the space whose origin is the VR device's playspace origin.
		const auto &nodePoseStates=renderState.openXR->GetNodePoseStates(this_server_uid,renderPlatform->GetFrameNumber());
		for(auto &n:nodePoseStates)
		{
			// TODO, we set LOCAL node pose from GLOBAL worldspace because we ASSUME no parent for these nodes.
			std::shared_ptr<clientrender::Node> node=geometryCache.mNodeManager->GetNode(n.first);
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

	const clientrender::NodeManager::nodeList_t& nodeList = geometryCache.mNodeManager->GetSortedRootNodes();
	for(const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		if(renderState.show_only!=0&&renderState.show_only!=node->id)
			continue;
		RenderNode(deviceContext
			,node);
	}
	const clientrender::NodeManager::nodeList_t& transparentList = geometryCache.mNodeManager->GetSortedTransparentNodes();
	for(const std::shared_ptr<clientrender::Node>& node : transparentList)
	{
		if(renderState.show_only!=0&&renderState.show_only!=node->id)
			continue;
		RenderNode(deviceContext
		,node,false,false);
	}
	if(renderState.show_node_overlays)
	for (const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		RenderNodeOverlay(deviceContext, node);
	}

}


void InstanceRenderer::RenderNode(crossplatform::GraphicsDeviceContext& deviceContext
	,const std::shared_ptr<clientrender::Node>& node
	,bool force
	,bool include_children)
{
	auto renderPlatform=deviceContext.renderPlatform;
	clientrender::AVSTextureHandle th = renderState.avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);

	std::shared_ptr<clientrender::Texture> globalIlluminationTexture;
	if(node->GetGlobalIlluminationTextureUid() )
		globalIlluminationTexture = geometryCache.mTextureManager.Get(node->GetGlobalIlluminationTextureUid());
	// Pass used for rendering geometry.
	std::string passName = "pbr_nolightmap";
	if(node->IsStatic())
		passName="pbr_lightmap";
	if(renderState.overridePassName.length()>0)
		passName=renderState.overridePassName;
	bool force_highlight = force||(renderState.selected_uid== node->id);
	//Only render visible nodes, but still render children that are close enough.
	if(node->GetPriority()>=0)
	if(node->IsVisible()&&(renderState.show_only == 0 || renderState.show_only == node->id))
	{
		const std::shared_ptr<clientrender::Mesh> mesh = node->GetMesh();
		if(mesh)
		{
			const auto& meshInfo	= mesh->GetMeshCreateInfo();
			static int mat_select	= -1;
			for(size_t element=0; element<node->GetMaterials().size() && element<meshInfo.ib.size(); element++)
			{
				if(mat_select >= 0 && mat_select != element)
					continue;
				std::shared_ptr<clientrender::Material> material = node->GetMaterials()[element];
				if(!material)
					continue;
				const clientrender::Material::MaterialCreateInfo& matInfo = material->GetMaterialCreateInfo();
				bool transparent	=(matInfo.materialMode==avs::MaterialMode::TRANSPARENT_MATERIAL);
				if(transparent==include_children)
					continue;
				bool double_sided=false;
				auto* vb = meshInfo.vb[element].get();
				const auto* ib = meshInfo.ib[element].get();

				const crossplatform::Buffer* const v[] = {vb->GetSimulVertexBuffer()};
				if(!v[0])
					continue;
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
					mat4::mul(renderState.stereoCameraConstants.leftWorldViewProj, *((mat4*)&mgdc.viewStructs[0].viewProj), model);
					renderState.stereoCameraConstants.leftWorld = model;
					mat4::mul(renderState.stereoCameraConstants.rightWorldViewProj, *((mat4*)&mgdc.viewStructs[1].viewProj), model);
					renderState.stereoCameraConstants.rightWorld = model;
				}
				//else
				{
					mat4::mul(renderState.cameraConstants.worldViewProj, *((mat4*)&deviceContext.viewStruct.viewProj), model);
					renderState.cameraConstants.world = model;
				}
				// TODO: Improve this.
				auto sc=node->GetGlobalScale();
				bool negative_scale=(sc.x*sc.y*sc.z)<0.0f;
				std::shared_ptr<clientrender::Texture> gi = globalIlluminationTexture;
				std::string usedPassName = passName;
				if(material->GetMaterialCreateInfo().shader.length())
				{
					usedPassName=material->GetMaterialCreateInfo().shader;
					double_sided=true;
				}
				std::shared_ptr<clientrender::SkinInstance> skinInstance = node->GetSkinInstance();
				bool anim=skinInstance!=nullptr;
				if (skinInstance)
				{
					mat4* scr_matrices = skinInstance->GetBoneMatrices(globalTransformMatrix);
					BoneMatrices *b=static_cast<BoneMatrices*>(&renderState.boneMatrices);
					memcpy(b, scr_matrices, sizeof(mat4) * clientrender::Skin::MAX_BONES);

					renderState.pbrEffect->SetConstantBuffer(deviceContext, &renderState.boneMatrices);
					//usedPassName = "anim_" + usedPassName;
				}

				crossplatform::MultiviewGraphicsDeviceContext* mvgdc = deviceContext.AsMultiviewGraphicsDeviceContext();
				bool highlight=node->IsHighlighted()||force_highlight;
				crossplatform::EffectPass *pass=nullptr;
				
				highlight|= (renderState.selected_uid == material->id);
				const clientrender::Material::MaterialData& md = material->GetMaterialData();
				memcpy(&renderState.pbrConstants.diffuseOutputScalar, &md, sizeof(md));
				renderState.pbrConstants.lightmapScaleOffset=*(const vec4*)(&(node->GetLightmapScaleOffset()));
				std::shared_ptr<clientrender::Texture> diffuse	= matInfo.diffuse.texture;
				std::shared_ptr<clientrender::Texture> normal	= matInfo.normal.texture;
				std::shared_ptr<clientrender::Texture> combined = matInfo.combined.texture;
				std::shared_ptr<clientrender::Texture> emissive = matInfo.emissive.texture;
				
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_diffuseTexture	,diffuse ? diffuse->GetSimulTexture() : nullptr);
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_normalTexture	,normal ? normal->GetSimulTexture() : nullptr);
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_combinedTexture	,combined ? combined->GetSimulTexture() : nullptr);
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_emissiveTexture	,emissive ? emissive->GetSimulTexture() : nullptr);
				
				crossplatform::EffectTechnique* pbrEffectTechnique = mvgdc ?(transparent?renderState.pbrEffect_transparentMultiviewTechnique:(anim?renderState.pbrEffect_solidAnimMultiviewTechnique:renderState.pbrEffect_solidMultiviewTechnique))
																			:(transparent?renderState.pbrEffect_transparentTechnique:(anim?renderState.pbrEffect_solidAnimTechnique:renderState.pbrEffect_solidTechnique));
				pass = pbrEffectTechnique->GetPass(usedPassName.c_str());
				if(!pass)
				{
					TELEPORT_CERR<<"Pass "<<usedPassName.c_str()<<" not found in "<<pbrEffectTechnique->name.c_str()<<"\n";
					pass=pbrEffectTechnique->GetPass(0);
				}
				if (highlight)
				{
					renderState.pbrConstants.emissiveOutputScalar += vec4(0.2f, 0.2f, 0.2f, 0.f);
				}
				renderState.pbrEffect->SetTexture(deviceContext,renderState.pbrEffect_globalIlluminationTexture, gi ? gi->GetSimulTexture() : nullptr);

				renderState.pbrEffect->SetTexture(deviceContext,renderState.pbrEffect_diffuseCubemap,renderState.diffuseCubemapTexture);
				// If lighting is via static textures.
				if(renderState.lastSetupCommand.backgroundMode!=teleport::core::BackgroundMode::VIDEO&&renderState.lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture!=0)
				{
					auto t = geometryCache.mTextureManager.Get(renderState.lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture);
					if(t)
					{
						renderState.pbrEffect->SetTexture(deviceContext,renderState.pbrEffect_diffuseCubemap,t->GetSimulTexture());
					}
				}
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_specularCubemap,renderState.specularCubemapTexture);
				if(renderState.lastSetupCommand.backgroundMode!=teleport::core::BackgroundMode::VIDEO&&renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture!=0)
				{
					auto t = geometryCache.mTextureManager.Get(renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
					if(t)
					{
						renderState.pbrEffect->SetTexture(deviceContext,renderState.pbrEffect_specularCubemap,t->GetSimulTexture());
					}
				}
				
				renderState.lightsBuffer.Apply(deviceContext, renderState.pbrEffect, renderState._lights );
				renderState.tagDataCubeBuffer.Apply(deviceContext, renderState.pbrEffect, renderState.cubemapClearEffect_TagDataCubeBuffer);
				renderState.tagDataIDBuffer.Apply(deviceContext, renderState.pbrEffect, renderState.pbrEffect_TagDataIDBuffer);

				renderState.pbrEffect->SetConstantBuffer(deviceContext, &renderState.pbrConstants);
				if (deviceContext.deviceContextType == crossplatform::DeviceContextType::MULTIVIEW_GRAPHICS)
					renderState.pbrEffect->SetConstantBuffer(deviceContext, &renderState.stereoCameraConstants);
				//else
					renderState.pbrEffect->SetConstantBuffer(deviceContext, &renderState.cameraConstants);
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
				renderState.pbrEffect->UnbindTextures(deviceContext);
				renderPlatform->UnapplyPass(deviceContext);
				layout->Unapply(deviceContext);
			}
		}
	}
	if(!include_children)
		return;
	for(std::weak_ptr<clientrender::Node> childPtr : node->GetChildren())
	{
		std::shared_ptr<clientrender::Node> child = childPtr.lock();
		if(child)
		{
			RenderNode(deviceContext,child,false);
		}
	}
}


void InstanceRenderer::RenderNodeOverlay(crossplatform::GraphicsDeviceContext& deviceContext
	,const std::shared_ptr<clientrender::Node>& node
	,bool force)
{
	auto renderPlatform=deviceContext.renderPlatform;
	clientrender::AVSTextureHandle th = renderState.avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
	avs::uid node_select=renderState.selected_uid;

	std::shared_ptr<clientrender::Texture> globalIlluminationTexture;
	if (node->GetGlobalIlluminationTextureUid())
		globalIlluminationTexture = geometryCache.mTextureManager.Get(node->GetGlobalIlluminationTextureUid());

	//Only render visible nodes, but still render children that are close enough.
	if (node->IsVisible()&& (node_select == 0 || node_select == node->id))
	{
		const std::shared_ptr<clientrender::Mesh> mesh = node->GetMesh();
		const clientrender::AnimationComponent& anim = node->animationComponent;
		avs::vec3 pos = node->GetGlobalPosition();
		mat4 m=node->GetGlobalTransform().GetTransformMatrix();
		renderPlatform->DrawAxes(deviceContext,m,0.1f);
	
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
			RenderNodeOverlay(deviceContext, child,true);
		}
	}
}


void InstanceRenderer::ConfigureVideo(const avs::VideoConfig& videoConfig)
{
	clientPipeline.videoConfig = videoConfig;
}

bool InstanceRenderer::OnNodeEnteredBounds(avs::uid id)
{
	return geometryCache.mNodeManager->ShowNode(id);
}

bool InstanceRenderer::OnNodeLeftBounds(avs::uid id)
{
	return geometryCache.mNodeManager->HideNode(id);
}


void InstanceRenderer::UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &updateNodeStructureCommand)
{
	geometryCache.mNodeManager->ReparentNode(updateNodeStructureCommand);
}

void InstanceRenderer::UpdateNodeSubtype(const teleport::core::UpdateNodeSubtypeCommand &updateNodeStructureCommand,const std::string &regexPath)
{
	if(regexPath.size())
	{
		renderState.openXR->MapNodeToPose(server_uid,updateNodeStructureCommand.nodeID,regexPath);
	}
	else
	{
		TELEPORT_CERR << "Unrecognised node regexPath: "<<regexPath.c_str() << "!\n";
	}
}

void InstanceRenderer::SetVisibleNodes(const std::vector<avs::uid>& visibleNodes)
{
	geometryCache.mNodeManager->SetVisibleNodes(visibleNodes);
}

void InstanceRenderer::UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList)
{
	geometryCache.mNodeManager->UpdateNodeMovement(updateList);
}

void InstanceRenderer::UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList)
{
	geometryCache.mNodeManager->UpdateNodeEnabledState(updateList);
}

void InstanceRenderer::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	geometryCache.mNodeManager->SetNodeHighlighted(nodeID, isHighlighted);
}

void InstanceRenderer::UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate)
{
	geometryCache.mNodeManager->UpdateNodeAnimation(animationUpdate);
}

void InstanceRenderer::UpdateNodeAnimationControl(const teleport::core::NodeUpdateAnimationControl& animationControlUpdate)
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

void InstanceRenderer::SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	geometryCache.mNodeManager->SetNodeAnimationSpeed(nodeID, animationID, speed);
}

void InstanceRenderer::UpdateTagDataBuffers(crossplatform::GraphicsDeviceContext& deviceContext)
{				
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
				t.shadowTexCoordOffset.x=float(l.texturePosition[0])/float(renderState.lastSetupCommand.video_config.video_width);
				t.shadowTexCoordOffset.y=float(l.texturePosition[1])/float(renderState.lastSetupCommand.video_config.video_height);
				t.shadowTexCoordScale.x=float(l.textureSize)/float(renderState.lastSetupCommand.video_config.video_width);
				t.shadowTexCoordScale.y=float(l.textureSize)/float(renderState.lastSetupCommand.video_config.video_height);
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
		renderState.tagDataCubeBuffer.SetData(deviceContext, videoTagDataCube);
	}	
}


void InstanceRenderer::OnReceiveVideoTagData(const uint8_t* data, size_t dataSize)
{
	clientrender::SceneCaptureCubeTagData tagData;
	memcpy(&tagData.coreData, data, sizeof(clientrender::SceneCaptureCubeCoreTagData));
	avs::ConvertTransform(renderState.lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, tagData.coreData.cameraTransform);

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
		//avs::ConvertTransform(renderState.lastSetupCommand.axesStandard, avs::AxesStandard::EngineeringStyle, light.worldTransform);
		index += sizeof(clientrender::LightTagData);
	}
	if(tagData.coreData.id>= videoTagDataCubeArray.size())
	{
		TELEPORT_CERR_BREAK("Bad tag id",1);
		return;
	}
	videoTagDataCubeArray[tagData.coreData.id] = std::move(tagData);
}


std::vector<uid> InstanceRenderer::GetGeometryResources()
{
	return geometryCache.GetAllResourceIDs();
}

// This is called when we connect to a new server.
void InstanceRenderer::ClearGeometryResources()
{
	geometryCache.ClearAll();
	resourceCreator.Clear();
}

void InstanceRenderer::OnLightingSetupChanged(const teleport::core::SetupLightingCommand &l)
{
	renderState.lastSetupLightingCommand=l;
}

void InstanceRenderer::OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions_)
{
	if (renderState.openXR)
		renderState.openXR->OnInputsSetupChanged(server_uid,inputDefinitions_);
}

void InstanceRenderer::SetOrigin(const avs::Pose &p) 
{
	auto &clientServerState=teleport::client::ClientServerState::GetClientServerState(server_uid);
	clientServerState.originPose=p;
}


bool InstanceRenderer::OnSetupCommandReceived(const char *server_ip,const teleport::core::SetupCommand &setupCommand,teleport::core::Handshake &handshake)
{
	ConfigureVideo(setupCommand.video_config);

	TELEPORT_CLIENT_WARN("SETUP COMMAND RECEIVED: server_streaming_port %d clr %d x %d dpth %d x %d\n", setupCommand.server_streaming_port, clientPipeline.videoConfig.video_width, clientPipeline.videoConfig.video_height
																	, clientPipeline.videoConfig.depth_width, clientPipeline.videoConfig.depth_height	);
	videoPosDecoded=false;

	videoTagDataCubeArray.clear();
	videoTagDataCubeArray.resize(RenderState::maxTagDataSize);

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

	AVSTextureImpl* ti = (AVSTextureImpl*)(renderState.avsTexture.get());
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
			renderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.colour_cubemap_size, clientPipeline.videoConfig.colour_cubemap_size, 1, 1,
				crossplatform::PixelFormat::RGBA_16_FLOAT, true, false, false, true);
	}
	else
	{
		renderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.perspective_width, clientPipeline.videoConfig.perspective_height, 1, 1,
			crossplatform::PixelFormat::RGBA_16_FLOAT, true, false, false, false);
	}
	renderState.specularCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, setupCommand.clientDynamicLighting.specularCubemapSize, setupCommand.clientDynamicLighting.specularCubemapSize, 1, setupCommand.clientDynamicLighting.specularMips, crossplatform::PixelFormat::RGBA_8_UNORM, true, false, false, true);
	renderState.diffuseCubemapTexture->ensureTextureArraySizeAndFormat(renderPlatform, setupCommand.clientDynamicLighting.diffuseCubemapSize, setupCommand.clientDynamicLighting.diffuseCubemapSize, 1, 1,crossplatform::PixelFormat::RGBA_8_UNORM, true, false, false, true);

	const float aspect = setupCommand.video_config.perspective_width / static_cast<float>(setupCommand.video_config.perspective_height);
	const float horzFOV = setupCommand.video_config.perspective_fov * clientrender::DEG_TO_RAD;
	const float vertFOV = clientrender::GetVerticalFOVFromHorizontal(horzFOV, aspect);

	renderState.cubemapConstants.serverProj = crossplatform::Camera::MakeDepthReversedProjectionMatrix(horzFOV, vertFOV, 0.01f, 0);

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(clientPipeline.videoConfig.video_height) / float(stream_height);

	CreateTexture(renderPlatform,renderState.avsTexture, int(stream_width), int(stream_height));

// Set to a custom backend that uses platform api video decoder if using D3D12 and non NVidia card. 
#if TELEPORT_CLIENT_USE_PLATFORM_VIDEO_DECODER
	clientPipeline.decoder.setBackend(CreateVideoDecoder());
#endif

	// Video streams are 0+...
	if (!clientPipeline.decoder.configure(dev, (int)stream_width, (int)stream_height, clientPipeline.decoderParams, 20))
	{
		TELEPORT_CERR << "Failed to configure decoder node!\n";
	}
	if (!clientPipeline.surface.configure(renderState.avsTexture->createSurface()))
	{
		TELEPORT_CERR << "Failed to configure output surface node!\n";
	}

	clientPipeline.videoQueue.configure(300000, 16, "VideoQueue");

	avs::PipelineNode::link(clientPipeline.source, clientPipeline.videoQueue);
	avs::PipelineNode::link(clientPipeline.videoQueue, clientPipeline.decoder);
	clientPipeline.pipeline.link({ &clientPipeline.decoder, &clientPipeline.surface });
	
	// Tag Data
	{
		auto f = std::bind(&InstanceRenderer::OnReceiveVideoTagData, this, std::placeholders::_1, std::placeholders::_2);
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

		clientPipeline.geometryQueue.configure(600000, 200, "clientPipeline.geometryQueue");

		avs::PipelineNode::link(clientPipeline.source, clientPipeline.geometryQueue);
		avs::PipelineNode::link(clientPipeline.geometryQueue, clientPipeline.avsGeometryDecoder);
		clientPipeline.pipeline.link({ &clientPipeline.avsGeometryDecoder, &clientPipeline.avsGeometryTarget });
	}

	handshake.startDisplayInfo.width = renderState.hdrFramebuffer->GetWidth();
	handshake.startDisplayInfo.height = renderState.hdrFramebuffer->GetHeight();
	handshake.axesStandard = avs::AxesStandard::EngineeringStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.FOV = 90.0f;
	handshake.isVR = false;
	handshake.framerate = 60;
	handshake.udpBufferSize = static_cast<uint32_t>(clientPipeline.source.getSystemBufferSize());
	handshake.maxBandwidthKpS = handshake.udpBufferSize * handshake.framerate;
	handshake.maxLightsSupported=10;
	handshake.clientStreamingPort = setupCommand.server_streaming_port + 1;
	renderState.lastSetupCommand = setupCommand;

	//java->Env->CallVoidMethod(java->ActivityObject, jni.initializeVideoStreamMethod, port, width, height, mVideoSurfaceTexture->GetJavaObject());
	return true;
}

void InstanceRenderer::OnVideoStreamClosed()
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

void InstanceRenderer::OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand& reconfigureVideoCommand)
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
		renderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.colour_cubemap_size, clientPipeline.videoConfig.colour_cubemap_size, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false, true);
	}
	else
	{
		renderState.videoTexture->ensureTextureArraySizeAndFormat(renderPlatform, clientPipeline.videoConfig.perspective_width, clientPipeline.videoConfig.perspective_height, 1, 1,
			crossplatform::PixelFormat::RGBA_32_FLOAT, true, false, false, false);
	}

	colourOffsetScale.x = 0;
	colourOffsetScale.y = 0;
	colourOffsetScale.z = 1.0f;
	colourOffsetScale.w = float(clientPipeline.videoConfig.video_height) / float(stream_height);

	AVSTextureImpl* ti = (AVSTextureImpl*)(renderState.avsTexture.get());
	// Only create new texture and register new surface if resolution has changed
	if (ti && ti->texture->GetWidth() != stream_width || ti->texture->GetLength() != stream_height)
	{
		SAFE_DELETE(ti->texture);

		if (!clientPipeline.decoder.unregisterSurface())
		{
			throw std::runtime_error("Failed to unregister decoder surface");
		}

		CreateTexture(renderPlatform,renderState.avsTexture, int(stream_width), int(stream_height));
	}

	if (!clientPipeline.decoder.reconfigure((int)stream_width, (int)stream_height, clientPipeline.decoderParams))
	{
		throw std::runtime_error("Failed to reconfigure decoder");
	}
	
	renderState.lastSetupCommand.video_config = reconfigureVideoCommand.video_config;
}