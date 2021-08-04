// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_Effect.h"
//#include <OVR_GlUtils.h>
#include <OVR_LogUtils.h>
#include <GLES3/gl32.h>

using namespace scc;
using namespace scr;
using namespace OVR;
using namespace OVRFW;

#define OPENGLES_310 310

GL_Effect::~GL_Effect()
{
    for(auto& programPair : m_EffectPrograms)
    {
        GlProgram::Free(programPair.second);
    }
}

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
    ShaderSystem::Pipeline& pipeline = m_EffectPasses.at(effectPassName).pipeline;
    switch(pipeline.m_Type)
    {
    case ShaderSystem::PipelineType::PIPELINE_TYPE_GRAPHICS:
        BuildGraphicsPipeline(effectPassName, pipeline, shaderResources);
        break;
    case ShaderSystem::PipelineType::PIPELINE_TYPE_COMPUTE:
        BuildComputePipeline(effectPassName, pipeline, shaderResources);
        break;
    default:
        SCR_LOG("Attempted to create a pipeline with an unset pipeline type!");
        assert(false);
        break;
    }
}

void GL_Effect::Bind(const char* effectPassName) const
{
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
		default:
			exit(1);
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
		default:
			exit(1);
    }
};

GLenum GL_Effect::ToGLPolygonMode(PolygonMode polygonMode)
{
    switch (polygonMode)
    {
        case PolygonMode::FILL:      return GL_FILL;
        case PolygonMode::LINE:      return GL_LINES;
        case PolygonMode::POINT:     return GL_POINTS;
		default:
			exit(1);
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
		default:
			exit(1);
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
		default:
			exit(1);
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
        default:
        	exit(1);
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
		default:
			exit(1);
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
        case ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER:           return ovrProgramParmType::BUFFER_STORAGE;
        case ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER_DYNAMIC:   return ovrProgramParmType::BUFFER_UNIFORM;
        case ShaderResourceLayout::ShaderResourceType::STORAGE_BUFFER_DYNAMIC:   return ovrProgramParmType::BUFFER_STORAGE;
        case ShaderResourceLayout::ShaderResourceType::INPUT_ATTACHMENT:         return ovrProgramParmType::MAX;
		default:
			exit(1);
    }
}
static const char *ToString(OVRFW::ovrProgramParmType t)
{
    switch (t)
    {
        case OVRFW::ovrProgramParmType::INT: return "int";
        case OVRFW::ovrProgramParmType::INT_VECTOR2: return "Vector2i";
        case OVRFW::ovrProgramParmType::INT_VECTOR3: return "Vector3i";
        case OVRFW::ovrProgramParmType::INT_VECTOR4: return "Vector4i";
        case OVRFW::ovrProgramParmType::FLOAT: return "float";
        case OVRFW::ovrProgramParmType::FLOAT_VECTOR2: return "Vector2f";
        case OVRFW::ovrProgramParmType::FLOAT_VECTOR3: return "Vector3f";
        case OVRFW::ovrProgramParmType::FLOAT_VECTOR4: return "Vector4f";
        case OVRFW::ovrProgramParmType::FLOAT_MATRIX4: return "Matrix4f";
        case OVRFW::ovrProgramParmType::TEXTURE_SAMPLED: return "GlTexture";
        case OVRFW::ovrProgramParmType::BUFFER_UNIFORM: return "read-only uniform buffer";
        case OVRFW::ovrProgramParmType::BUFFER_STORAGE: return "read-write storage buffer";
        default:
            return "";
    }
}
void GL_Effect::BuildGraphicsPipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources)
{
    Shader* vertex = nullptr;
    Shader* fragment = nullptr;

    for(size_t i = 0; i < pipeline.m_ShaderCount; i++)
    {
        if(pipeline.m_Shaders[i]->GetShaderCreateInfo().stage == Shader::Stage::SHADER_STAGE_VERTEX)
        {
            vertex = pipeline.m_Shaders[i].get();
        }
        else if(pipeline.m_Shaders[i]->GetShaderCreateInfo().stage == Shader::Stage::SHADER_STAGE_FRAGMENT)
        {
            fragment = pipeline.m_Shaders[i].get();
        }
    }

    assert(vertex != nullptr && fragment != nullptr);
    OVR_LOG("Linking %s ",effectPassName);
    int i=0;
    for(const auto& shaderResource : shaderResources)
    {
        for(const auto& resource : shaderResource.GetWriteShaderResources())
        {
            const char* name = resource.shaderResourceName;
            ovrProgramParmType type = ToOVRProgramParmType(resource.shaderResourceType);
            assert(type != ovrProgramParmType::MAX);
            uniformParms.push_back({name, type});
            OVR_LOG("Linking %d uniform %s %s ",i,ToString(type),name);
            i++;
        }
    }

    std::string vertSourceCode = "#define " + vertex->GetShaderCreateInfo().entryPoint + " main\r\n" + vertex->GetShaderCreateInfo().sourceCode;
    std::string fragSourceCode = "#define " + fragment->GetShaderCreateInfo().entryPoint + " main\r\n" + fragment->GetShaderCreateInfo().sourceCode;

    static std::string vertDir = "";
    static std::string fragDir = "";//#extension GL_EXT_shader_texture_lod : require\n";

    m_EffectPrograms.emplace
    (
            effectPassName,
            GlProgram::Build(vertDir.c_str(), vertSourceCode.c_str(), fragDir.c_str(), fragSourceCode.c_str(), uniformParms.data(), (int)uniformParms.size(), OPENGLES_310)
     );
}

void GL_Effect::BuildComputePipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources)
{
    std::string src = "#version 310 es\r\n";
    //Compile compute shader
    const auto& sc = pipeline.m_Shaders[0]->GetShaderCreateInfo();
    if(sc.entryPoint.length())
    {
        src += "#define ";
        src += sc.entryPoint;
        src += " main\r\n";
    }
    src += sc.sourceCode;

    GLuint id = glCreateShader(GL_COMPUTE_SHADER);
    const char* source = src.c_str();
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    GLint isCompiled = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength);
        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(id, maxLength, &maxLength, &errorLog[0]);
        OVR_FAIL("%s", (const char*)errorLog.data());
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
    if(isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        // The maxLength includes the NULL character
        std::vector<GLchar> errorLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &errorLog[0]);
        OVR_FAIL("%s", (const char*)errorLog.data());
        OVR_DEBUG_BREAK;
        glDeleteProgram(program);
        return;
    }
    glValidateProgram(program);
    glDeleteShader(id);
    m_EffectPrograms[effectPassName].Program = program;
}
