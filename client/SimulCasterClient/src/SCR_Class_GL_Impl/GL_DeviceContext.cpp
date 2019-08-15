// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_DeviceContext.h"

using namespace scc;
using namespace scr;

void GL_DeviceContext::BindDescriptorSets(const std::vector<DescriptorSet>& descriptorSets)
{
    //TODO: Move to OpenGL ES 3.2 for explicit in-shader UniformBlockBinding with the 'binding = X' layout qualifier!
    //Set Uniforms for textures and UBOs!
    /*glUseProgram(m_Program.Program);
    for(auto& ds : descriptorSets)
    {
        for(auto& wds : ds.GetWriteDescriptorSet())
        {
            DescriptorSetLayout::DescriptorType type = wds.descriptorType;
            if(type == DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER)
            {
                GLint location = glGetUniformLocation(m_Program.Program, wds.descriptorName);
                glUniform1i(location, wds.dstBinding);
            }
            else if(type == DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER)
            {
                GLuint location = glGetUniformBlockIndex(m_Program.Program, wds.descriptorName);
                glUniformBlockBinding(m_Program.Program, location, wds.dstBinding);
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
                wds.pImageInfo->texture->Bind();
            }
            else if(type == DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER)
            {
                wds.pBufferInfo->buffer->Submit();
            }
            else
            {
                continue;
            }
        }
    }*/
}
void GL_DeviceContext::Draw(InputCommand* pInputCommand)
{

}
void GL_DeviceContext::DispatchCompute(InputCommand* pInputCommand)
{

}
void BeginFrame()
{

}
void EndFrame()
{

}