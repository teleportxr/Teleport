// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Pipeline.h"

using namespace scr;

void PC_Pipeline::Create(const std::vector<Shader*>& shaders,
            const VertexBufferLayout& layout,
            const TopologyType& topology,
            const ViewportAndScissor& viewportAndScissor,
            const RasterizationState& rasterization,
            const MultisamplingState& multisample,
            const DepthStencilingState& depthStenciling,
            const ColourBlendingState& colourBlending)
{
    m_Shaders = shaders;
    m_VertexLayout = layout;
    m_Topology = topology;
    m_ViewportAndScissor = viewportAndScissor;
    m_RasterizationState = rasterization;
    m_MultisamplingState = multisample;
    m_DepthStencilingState = depthStenciling;
    m_ColourBlendingState = colourBlending;
}

void PC_Pipeline::LinkShaders()
{
    size_t vertexShaderIndex = (size_t)-1;
    size_t fragmentShaderIndex = (size_t)-1;

    size_t i = 0;
    for(auto& shader : m_Shaders)
    {
        if(shader->GetStage() == Shader::Stage::SHADER_STAGE_VERTEX)
        {
            vertexShaderIndex = i;
        }
        else if(shader->GetStage() == Shader::Stage::SHADER_STAGE_FRAGMENT)
        {
            fragmentShaderIndex = i;
        }
        i++;

    }

    assert(vertexShaderIndex != - 1 && fragmentShaderIndex != -1);
}

void PC_Pipeline::Bind() const
{
}
void PC_Pipeline::Unbind() const
{
}
void PC_Pipeline::BindDescriptorSets(const std::vector<DescriptorSet>& descriptorSets)
{
}
void PC_Pipeline::Draw(size_t indexBufferCount)
{
}