#include "InstanceRenderer.h"
#include "Renderer.h"
#include <fmt/core.h>

using namespace teleport;
using namespace clientrender;
using namespace platform;

InstanceRenderer::InstanceRenderer(avs::uid server)
	:geometryCache(new clientrender::NodeManager)
{
	server_uid=server;
	resourceCreator.SetGeometryCache(&geometryCache);
}

InstanceRenderer::~InstanceRenderer()
{
}

void InstanceRenderer::RestoreDeviceObjects(clientrender::RenderPlatform *r)
{
	clientRenderPlatform=r;
	resourceCreator.Initialize(clientRenderPlatform, clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);
}

void InstanceRenderer::InvalidateDeviceObjects()
{}

void InstanceRenderer::RenderLocalNodes(crossplatform::GraphicsDeviceContext& deviceContext
			,RenderState &renderState
			,avs::uid this_server_uid, clientrender::GeometryCache& g)
{
	auto renderPlatform=deviceContext.renderPlatform;
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
		// The following block renders to the hdrFramebuffer's rendertarget:
		//renderState.stereoCameraConstants.stereoViewPosition = ((const float*)&clientDeviceState->headPose.globalPose.position);
	}
	//else
	{
		deviceContext.viewStruct.Init();
		renderState.cameraConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
		renderState.cameraConstants.view = deviceContext.viewStruct.view;
		renderState.cameraConstants.proj = deviceContext.viewStruct.proj;
		renderState.cameraConstants.viewProj = deviceContext.viewStruct.viewProj;
		// The following block renders to the hdrFramebuffer's rendertarget:
		//renderState.cameraConstants.viewPosition = ((const float*)&clientDeviceState->headPose.globalPose.position);
	}

	{
		std::unique_ptr<std::lock_guard<std::mutex>> cacheLock;
		auto &cachedLights=g.mLightManager.GetCache(cacheLock);
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

	const clientrender::NodeManager::nodeList_t& nodeList = g.mNodeManager->GetSortedRootNodes();
	for(const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		if(renderState.show_only!=0&&renderState.show_only!=node->id)
			continue;
		RenderNode(deviceContext,renderState
			,node,g);
	}
	const clientrender::NodeManager::nodeList_t& transparentList = g.mNodeManager->GetSortedTransparentNodes();
	for(const std::shared_ptr<clientrender::Node>& node : transparentList)
	{
		if(renderState.show_only!=0&&renderState.show_only!=node->id)
			continue;
		RenderNode(deviceContext,renderState
		,node,g,false,false);
	}
	if(renderState.show_node_overlays)
	for (const std::shared_ptr<clientrender::Node>& node : nodeList)
	{
		RenderNodeOverlay(deviceContext,renderState, node,g);
	}

}


void InstanceRenderer::RenderNode(crossplatform::GraphicsDeviceContext& deviceContext
	,RenderState &renderState
	,const std::shared_ptr<clientrender::Node>& node
	,clientrender::GeometryCache &g
	,bool force
	,bool include_children)
{
	auto renderPlatform=deviceContext.renderPlatform;
	clientrender::AVSTextureHandle th = renderState.avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);

	std::shared_ptr<clientrender::Texture> globalIlluminationTexture;
	if(node->GetGlobalIlluminationTextureUid() )
		globalIlluminationTexture = g.mTextureManager.Get(node->GetGlobalIlluminationTextureUid());
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
					auto t = g.mTextureManager.Get(renderState.lastSetupCommand.clientDynamicLighting.diffuseCubemapTexture);
					if(t)
					{
						renderState.pbrEffect->SetTexture(deviceContext,renderState.pbrEffect_diffuseCubemap,t->GetSimulTexture());
					}
				}
				renderState.pbrEffect->SetTexture(deviceContext, renderState.pbrEffect_specularCubemap,renderState.specularCubemapTexture);
				if(renderState.lastSetupCommand.backgroundMode!=teleport::core::BackgroundMode::VIDEO&&renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture!=0)
				{
					auto t = g.mTextureManager.Get(renderState.lastSetupCommand.clientDynamicLighting.specularCubemapTexture);
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
			RenderNode(deviceContext,
			renderState
		,child,g,false);
		}
	}
}


void InstanceRenderer::RenderNodeOverlay(crossplatform::GraphicsDeviceContext& deviceContext
	,RenderState &renderState
	,const std::shared_ptr<clientrender::Node>& node
	,clientrender::GeometryCache &g,bool force)
{
	auto renderPlatform=deviceContext.renderPlatform;
	clientrender::AVSTextureHandle th = renderState.avsTexture;
	clientrender::AVSTexture& tx = *th;
	AVSTextureImpl* ti = static_cast<AVSTextureImpl*>(&tx);
	avs::uid node_select=renderState.selected_uid;

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
			RenderNodeOverlay(deviceContext, renderState,child,g,true);
		}
	}
}
