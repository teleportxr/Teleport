// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_Effect.h"
#include <OVR_GlUtils.h>
#include <OVR_LogUtils.h>

using namespace scc;
using namespace scr;
using namespace OVR;

#define OPENGLES_310 310

void GL_Effect::Create(EffectCreateInfo* pEffectCreateInfo)
{
    m_CI = *pEffectCreateInfo;
}
void GL_Effect::CreatePass(EffectPassCreateInfo* pEffectCreateInfo)
{
    m_EffectPasses[pEffectCreateInfo->effectPassName] = *pEffectCreateInfo;
}

void GL_Effect::LinkShaders(const char* effectPassName, const std::vector<ShaderResource>& shaderResources)
{
    Shader* vertex = nullptr;
    Shader* fragment = nullptr;

    ShaderSystem::Pipeline& pipeline = m_EffectPasses.at(effectPassName).pipeline;
    if(pipeline.m_Type == ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS)
    {
        for(size_t i = 0; i < pipeline.m_ShaderCount; i++)
        {
            if(pipeline.m_Shaders[i]->GetShaderCreateInfo().stage == Shader::Stage ::SHADER_STAGE_VERTEX)
                vertex = pipeline.m_Shaders[i].get();
            else if(pipeline.m_Shaders[i]->GetShaderCreateInfo().stage == Shader::Stage ::SHADER_STAGE_FRAGMENT)
                fragment = pipeline.m_Shaders[i].get();
            else
                continue;
        }
    }
    else
    {
		//Compile compute shader
		const auto &sc=pipeline.m_Shaders[0]->GetShaderCreateInfo();
		GLuint id = glCreateShader(GL_COMPUTE_SHADER);
		const char* source = sc.sourceCode.c_str();
		glShaderSource(id, 1, &source, nullptr);
		glCompileShader(id);
		GLint isCompiled = 0;
		glGetShaderiv(id, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength);
			// The maxLength includes the NULL character
			std::vector<GLchar> errorLog(maxLength);
			glGetShaderInfoLog(id, maxLength, &maxLength, &errorLog[0]);
			OVR_FAIL("%s",(const char*)errorLog.data());
			OVR_DEBUG_BREAK;
			glDeleteShader(id);
			return;
		}

		//Build compute program
		GLuint program = glCreateProgram();
		glAttachShader(program, id);
		glLinkProgram(program);
		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			// The maxLength includes the NULL character
			std::vector<GLchar> errorLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &errorLog[0]);
			OVR_FAIL("%s",(const char*)errorLog.data());
			OVR_DEBUG_BREAK;
			glDeleteProgram(program);
			return;
		}
		glValidateProgram(program);
		glDeleteShader(id);
		m_Program.Program = program;
        return;
    }

    assert(vertex != nullptr && fragment != nullptr);

    std::vector<ovrProgramParm> uniformParms;
    for(const auto& shaderResource : shaderResources )
    {
        for(const auto& resource : shaderResource.GetWriteShaderResources())
        {
            const char* name = resource.shaderResourceName;
            ovrProgramParmType type = ToOVRProgramParmType(resource.shaderResourceType);
            assert(type != ovrProgramParmType::MAX);
            uniformParms.push_back({name, type});
        }
    }
    const char* vertSrc = nullptr;
    const char* fragSrc = nullptr;
    const size_t maxStringLiteralSize = 4096;
    bool vertHeapAlloc = vertex->GetShaderCreateInfo().sourceCode.size() > maxStringLiteralSize;
    bool fragHeapAlloc = fragment->GetShaderCreateInfo().sourceCode.size() > maxStringLiteralSize;

    if(vertHeapAlloc)
        vertSrc = new char[vertex->GetShaderCreateInfo().sourceCode.size()];
    if(fragHeapAlloc)
        fragSrc = new char[fragment->GetShaderCreateInfo().sourceCode.size()];

    vertSrc = vertex->GetShaderCreateInfo().sourceCode.c_str();
    fragSrc = fragment->GetShaderCreateInfo().sourceCode.c_str();

    m_Program = GlProgram::Build(vertSrc, fragSrc, uniformParms.data(), (int)uniformParms.size(), OPENGLES_310);

    if(vertHeapAlloc)
        delete[] vertSrc;
    if(fragHeapAlloc)
        delete[] fragSrc;

}

