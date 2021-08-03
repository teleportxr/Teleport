#include "OVRNode.h"

#include "OVR_LogUtils.h"

#include "libavstream/common.hpp"

#include "crossplatform/Material.h"
#include "crossplatform/Mesh.h"
#include "Config.h"

#include "GlobalGraphicsResources.h"
#include "SCR_Class_GL_Impl/GL_Effect.h"
#include "SCR_Class_GL_Impl/GL_IndexBuffer.h"
#include "SCR_Class_GL_Impl/GL_Texture.h"
#include "SCR_Class_GL_Impl/GL_UniformBuffer.h"
#include "SCR_Class_GL_Impl/GL_VertexBuffer.h"
#include "SCR_Class_GL_Impl/GL_ShaderStorageBuffer.h"

static bool support_normals=false;
static bool support_combined=false;

OVRNode::OVRNode(avs::uid id, const std::string& name)
:Node(id, name)
{

}

void OVRNode::SurfaceInfo::SetPrograms(OVRFW::GlProgram* newProgram, OVRFW::GlProgram* newHighlightProgram)
{
	//We don't want to set a nullptr as a program, so we just use the current program.
	if(!newProgram)
	{
		newProgram = program;
	}

	if(!newHighlightProgram)
	{
		newHighlightProgram = highlightProgram;
	}

	//We want to stay in the highlighting mode we were in.
	if(!program || surfaceDef.graphicsCommand.Program.Program == program->Program)
	{
		surfaceDef.graphicsCommand.Program = *newProgram;
	}
	else
	{
		surfaceDef.graphicsCommand.Program = *newHighlightProgram;
	}
	program = newProgram;
	highlightProgram = newHighlightProgram;
}

void OVRNode::SurfaceInfo::SetHighlighted(bool highlighted)
{
	surfaceDef.graphicsCommand.Program = *(highlighted ? highlightProgram : program);
}

void OVRNode::SetMesh(std::shared_ptr<scr::Mesh> mesh)
{
	Node::SetMesh(mesh);

	//Recreate surfaces for new mesh.
	RefreshOVRSurfaces();
}

void OVRNode::SetSkin(std::shared_ptr<scr::Skin> skin)
{
	Node::SetSkin(skin);
	//Recreate surfaces for new skin.
	RefreshOVRSurfaces();
}

void OVRNode::SetMaterial(size_t index, std::shared_ptr<scr::Material> material)
{
	Node::SetMaterial(index, material);

	//Surface may not exist as we don't have a mesh, or the index is in an invalid range.
	if(index >= surfaceDefinitions.size() || index < 0)
	{
		return;
	}

	surfaceDefinitions[index] = CreateOVRSurface(index, material);
}

void OVRNode::SetMaterialListSize(size_t size)
{
	Node::SetMaterialListSize(size);

	//We can't create surfaces without a mesh, so we shouldn't extend the surface list.
	std::shared_ptr<scr::Mesh> mesh = GetMesh();
	if(!mesh)
	{
		return;
	}

	size_t old_size = surfaceDefinitions.size();
	surfaceDefinitions.resize(size);
	if(old_size < size)
	{
		GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

		//We don't know have the information on the material yet, so we use placeholder OVR surfaces.
		for(size_t i = old_size; i < size; i++)
		{
			surfaceDefinitions[i] = CreateOVRSurface(i, globalGraphicsResources.renderPlatform.placeholderMaterial);
		}
	}
}

void OVRNode::SetMaterialList(std::vector<std::shared_ptr<scr::Material>>& materials)
{
	Node::SetMaterialList(materials);

	//Recreate surfaces for new material list.
	RefreshOVRSurfaces();
}

std::string OVRNode::GetCompleteEffectPassName(const char *effectPassName)
{
	return std::string(GetSkin() ? "Animated_" : "Static_") + effectPassName;
}

