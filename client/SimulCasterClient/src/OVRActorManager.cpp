#include "OVRActorManager.h"

#include "GlGeometry.h"
#include "OVR_LogUtils.h"

#include "SCR_Class_GL_Impl/GL_Effect.h"
#include "SCR_Class_GL_Impl/GL_IndexBuffer.h"
#include "SCR_Class_GL_Impl/GL_Texture.h"
#include "SCR_Class_GL_Impl/GL_UniformBuffer.h"
#include "SCR_Class_GL_Impl/GL_VertexBuffer.h"

#include "GlobalGraphicsResources.h"

using namespace OVR;
using namespace scc;
using namespace scr;

void OVRActor::SetMesh(std::shared_ptr<scr::Mesh> mesh)
{
    Node::SetMesh(mesh);

    //Recreate surfaces for new mesh.
    std::vector<std::shared_ptr<Material>> materials = GetMaterials();
    SetMaterialList(materials);
}

void OVRActor::SetMaterial(size_t index, std::shared_ptr<scr::Material> material)
{
    Node::SetMaterial(index, material);
	ovrSurfaceDefs[index] = CreateOVRSurface(index, material);
}

void OVRActor::SetMaterialListSize(size_t size)
{
	Node::SetMaterialListSize(size);

	size_t old_size = ovrSurfaceDefs.size();
	ovrSurfaceDefs.resize(size);
	if(old_size < size)
	{
        GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

	    //We don't know have the information on the material yet, so we use placeholder OVR surfaces.
		for(size_t i = old_size; i < size; i++)
		{
			ovrSurfaceDefs[i] = CreateOVRSurface(i, globalGraphicsResources.renderPlatform.placeholderMaterial);
		}
	}
}

void OVRActor::SetMaterialList(std::vector<std::shared_ptr<scr::Material>>& materials)
{
	Node::SetMaterialList(materials);

	ovrSurfaceDefs.clear();
	ovrSurfaceDefs.resize(materials.size());
	for(size_t i = 0; i < materials.size(); i++)
	{
		ovrSurfaceDefs[i] = CreateOVRSurface(i, materials[i]);
	}
}

