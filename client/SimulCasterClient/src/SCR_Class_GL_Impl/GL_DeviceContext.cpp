// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_DeviceContext.h"

using namespace scc;
using namespace scr;

void GL_DeviceContext::Create(DeviceContextCreateInfo* pDeviceContextCreateInfo)
{
    m_CI = *pDeviceContextCreateInfo;
}

void GL_DeviceContext::Draw(InputCommand* pInputCommand)
{
    glDrawElements(m_Topology, m_IndexCount, m_Type, nullptr);
}
void GL_DeviceContext::DispatchCompute(InputCommand* pInputCommand)
{

}
void GL_DeviceContext::BeginFrame()
{

}
void GL_DeviceContext::EndFrame()
{

}
void GL_DeviceContext::ParseInputCommand(InputCommand* pInputCommand)
{
   /*
    //Set up for DescriptorSet binding
    std::vector<DescriptorSet> descriptorSets;
    Effect* effect = nullptr;

    //Default Init
    dynamic_cast<GL_FrameBuffer *>(pInputCommand->pFBs)[0].BeginFrame();
    pInputCommand->pCamera->UpdateCameraUBO();
    descriptorSets.push_back(pInputCommand->pCamera->GetDescriptorSet());

    //Switch for types
    switch (pInputCommand->type)
    {
        case INPUT_COMMAND:
        {
            //NULL No other command to execute.
            break;
        }
        case INPUT_COMMAND_MESH_MATERIAL_TRANSFORM:
        {
            InputCommand_Mesh_Material_Transform* ic_mmm = dynamic_cast<InputCommand_Mesh_Material_Transform*>(pInputCommand);

            //Mesh
            dynamic_cast<const GL_VertexBuffer*>(ic_mmm->pMesh->GetMeshCreateInfo().vb)->Bind();

            const GL_IndexBuffer* gl_IndexBuffer = dynamic_cast<const GL_IndexBuffer*>(ic_mmm->pMesh->GetMeshCreateInfo().ib);
            gl_IndexBuffer->Bind();
            m_IndexCount = static_cast<GLsizei>(gl_IndexBuffer->GetIndexBufferCreateInfo().indexCount);
            size_t stride = gl_IndexBuffer->GetIndexBufferCreateInfo().stride;
            m_Type = stride == 4 ? GL_UNSIGNED_INT : stride == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

            //Material
            descriptorSets.push_back(ic_mmm->pMaterial->GetDescriptorSet());
            effect = ic_mmm->pMaterial->GetEffect();
            dynamic_cast<GL_Effect*>(effect)->LinkShaders();
            dynamic_cast<const GL_Effect*>(effect)->Bind();
            m_Topology = dynamic_cast<const GL_Effect*>(effect)->ToGLTopology(effect->GetEffectCreateInfo().topology);

            //Transform
            descriptorSets.push_back(ic_mmm->pTransform->GetDescriptorSet());
            ic_mmm->pTransform->UpdateModelUBO();

            break;
        }
    }
    BindDescriptorSets(descriptorSets, effect);
    */

}

void GL_DeviceContext::BindDescriptorSets(const std::vector<DescriptorSet>& descriptorSets, Effect* pEffect)
{
    //TODO: Move to OpenGL ES 3.2 for explicit in-shader UniformBlockBinding with the 'binding = X' layout qualifier!

    if(!pEffect)
        return; //SCR_CERR_BREAK("Invalid effect. Can not bind descriptor sets!", -1);

    //Set Uniforms for textures and UBOs!
    GLuint& program = dynamic_cast<GL_Effect*>(pEffect)->GetGlPlatform().Program;
    glUseProgram(program);
    for(auto& ds : descriptorSets)
    {
        for(auto& wds : ds.GetWriteDescriptorSet())
        {
            DescriptorSetLayout::DescriptorType type = wds.descriptorType;
            if(type == DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER)
            {
                GLint location = glGetUniformLocation(program, wds.descriptorName);
                glUniform1i(location, wds.dstBinding);
            }
            else if(type == DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER)
            {
                GLuint location = glGetUniformBlockIndex(program, wds.descriptorName);
                glUniformBlockBinding(program, location, wds.dstBinding);
            }
            else
            {
                continue;
            }
        }
    }
    glUseProgram(0);

    //Bind Resources
    for(auto& ds : descriptorSets)
    {
        for(auto& wds : ds.GetWriteDescriptorSet())
        {
            DescriptorSetLayout::DescriptorType type = wds.descriptorType;
            if(type == DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER)
            {
                dynamic_cast<const GL_Texture*>(wds.pImageInfo->texture)->Bind();
            }
            else if(type == DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER)
            {
                dynamic_cast<const GL_UniformBuffer*>(wds.pBufferInfo->buffer)->Submit();
            }
            else
            {
                continue;
            }
        }
    }
}