OVRFW::GlProgram* OVRNode::GetEffectPass(const char* effectPassName)
{
	std::string completePassName = GetCompleteEffectPassName(effectPassName);
	return GlobalGraphicsResources::GetInstance().defaultPBREffect.GetGlProgram(completePassName.c_str());
}

void OVRNode::ChangeEffectPass(const char* effectPassName)
{
	std::string highlightPassName = std::string(effectPassName) + GlobalGraphicsResources::HIGHLIGHT_APPEND;
	OVRFW::GlProgram* program = GetEffectPass(effectPassName);
	OVRFW::GlProgram* highlightProgram = GetEffectPass(highlightPassName.c_str());

	for(SurfaceInfo& surfaceInfo : surfaceDefinitions)
	{
		surfaceInfo.SetPrograms(program, highlightProgram);
	}
}

void OVRNode::SetHighlighted(bool highlighted)
{
	Node::SetHighlighted(highlighted);

	for(SurfaceInfo& surfaceInfo : surfaceDefinitions)
	{
		surfaceInfo.SetHighlighted(highlighted);
	}
}

OVRNode::SurfaceInfo OVRNode::CreateOVRSurface(size_t materialIndex, std::shared_ptr<scr::Material> material)
{
	GlobalGraphicsResources &globalGraphicsResources = GlobalGraphicsResources::GetInstance();
	OVRNode::SurfaceInfo surfaceInfo;
	bool                 useLightmap                 = (this->isStatic);
	std::string          passname                    = GlobalGraphicsResources::GenerateShaderPassName
			(
					useLightmap,
					true,
					support_normals && !scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().normal.texture.get()),
					support_combined && !scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().combined.texture.get()),
					!scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().emissive.texture.get()) ||
					material->GetMaterialCreateInfo().emissive.textureOutputScalar !=
					avs::vec4::ZERO,
					TELEPORT_MAX_LIGHTS,
					false
			);

	OVRFW::GlProgram *ovrProgram = GetEffectPass(passname.c_str());
	if (ovrProgram == nullptr)
	{
		OVR_WARN("CreateOVRSurface Failed to create OVR surface! Effect pass %s, not found!",
				 passname.c_str());
		return surfaceInfo;
	}

	std::string highlightPassname = GlobalGraphicsResources::GenerateShaderPassName
			(
					useLightmap,
					true,
					support_normals && !scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().normal.texture.get()),
					support_combined && !scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().combined.texture.get()),
					!scc::GL_Texture::IsDummy(
							material->GetMaterialCreateInfo().emissive.texture.get()) ||
					material->GetMaterialCreateInfo().emissive.textureOutputScalar !=
					avs::vec4::ZERO,
					TELEPORT_MAX_LIGHTS,
					true
			);
	OVRFW::GlProgram *ovrHighlightProgram = GetEffectPass(highlightPassname.c_str());

	surfaceInfo.SetPrograms(ovrProgram, ovrHighlightProgram);

	if (material == nullptr)
	{
		OVR_WARN(
				"CreateOVRSurface Failed to create OVR surface! Null material passed to CreateOVRSurface(...)!");
		return surfaceInfo;
	}

	std::shared_ptr<scr::Mesh> mesh = GetMesh();
	if (!mesh)
	{
		OVR_WARN("CreateOVRSurface Failed to create OVR surface! OVRNode has no mesh!");
		return surfaceInfo;
	}

	const scr::Mesh::MeshCreateInfo &meshCI = mesh->GetMeshCreateInfo();
	if (materialIndex >= meshCI.vb.size() || materialIndex >= meshCI.ib.size())
	{
		OVR_LOG("CreateOVRSurface Failed to create OVR surface!\nMaterial index %zu greater than amount of mesh buffers: %zu Vertex | %zu Index",
				materialIndex, meshCI.vb.size(), meshCI.ib.size());
		return surfaceInfo;
	}

	///MESH

	std::shared_ptr<scc::GL_VertexBuffer> gl_vb = std::dynamic_pointer_cast<scc::GL_VertexBuffer>(
			meshCI.vb[materialIndex]);
	std::shared_ptr<scc::GL_IndexBuffer>  gl_ib = std::dynamic_pointer_cast<scc::GL_IndexBuffer>(
			meshCI.ib[materialIndex]);

	if (!gl_vb)
	{
		OVR_LOG("CreateOVRSurface Failed to create OVR surface!\nNo vertex buffer to create OVR surface for material: %zu",
				materialIndex);
		return surfaceInfo;
	}

	if (!gl_ib)
	{
		OVR_LOG("CreateOVRSurface Failed to create OVR surface!\nNo index buffer to create OVR surface for material: %zu",
				materialIndex);
		return surfaceInfo;
	}

	gl_vb->CreateVAO(gl_ib->GetIndexID());

	std::shared_ptr<scc::GL_Skin> skin = std::dynamic_pointer_cast<scc::GL_Skin>(GetSkin());

	//Update material with Android-specific state.
	scr::Material::MaterialCreateInfo &materialCI = material->GetMaterialCreateInfo();
	materialCI.effect = &globalGraphicsResources.defaultPBREffect;

	if (materialCI.diffuse.texture)
	{
		materialCI.diffuse.texture->UseSampler(globalGraphicsResources.sampler);
	}
	if (materialCI.normal.texture)
	{
		materialCI.normal.texture->UseSampler(globalGraphicsResources.sampler);
	}
	if (materialCI.combined.texture)
	{
		materialCI.combined.texture->UseSampler(globalGraphicsResources.sampler);
	}
	if (materialCI.emissive.texture)
	{
		materialCI.emissive.texture->UseSampler(globalGraphicsResources.sampler);
	}

	//Get effect pass create info.
	std::string completePassName = GetCompleteEffectPassName(
			globalGraphicsResources.effectPassName);
	const scc::GL_Effect                    &gl_effect            = globalGraphicsResources.defaultPBREffect;
	const scr::Effect::EffectPassCreateInfo *effectPassCreateInfo = gl_effect.GetEffectPassCreateInfo(
			completePassName.c_str());

	//Construct OVR::GLGeometry from mesh data.
	OVRFW::GlGeometry geo;
	geo.vertexBuffer      = gl_vb->GetVertexID();
	geo.indexBuffer       = gl_ib->GetIndexID();
	geo.vertexArrayObject = gl_vb->GetVertexArrayID();
	geo.primitiveType     = scc::GL_Effect::ToGLTopology(effectPassCreateInfo->topology);
	geo.vertexCount       = (int) gl_vb->GetVertexCount();
	geo.indexCount        = (int) gl_ib->GetIndexBufferCreateInfo().indexCount;
	OVRFW::GlGeometry::IndexType = gl_ib->GetIndexBufferCreateInfo().stride == 4 ? GL_UNSIGNED_INT :
								   gl_ib->GetIndexBufferCreateInfo().stride == 2 ? GL_UNSIGNED_SHORT
																				 : GL_UNSIGNED_BYTE;

	OVRFW::ovrSurfaceDef &surfaceDef = surfaceInfo.surfaceDef;

	//Initialise surface definition.
	std::string _nodeName = std::string("NodeID: ") + std::to_string(id);
	surfaceDef.surfaceName  = _nodeName;
	surfaceDef.numInstances = 1;
	surfaceDef.geo          = geo;

	//Set Rendering Set
	if (effectPassCreateInfo)
	{
		surfaceDef.graphicsCommand.GpuState.blendMode      = scc::GL_Effect::ToGLBlendOp(
				effectPassCreateInfo->colourBlendingState.colorBlendOp);
		surfaceDef.graphicsCommand.GpuState.blendSrc       = scc::GL_Effect::ToGLBlendFactor(
				effectPassCreateInfo->colourBlendingState.srcColorBlendFactor);
		surfaceDef.graphicsCommand.GpuState.blendDst       = scc::GL_Effect::ToGLBlendFactor(
				effectPassCreateInfo->colourBlendingState.dstColorBlendFactor);
		surfaceDef.graphicsCommand.GpuState.blendModeAlpha = scc::GL_Effect::ToGLBlendOp(
				effectPassCreateInfo->colourBlendingState.alphaBlendOp);
		surfaceDef.graphicsCommand.GpuState.blendSrcAlpha  = scc::GL_Effect::ToGLBlendFactor(
				effectPassCreateInfo->colourBlendingState.srcAlphaBlendFactor);
		surfaceDef.graphicsCommand.GpuState.blendDstAlpha  = scc::GL_Effect::ToGLBlendFactor(
				effectPassCreateInfo->colourBlendingState.dstAlphaBlendFactor);
		surfaceDef.graphicsCommand.GpuState.depthFunc      = scc::GL_Effect::ToGLCompareOp(
				effectPassCreateInfo->depthStencilingState.depthCompareOp);

		surfaceDef.graphicsCommand.GpuState.frontFace       = GL_CW;
		surfaceDef.graphicsCommand.GpuState.polygonMode     = scc::GL_Effect::ToGLPolygonMode(
				effectPassCreateInfo->rasterizationState.polygonMode);
		surfaceDef.graphicsCommand.GpuState.blendEnable     = effectPassCreateInfo->colourBlendingState.blendEnable
															  ? OVRFW::ovrGpuState::ovrBlendEnable::BLEND_ENABLE
															  : OVRFW::ovrGpuState::ovrBlendEnable::BLEND_DISABLE;
		surfaceDef.graphicsCommand.GpuState.depthEnable     = effectPassCreateInfo->depthStencilingState.depthTestEnable;
		surfaceDef.graphicsCommand.GpuState.depthMaskEnable = true;
		surfaceDef.graphicsCommand.GpuState.colorMaskEnable[0] = true;
		surfaceDef.graphicsCommand.GpuState.colorMaskEnable[1] = true;
		surfaceDef.graphicsCommand.GpuState.colorMaskEnable[2] = true;
		surfaceDef.graphicsCommand.GpuState.colorMaskEnable[3] = true;
		surfaceDef.graphicsCommand.GpuState.polygonOffsetEnable = false;
		surfaceDef.graphicsCommand.GpuState.cullEnable          =
				effectPassCreateInfo->rasterizationState.cullMode != scr::Effect::CullMode::NONE;
		surfaceDef.graphicsCommand.GpuState.lineWidth           = 1.0F;
		surfaceDef.graphicsCommand.GpuState.depthRange[0] = effectPassCreateInfo->depthStencilingState.minDepthBounds;
		surfaceDef.graphicsCommand.GpuState.depthRange[1] = effectPassCreateInfo->depthStencilingState.maxDepthBounds;
	}

	perMeshInstanceData.u_LightmapScaleOffset = lightmapScaleOffset;
	scr::UniformBuffer::UniformBufferCreateInfo ub_ci;
	ub_ci.bindingLocation = 5;
	ub_ci.size = sizeof(PerMeshInstanceData);
	ub_ci.data =  &perMeshInstanceData;
	s_perMeshInstanceUniformBuffer = globalGraphicsResources.renderPlatform.InstantiateUniformBuffer();
	s_perMeshInstanceUniformBuffer->Create(&ub_ci);
	s_perMeshInstanceUniformBuffer->Update();

	scr::ShaderResourceLayout perMeshInstanceBufferLayout;
	perMeshInstanceBufferLayout.AddBinding(5,
										   scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,
										   scr::Shader::Stage::SHADER_STAGE_VERTEX);
	perMeshInstanceShaderResource.SetLayout(perMeshInstanceBufferLayout);
	perMeshInstanceShaderResource.AddBuffer(scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER,
											5,"u_PerMeshInstanceData"
			,{ s_perMeshInstanceUniformBuffer.get(), 0, sizeof(PerMeshInstanceData) });

	//Fill shader resources vector.
	std::vector<const scr::ShaderResource *> pbrShaderResources;
	pbrShaderResources.push_back(&globalGraphicsResources.scrCamera->GetShaderResource());
	pbrShaderResources.push_back(&globalGraphicsResources.tagShaderResource);
	pbrShaderResources.push_back(
			&(skin ? skin->GetShaderResource()
				   : globalGraphicsResources.defaultSkin.GetShaderResource()));
	pbrShaderResources.push_back(&perMeshInstanceShaderResource);
	pbrShaderResources.push_back(&material->GetShaderResource());
	pbrShaderResources.push_back(&globalGraphicsResources.lightCubemapShaderResources);

	//Set image samplers and uniform buffers.
	size_t                        resourceCount = 0;
	GLint                         textureCount  = 0, uniformCount = 0, storageBufferCount = 0;
	size_t                        j             = 0;
	for (const scr::ShaderResource *sr : pbrShaderResources)
	{
		const std::vector<scr::ShaderResource::WriteShaderResource> &shaderResourceSet = sr->GetWriteShaderResources();
		for (const scr::ShaderResource::WriteShaderResource &resource : shaderResourceSet)
		{
			scr::ShaderResourceLayout::ShaderResourceType type = resource.shaderResourceType;
			if (type == scr::ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER)
			{
				if (resource.imageInfo.texture)
				{
					auto gl_texture = dynamic_cast<scc::GL_Texture *>(resource.imageInfo.texture.get());
					surfaceDef.graphicsCommand.UniformData[j].Data = &(gl_texture->GetGlTexture());
					textureCount++;
				}
			}
			else if (type == scr::ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER)
			{
				if (resource.bufferInfo.buffer)
				{
					scc::GL_UniformBuffer *gl_uniformBuffer = static_cast<scc::GL_UniformBuffer *>(resource.bufferInfo.buffer);
					surfaceDef.graphicsCommand.UniformData[j].Data = &(gl_uniformBuffer->GetGlBuffer());
					uniformCount++;
				}
			}
			else if (type == scr::ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER)
			{
				if (resource.bufferInfo.buffer)
				{
					scc::GL_ShaderStorageBuffer *gl_storageBuffer = static_cast<scc::GL_ShaderStorageBuffer *>(resource.bufferInfo.buffer);

					surfaceDef.graphicsCommand.UniformData[j].Data = &(gl_storageBuffer->GetGlBuffer());
					storageBufferCount++;
				}
			}
			j++;
			resourceCount++;
			assert(resourceCount <= OVRFW::ovrUniform::MAX_UNIFORMS);
			assert(textureCount <= globalGraphicsResources.maxFragTextureSlots);
			assert(uniformCount <= globalGraphicsResources.maxFragUniformBlocks);
		}
	}

	OVR_LOG("CreateOVRSurface Created OVR surface! Effect pass %s resource %d", passname.c_str(),
			(int) j);
	return surfaceInfo;
}

const scr::ShaderResource& OVRNode::GetPerMeshInstanceShaderResource(const PerMeshInstanceData &p) const
{
	// I THINK this updates the values on the GPU...
	s_perMeshInstanceUniformBuffer->Update();
	return perMeshInstanceShaderResource;
}

void OVRNode::RefreshOVRSurfaces()
{
	surfaceDefinitions.clear();

	//We can't create surfaces without a mesh, so we should leave the list empty.
	std::shared_ptr<scr::Mesh> mesh = GetMesh();
	if(!mesh)
	{
		return;
	}

	std::vector<std::shared_ptr<scr::Material>> materials = GetMaterials();
	surfaceDefinitions.resize(materials.size());
	for(size_t i = 0; i < materials.size(); i++)
	{
		surfaceDefinitions[i] = CreateOVRSurface(i, materials[i]);
	}
}