OVR::ovrSurfaceDef OVRActor::CreateOVRSurface(size_t materialIndex, std::shared_ptr<Material> material)
{
	ovrSurfaceDef ovr_surface_def;
    if(material == nullptr)
    {
        OVR_WARN("Failed to create OVR surface!\nNull material passed to CreateOVRSurface(...).");
        return ovr_surface_def;
    }
    if(mesh==nullptr)
    {
        OVR_WARN("Mesh is null in CreateOVRSurface(...).");
        return ovr_surface_def;
    }
    GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

    const Mesh::MeshCreateInfo &meshCI = mesh->GetMeshCreateInfo();
    Material::MaterialCreateInfo &materialCI = material->GetMaterialCreateInfo();
    if(materialIndex >= meshCI.vb.size() || materialIndex >= meshCI.ib.size())
    {
        OVR_LOG("Failed to create OVR surface!\nMaterial index %zu greater than amount of mesh buffers: %zu Vertex | %zu Index", materialIndex, meshCI.vb.size(), meshCI.ib.size());
        return ovr_surface_def;
    }
    materialCI.effect = globalGraphicsResources.GetPbrEffect();

    //Mesh.
    // The first instance of vb/ib should be adequate to get the information needed.
    std::shared_ptr<GL_VertexBuffer> gl_vb = std::dynamic_pointer_cast<GL_VertexBuffer>(meshCI.vb[materialIndex]);
	std::shared_ptr<GL_IndexBuffer> gl_ib = std::dynamic_pointer_cast<GL_IndexBuffer>(meshCI.ib[materialIndex]);

	if(!gl_vb)
    {
        OVR_LOG("Failed to create OVR surface!\nNo vertex buffer to create OVR surface for material: %zu", materialIndex);
        return ovr_surface_def;
    }

	if(!gl_ib)
    {
        OVR_LOG("Failed to create OVR surface!\nNo index buffer to create OVR surface for material: %zu", materialIndex);
	    return ovr_surface_def;
    }

    gl_vb->CreateVAO(gl_ib->GetIndexID());

    //Material
    std::vector<const ShaderResource*> pbrShaderResources;
    pbrShaderResources.push_back(&(globalGraphicsResources.scrCamera->GetShaderResource()));
    pbrShaderResources.push_back(&(material->GetShaderResource()));
    pbrShaderResources.push_back(&(globalGraphicsResources.lightCubemapShaderResources));
    const scc::GL_Effect& gl_effect = globalGraphicsResources.pbrEffect;
    const scr::Effect::EffectPassCreateInfo* gl_effectPass = gl_effect.GetEffectPassCreateInfo(globalGraphicsResources.effectPassName);

    if(materialCI.diffuse.texture)
        materialCI.diffuse.texture->UseSampler(globalGraphicsResources.sampler);
    if(materialCI.normal.texture)
        materialCI.normal.texture->UseSampler(globalGraphicsResources.sampler);
    if(materialCI.combined.texture)
        materialCI.combined.texture->UseSampler(globalGraphicsResources.sampler);

    //----Set OVR Node----//
    //Construct Mesh
    GlGeometry geo;
    geo.vertexBuffer = gl_vb->GetVertexID();
    geo.indexBuffer = gl_ib->GetIndexID();
    geo.vertexArrayObject = gl_vb->GetVertexArrayID();
    if(gl_effectPass)
        geo.primitiveType = GL_Effect::ToGLTopology(gl_effectPass->topology);
    geo.vertexCount = (int)gl_vb->GetVertexCount();
    geo.indexCount = (int)gl_ib->GetIndexBufferCreateInfo().indexCount;
    GlGeometry::IndexType = gl_ib->GetIndexBufferCreateInfo().stride == 4 ? GL_UNSIGNED_INT : gl_ib->GetIndexBufferCreateInfo().stride == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

    //Initialise OVR Node
    std::string _actorName = std::string("ActorUID: ") + std::to_string(id);
    ovr_surface_def.surfaceName = _actorName;
    ovr_surface_def.numInstances = 1;
    ovr_surface_def.geo = geo;

    //Set Shader Program
    ovr_surface_def.graphicsCommand.Program = *gl_effect.GetGlPlatform(globalGraphicsResources.effectPassName);

    //Set Rendering Set
    if(gl_effectPass)
    {
        ovr_surface_def.graphicsCommand.GpuState.blendMode      = GL_Effect::ToGLBlendOp(
                gl_effectPass->colourBlendingState.colorBlendOp);
        ovr_surface_def.graphicsCommand.GpuState.blendSrc       = GL_Effect::ToGLBlendFactor(
                gl_effectPass->colourBlendingState.srcColorBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendDst       = GL_Effect::ToGLBlendFactor(
                gl_effectPass->colourBlendingState.dstColorBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendModeAlpha = GL_Effect::ToGLBlendOp(
                gl_effectPass->colourBlendingState.alphaBlendOp);
        ovr_surface_def.graphicsCommand.GpuState.blendSrcAlpha  = GL_Effect::ToGLBlendFactor(
                gl_effectPass->colourBlendingState.srcAlphaBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendDstAlpha  = GL_Effect::ToGLBlendFactor(
                gl_effectPass->colourBlendingState.dstAlphaBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.depthFunc      = GL_Effect::ToGLCompareOp(
                gl_effectPass->depthStencilingState.depthCompareOp);

        ovr_surface_def.graphicsCommand.GpuState.frontFace       = GL_CCW;
        ovr_surface_def.graphicsCommand.GpuState.polygonMode     = GL_Effect::ToGLPolygonMode(
                gl_effectPass->rasterizationState.polygonMode);
        ovr_surface_def.graphicsCommand.GpuState.blendEnable     = gl_effectPass->colourBlendingState.blendEnable
                                                                   ? ovrGpuState::ovrBlendEnable::BLEND_ENABLE
                                                                   : ovrGpuState::ovrBlendEnable::BLEND_DISABLE;
        ovr_surface_def.graphicsCommand.GpuState.depthEnable     = gl_effectPass->depthStencilingState.depthTestEnable;
        ovr_surface_def.graphicsCommand.GpuState.depthMaskEnable = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[0] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[1] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[2] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[3] = true;
        ovr_surface_def.graphicsCommand.GpuState.polygonOffsetEnable = false;
        ovr_surface_def.graphicsCommand.GpuState.cullEnable          =
                gl_effectPass->rasterizationState.cullMode != Effect::CullMode::NONE;
        ovr_surface_def.graphicsCommand.GpuState.lineWidth           = 1.0F;
        ovr_surface_def.graphicsCommand.GpuState.depthRange[0] = gl_effectPass->depthStencilingState.minDepthBounds;
        ovr_surface_def.graphicsCommand.GpuState.depthRange[1] = gl_effectPass->depthStencilingState.maxDepthBounds;
    }
    //Update Uniforms and Textures
    size_t resourceCount = 0;
    GLint textureCount = 0, uniformCount = 0;
    size_t j = 0;
    for(const ShaderResource *sr : pbrShaderResources)
    {
        const std::vector<ShaderResource::WriteShaderResource> &shaderResourceSet = sr->GetWriteShaderResources();
        for(auto &resource : shaderResourceSet)
        {
            ShaderResourceLayout::ShaderResourceType type = resource.shaderResourceType;
            if(type == ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER)
            {
                if(resource.imageInfo.texture.get())
                {
                    auto gl_texture = dynamic_cast<GL_Texture *>(resource.imageInfo.texture.get());
                    ovr_surface_def.graphicsCommand.UniformData[j].Data = &(gl_texture->GetGlTexture());
                    textureCount++;
                }
            }
            else if(type == ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER)
            {
                if(resource.bufferInfo.buffer)
                {
                    GL_UniformBuffer *gl_uniformBuffer = static_cast<GL_UniformBuffer *>(resource.bufferInfo.buffer);
                    ovr_surface_def.graphicsCommand.UniformData[j].Data = &(gl_uniformBuffer->GetGlBuffer());
                    uniformCount++;
                }
            }
            else
            {
                //NULL
            }
            j++;
            resourceCount++;
            assert(resourceCount <= ovrUniform::MAX_UNIFORMS);
            assert(textureCount <= globalGraphicsResources.maxFragTextureSlots);
            assert(uniformCount <= globalGraphicsResources.maxFragUniformBlocks);
        }
    }

    return ovr_surface_def;
}

std::shared_ptr<scr::Node> OVRActorManager::CreateActor(avs::uid id) const
{
    return std::make_shared<OVRActor>(id);
}

void OVRActorManager::AddActor(std::shared_ptr<Node> actor, bool isHand)
{
	ActorManager::AddActor(actor, isHand);
}

void OVRActorManager::ChangeEffectPass(const char* effectPassName)
{
    GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

    //Early-out if this we aren't actually changing the effect pass.
    if(strcmp(globalGraphicsResources.effectPassName, effectPassName) == 0)
    {
        return;
    }

    globalGraphicsResources.effectPassName = const_cast<char*>(effectPassName);
    OVR::GlProgram* effectPass = globalGraphicsResources.pbrEffect.GetGlPlatform(effectPassName);

    //Change effect used by all actors/surfaces.
    for(auto& actorPair : actorLookup)
    {
        for(auto& surface : std::static_pointer_cast<OVRActor>(actorPair.second)->ovrSurfaceDefs)
        {
            surface.graphicsCommand.Program = *effectPass;
        }
    }
}