void GL_Effect::Bind(const char* effectPassName) const
{
    /*EffectPassCreateInfo& epci = m_EffectPasses[effectPassName];


    //!Viewport and Scissor State!
    glEnable(GL_DEPTH_TEST | GL_SCISSOR_TEST);
    glViewport((GLint)epci.viewportAndScissor.x, (GLint)epci.viewportAndScissor.y, (GLint)epci.viewportAndScissor.width, (GLint)epci.viewportAndScissor.height);
    glDepthRangef(epci.viewportAndScissor.minDepth, epci.viewportAndScissor.maxDepth);
    glScissor(epci.viewportAndScissor.offsetX, epci.viewportAndScissor.offsetY, epci.viewportAndScissor.extentX, epci.viewportAndScissor.extentY);

    //!Rasterization State!
    glEnable(GL_CULL_FACE);
    glFrontFace(epci.rasterizationState.frontFace == FrontFace::COUNTER_CLOCKWISE ? GL_CCW : GL_CW);
    glCullFace(ToGLCullMode(epci.rasterizationState.cullMode));

    if(epci.rasterizationState.rasterizerDiscardEnable)
        glEnable(GL_RASTERIZER_DISCARD);
    else
        glDisable(GL_RASTERIZER_DISCARD);

    //TODO: Not supported in current OpenGL ES 3.0 loader!
    //if(epci.rasterizationState.depthClampEnable);
    //    glEnable(GL_DEPTH_CLAMP);
    //else
    //    glDisable(GL_DEPTH_CLAMP);

    //!Multisample Sample State!
    //TODO: Not supported in current OpenGL ES 3.0 loader! GL_EXT_multisample_compatibility?

    //!Depth Stenciling State!
    if(epci.depthStencilingState.depthTestEnable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if(epci.depthStencilingState.depthWriteEnable)
        glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);

    glDepthFunc(ToGLCompareOp(epci.depthStencilingState.depthCompareOp));

    //TODO: Not supported in current OpenGL ES 3.0 loader! GL_EXT_depth_bounds_test
    //if(epci.depthStencilingState.depthBoundTestEnable) {
    //    glEnable(DEPTH_BOUNDS_TEST_EXT)
    //    glDepthBoundEXT(epci.depthStencilingState.minDepthBounds, epci.depthStencilingState.maxDepthBounds);
    //}
    //else
    //   glDisable(DEPTH_BOUNDS_TEST_EXT)

    if(epci.depthStencilingState.stencilTestEnable)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);

    glStencilOpSeparate(GL_FRONT,
            ToGLStencilCompareOp(epci.depthStencilingState.frontCompareOp.stencilFailOp),
            ToGLStencilCompareOp(epci.depthStencilingState.frontCompareOp.stencilPassDepthFailOp),
            ToGLStencilCompareOp(epci.depthStencilingState.frontCompareOp.passOp));

    glStencilOpSeparate(GL_BACK,
            ToGLStencilCompareOp(epci.depthStencilingState.backCompareOp.stencilFailOp),
            ToGLStencilCompareOp(epci.depthStencilingState.backCompareOp.stencilPassDepthFailOp),
            ToGLStencilCompareOp(epci.depthStencilingState.backCompareOp.passOp));

    //!Colour Blending State!
    if(epci.colourBlendingState.blendEnable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    glBlendEquationSeparate(ToGLBlendOp(epci.colourBlendingState.colorBlendOp), ToGLBlendOp(epci.colourBlendingState.alphaBlendOp));

    glBlendFuncSeparate(
            ToGLBlendFactor(epci.colourBlendingState.srcColorBlendFactor),
            ToGLBlendFactor(epci.colourBlendingState.dstColorBlendFactor),
            ToGLBlendFactor(epci.colourBlendingState.srcAlphaBlendFactor),
            ToGLBlendFactor(epci.colourBlendingState.dstAlphaBlendFactor));*/
}
void GL_Effect::Unbind(const char* effectPassName) const
{
    //NULL
}

GLenum GL_Effect::ToGLTopology(TopologyType topology)
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

GLenum GL_Effect::ToGLCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
    case CullMode::NONE:               return GL_BACK;
    case CullMode::FRONT_BIT:          return GL_FRONT;
    case CullMode::BACK_BIT:           return GL_BACK;
    case CullMode::FRONT_AND_BACK:     return GL_FRONT_AND_BACK;
    }
};

GLenum GL_Effect::ToGLPolygonMode(PolygonMode polygonMode)
{
    switch (polygonMode)
    {
        case PolygonMode::FILL:      return GL_FILL;
        case PolygonMode::LINE:      return GL_LINES;
        case PolygonMode::POINT:     return GL_POINTS;
    }
}

GLenum GL_Effect::ToGLCompareOp(CompareOp op)
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

GLenum GL_Effect::ToGLStencilCompareOp(StencilCompareOp op)
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

GLenum GL_Effect::ToGLBlendFactor(BlendFactor factor)
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
GLenum GL_Effect::ToGLBlendOp(BlendOp op)
{
    switch (op) {
        case BlendOp::ADD:                  return GL_FUNC_ADD;
        case BlendOp::SUBTRACT:             return GL_FUNC_SUBTRACT;
        case BlendOp::REVERSE_SUBTRACT:     return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::MIN:                  return GL_MIN;
        case BlendOp::MAX:                  return GL_MAX;
    }
};

ovrProgramParmType GL_Effect::ToOVRProgramParmType(ShaderResourceLayout::ShaderResourceType type)
{
    switch(type)
    {
        case ShaderResourceLayout::ShaderResourceType::SAMPLER:                  return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER:   return ovrProgramParmType::TEXTURE_SAMPLED;
        case ShaderResourceLayout::ShaderResourceType::SAMPLED_IMAGE:            return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::STORAGE_IMAGE:            return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::UNIFORM_TEXEL_BUFFER:     return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::STORAGE_TEXEL_BUFFER:     return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER:           return ovrProgramParmType::BUFFER_UNIFORM;
        case ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER:           return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER_DYNAMIC:   return ovrProgramParmType::BUFFER_UNIFORM;
        case ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER_DYNAMIC:   return ovrProgramParmType::MAX;
        case ShaderResourceLayout::ShaderResourceType::INPUT_ATTACHMENT:         return ovrProgramParmType::MAX;
    }
};