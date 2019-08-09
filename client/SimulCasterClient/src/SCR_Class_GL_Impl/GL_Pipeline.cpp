// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_Pipeline.h"

using namespace scr;
using namespace OVR;

void GL_Pipeline::Create(const std::vector<Shader*>& shaders,
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

void GL_Pipeline::LinkShaders()
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

    m_Program = GlProgram::Build(m_Shaders[vertexShaderIndex]->GetSourceCode(), m_Shaders[fragmentShaderIndex]->GetSourceCode(), nullptr, 0);
}

void GL_Pipeline::Bind() const
{
    //!Viewport and Scissor State!
    glEnable(GL_DEPTH_TEST | GL_SCISSOR_TEST);
    glViewport((GLint)m_ViewportAndScissor.x, (GLint)m_ViewportAndScissor.y, (GLint)m_ViewportAndScissor.width, (GLint)m_ViewportAndScissor.height);
    glDepthRangef(m_ViewportAndScissor.minDepth, m_ViewportAndScissor.maxDepth);
    glScissor(m_ViewportAndScissor.offsetX, m_ViewportAndScissor.offsetY, m_ViewportAndScissor.extentX, m_ViewportAndScissor.extentY);

    //!Rasterization State!
    glEnable(GL_CULL_FACE);
    glFrontFace(m_RasterizationState.frontFace == FrontFace::COUNTER_CLOCKWISE ? GL_CCW : GL_CW);
    glCullFace(ToGLCullMode(m_RasterizationState.cullMode));

    if(m_RasterizationState.rasterizerDiscardEnable)
        glEnable(GL_RASTERIZER_DISCARD);
    else
        glDisable(GL_RASTERIZER_DISCARD);

    //TODO: Not supported in current OpenGL ES 3.0 loader!
    /*if(m_RasterizationState.depthClampEnable);
        glEnable(GL_DEPTH_CLAMP);
    else
        glDisable(GL_DEPTH_CLAMP);*/

    //!Multisample Sample State!
    //TODO: Not supported in current OpenGL ES 3.0 loader! GL_EXT_multisample_compatibility?

    //!Depth Stenciling State!
    if(m_DepthStencilingState.depthTestEnable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if(m_DepthStencilingState.depthWriteEnable)
        glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);

    glDepthFunc(ToGLCompareOp(m_DepthStencilingState.depthCompareOp));

    //TODO: Not supported in current OpenGL ES 3.0 loader! GL_EXT_depth_bounds_test
    /*if(m_DepthStencilingState.depthBoundTestEnable) {
        glEnable(DEPTH_BOUNDS_TEST_EXT)
        glDepthBoundEXT(m_DepthStencilingState.minDepthBounds, m_DepthStencilingState.maxDepthBounds);
    }
    else
        glDisable(DEPTH_BOUNDS_TEST_EXT)*/

    if(m_DepthStencilingState.stencilTestEnable)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);

    glStencilOpSeparate(GL_FRONT,
            ToGLStencilCompareOp(m_DepthStencilingState.frontCompareOp.stencilFailOp),
            ToGLStencilCompareOp(m_DepthStencilingState.frontCompareOp.stencilPassDepthFailOp),
            ToGLStencilCompareOp(m_DepthStencilingState.frontCompareOp.passOp));

    glStencilOpSeparate(GL_BACK,
            ToGLStencilCompareOp(m_DepthStencilingState.backCompareOp.stencilFailOp),
            ToGLStencilCompareOp(m_DepthStencilingState.backCompareOp.stencilPassDepthFailOp),
            ToGLStencilCompareOp(m_DepthStencilingState.backCompareOp.passOp));

    //!Colour Blending State!
    if(m_ColourBlendingState.blendEnable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    glBlendEquationSeparate(ToGLBlendOp(m_ColourBlendingState.colorBlendOp), ToGLBlendOp(m_ColourBlendingState.alphaBlendOp));

    glBlendFuncSeparate(
            ToGLBlendFactor(m_ColourBlendingState.srcColorBlendFactor),
            ToGLBlendFactor(m_ColourBlendingState.dstColorBlendFactor),
            ToGLBlendFactor(m_ColourBlendingState.srcAlphaBlendFactor),
            ToGLBlendFactor(m_ColourBlendingState.dstAlphaBlendFactor));
}
void GL_Pipeline::Unbind() const
{}

