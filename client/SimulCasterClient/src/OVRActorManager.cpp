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

std::vector<ovrSurfaceDef> CreateNativeActor(avs::uid actorID, const Actor::ActorCreateInfo& actorCreateInfo)
{
    GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();
    std::vector<ovrSurfaceDef> ovrSurfaceDefs;

    for(size_t i = 0; i < actorCreateInfo.materials.size(); i++)
    {
        //From Actor
        const Mesh::MeshCreateInfo& meshCI = actorCreateInfo.mesh->GetMeshCreateInfo();
        Material::MaterialCreateInfo& materialCI = actorCreateInfo.materials[i]->GetMaterialCreateInfo();
        if(i >= meshCI.vb.size() || i >= meshCI.ib.size())
        {
            OVR_LOG("Skipping empty element in mesh.");
            break; //This break; isn't working correctly
        }
        //Mesh.
        // The first instance of vb/ib should be adequate to get the information needed.
        const auto gl_vb = dynamic_cast<GL_VertexBuffer*>(meshCI.vb[i].get());
        const auto gl_ib = dynamic_cast<GL_IndexBuffer*>(meshCI.ib[i].get());
        gl_vb->CreateVAO(gl_ib->GetIndexID());

        //Material
        std::vector<ShaderResource> pbrShaderResources;
        pbrShaderResources.push_back(GlobalGraphicsResources.scrCamera->GetShaderResource());
        pbrShaderResources.push_back(actorCreateInfo.materials[i]->GetShaderResource());
        pbrShaderResources.push_back(GlobalGraphicsResources.lightCubemapShaderResources);

        materialCI.effect = dynamic_cast<Effect*>(&GlobalGraphicsResources.pbrEffect);
        const auto gl_effect = &GlobalGraphicsResources.pbrEffect;
        const auto gl_effectPass = gl_effect->GetEffectPassCreateInfo(GlobalGraphicsResources.effectPassName);
        if(materialCI.diffuse.texture)
        {
            materialCI.diffuse.texture->UseSampler(GlobalGraphicsResources.sampler);
        }
        if(materialCI.normal.texture)
        {
            materialCI.normal.texture->UseSampler(GlobalGraphicsResources.sampler);
        }
        if(materialCI.combined.texture)
        {
            materialCI.combined.texture->UseSampler(GlobalGraphicsResources.sampler);
        }

        //----Set OVR Actor----//
        //Construct Mesh
        GlGeometry geo;
        geo.vertexBuffer = gl_vb->GetVertexID();
        geo.indexBuffer = gl_ib->GetIndexID();
        geo.vertexArrayObject = gl_vb->GetVertexArrayID();
        geo.primitiveType = GL_Effect::ToGLTopology(gl_effectPass.topology);
        geo.vertexCount = (int)gl_vb->GetVertexCount();
        geo.indexCount = (int)gl_ib->GetIndexBufferCreateInfo().indexCount;
        GlGeometry::IndexType = gl_ib->GetIndexBufferCreateInfo().stride == 4 ? GL_UNSIGNED_INT : gl_ib->GetIndexBufferCreateInfo().stride == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

        //Initialise OVR Actor
        ovrSurfaceDef ovr_surface_def;
        std::string _actorName = std::string("ActorUID: ") + std::to_string(actorID);
        ovr_surface_def.surfaceName = _actorName;
        ovr_surface_def.numInstances = 1;
        ovr_surface_def.geo = geo;

        //Set Shader Program
        ovr_surface_def.graphicsCommand.Program = gl_effect->GetGlPlatform(GlobalGraphicsResources.effectPassName);

        //Set Rendering Set
        ovr_surface_def.graphicsCommand.GpuState.blendMode = GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.colorBlendOp);
        ovr_surface_def.graphicsCommand.GpuState.blendSrc = GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcColorBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendDst = GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstColorBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendModeAlpha = GL_Effect::ToGLBlendOp(gl_effectPass.colourBlendingState.alphaBlendOp);
        ovr_surface_def.graphicsCommand.GpuState.blendSrcAlpha = GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.srcAlphaBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.blendDstAlpha = GL_Effect::ToGLBlendFactor(gl_effectPass.colourBlendingState.dstAlphaBlendFactor);
        ovr_surface_def.graphicsCommand.GpuState.depthFunc = GL_Effect::ToGLCompareOp(gl_effectPass.depthStencilingState.depthCompareOp);

        ovr_surface_def.graphicsCommand.GpuState.frontFace = GL_CCW;
        ovr_surface_def.graphicsCommand.GpuState.polygonMode = GL_Effect::ToGLPolygonMode(gl_effectPass.rasterizationState.polygonMode);
        ovr_surface_def.graphicsCommand.GpuState.blendEnable = gl_effectPass.colourBlendingState.blendEnable ? ovrGpuState::ovrBlendEnable::BLEND_ENABLE : ovrGpuState::ovrBlendEnable::BLEND_DISABLE;
        ovr_surface_def.graphicsCommand.GpuState.depthEnable = gl_effectPass.depthStencilingState.depthTestEnable;
        ovr_surface_def.graphicsCommand.GpuState.depthMaskEnable = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[0] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[1] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[2] = true;
        ovr_surface_def.graphicsCommand.GpuState.colorMaskEnable[3] = true;
        ovr_surface_def.graphicsCommand.GpuState.polygonOffsetEnable = false;
        ovr_surface_def.graphicsCommand.GpuState.cullEnable = gl_effectPass.rasterizationState.cullMode != Effect::CullMode::NONE;
        ovr_surface_def.graphicsCommand.GpuState.lineWidth = 1.0F;
        ovr_surface_def.graphicsCommand.GpuState.depthRange[0] = gl_effectPass.depthStencilingState.minDepthBounds;
        ovr_surface_def.graphicsCommand.GpuState.depthRange[1] = gl_effectPass.depthStencilingState.maxDepthBounds;

        //Update Uniforms and Textures
        size_t resourceCount = 0;
        GLint textureCount = 0, uniformCount = 0;
        size_t j = 0;
        for(auto& sr : pbrShaderResources)
        {
            std::vector<ShaderResource::WriteShaderResource>& shaderResourceSet = sr.GetWriteShaderResources();
            for(auto& resource : shaderResourceSet)
            {
                ShaderResourceLayout::ShaderResourceType type = resource.shaderResourceType;
                if(type == ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER)
                {
                    if(resource.imageInfo.texture.get())
                    {
                        auto gl_texture = dynamic_cast<GL_Texture*>(resource.imageInfo.texture.get());
                        ovr_surface_def.graphicsCommand.UniformData[j].Data = &(gl_texture->GetGlTexture());
                        textureCount++;
                    }
                }
                else if(type == ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER)
                {
                    if(resource.bufferInfo.buffer)
                    {
                        GL_UniformBuffer* gl_uniformBuffer = static_cast<GL_UniformBuffer*>(resource.bufferInfo.buffer);
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
                assert(textureCount <= GlobalGraphicsResources.maxFragTextureSlots);
                assert(uniformCount <= GlobalGraphicsResources.maxFragUniformBlocks);
            }
        }
        ovrSurfaceDefs.emplace_back(std::move(ovr_surface_def));
    }
    return ovrSurfaceDefs;
}
void OVRActor::Init(const scr::Actor::ActorCreateInfo& actorCreateInfo)
{

    ovrSurfaceDefs=CreateNativeActor(id, actorCreateInfo);
}



void OVRActorManager::ChangeEffectPass(const char* effectPassName)
{
    //Early-out if this we aren't actually changing the effect pass.
    if(strcmp(GlobalGraphicsResources.effectPassName, effectPassName) == 0)
    {
        return;
    }

    GlobalGraphicsResources.effectPassName = const_cast<char*>(effectPassName);
    OVR::GlProgram& effectPass = GlobalGraphicsResources.pbrEffect.GetGlPlatform(effectPassName);

    //Change effect used by all actors/surfaces.
    for(auto& actorPair : actorLookup)
    {
        for(auto& surface : std::static_pointer_cast<OVRActor>(actorPair.second)->ovrSurfaceDefs)
        {
            surface.graphicsCommand.Program = effectPass;
        }
    }
}