void GL_Pipeline::BindDescriptorSets(const std::vector<DescriptorSet>& descriptorSets)
{
    //TODO: Move to OpenGL ES 3.2 for explicit in-shader UniformBlockBinding with the 'binding = X' layout qualifier!
    //Set Uniforms for textures and UBOs!
    glUseProgram(m_Program.Program);
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
    }
}
void GL_Pipeline::Draw(size_t indexBufferCount)
{
    glDrawElements(ToGLTopology(m_Topology), (GLsizei)indexBufferCount, GL_UNSIGNED_INT, nullptr);
}

GLenum GL_Pipeline::ToGLTopology(TopologyType topology) const
{
    switch (topology)
    {
        case TopologyType::POINT_LIST:     return GL_POINTS;
        case TopologyType::LINE_LIST:      return GL_LINES;
        case TopologyType::LINE_STRIP:     return GL_LINE_STRIP;
        case TopologyType::TRIANGLE_LIST:  return GL_TRIANGLES;
        case TopologyType::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case TopologyType::TRIANGLE_FAN:   return GL_TRIANGLE_FAN;
    }
};

GLenum GL_Pipeline::ToGLCullMode(CullMode cullMode) const
{
    switch (cullMode)
    {
    case CullMode::NONE:               return GL_BACK;
    case CullMode::FRONT_BIT:          return GL_FRONT;
    case CullMode::BACK_BIT:           return GL_BACK;
    case CullMode::FRONT_AND_BACK:     return GL_FRONT_AND_BACK;
    }
};

GLenum GL_Pipeline::ToGLCompareOp(CompareOp op) const
{
    switch(op)
    {
        case CompareOp::NEVER:            return GL_NEVER;
        case CompareOp::LESS:             return GL_LESS;
        case CompareOp::EQUAL:            return GL_EQUAL;
        case CompareOp::LESS_OR_EQUAL:    return GL_LEQUAL;
        case CompareOp::GREATER:          return GL_GREATER;
        case CompareOp::NOT_EQUAL:        return GL_NOTEQUAL;
        case CompareOp::GREATER_OR_EQUAL: return GL_GEQUAL;
        case CompareOp::ALWAYS:           return GL_ALWAYS;
    }
};

GLenum GL_Pipeline::ToGLStencilCompareOp(StencilCompareOp op) const
{
    switch (op) {
        case StencilCompareOp::KEEP:                    return GL_KEEP;
        case StencilCompareOp::ZERO:                    return GL_ZERO;
        case StencilCompareOp::REPLACE:                 return GL_REPLACE;
        case StencilCompareOp::INCREMENT_AND_CLAMP:     return GL_INCR;
        case StencilCompareOp::DECREMENT_AND_CLAMP:     return GL_DECR;
        case StencilCompareOp::INVERT:                  return GL_INVERT;
        case StencilCompareOp::INCREMENT_AND_WRAP:      return GL_INCR_WRAP;
        case StencilCompareOp::DECREMENT_AND_WRAP:      return GL_DECR_WRAP;
    }
};

GLenum GL_Pipeline::ToGLBlendFactor(BlendFactor factor) const
{
    switch(factor)
    {
        case BlendFactor::ZERO:                return GL_ZERO;
        case BlendFactor::ONE:                 return GL_ONE;
        case BlendFactor::SRC_COLOR:           return GL_SRC_COLOR;
        case BlendFactor::ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DST_COLOR:           return GL_DST_COLOR;
        case BlendFactor::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SRC_ALPHA:           return GL_SRC_ALPHA;
        case BlendFactor::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DST_ALPHA:           return GL_DST_ALPHA;
        case BlendFactor::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
    }
};
GLenum GL_Pipeline::ToGLBlendOp(BlendOp op) const
{
    switch (op) {
        case BlendOp::ADD:                  return GL_FUNC_ADD;
        case BlendOp::SUBTRACT:             return GL_FUNC_SUBTRACT;
        case BlendOp::REVERSE_SUBTRACT:     return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::MIN:                  return GL_MIN;
        case BlendOp::MAX:                  return GL_MAX;
    }
};