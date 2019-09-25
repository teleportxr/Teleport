// (C) Copyright 2018-2019 Simul Software Ltd

#include "GL_Sampler.h"

using namespace scc;
using namespace scr;

void GL_Sampler::Create(SamplerCreateInfo* pSamplerCreateInfo)
{
    m_CI = *pSamplerCreateInfo;
}
void GL_Sampler::Destroy()
{
    //NULL
}

void GL_Sampler::Bind() const
{
    //NULL
}
void GL_Sampler::Unbind() const
{
    //NULL
}

GLenum GL_Sampler::ToGLFilterType(Filter filter) const
{
    switch (filter)
    {
        case Filter::NEAREST:           return GL_NEAREST;
        case Filter::LINEAR:            return GL_LINEAR;
        case Filter::MIPMAP_NEAREST:    return GL_NEAREST_MIPMAP_NEAREST;
        case Filter::MIPMAP_LINEAR:     return GL_LINEAR_MIPMAP_LINEAR;
        default:
            exit(1);
    }
};
GLenum GL_Sampler::ToGLWrapType(Wrap wrap) const
{
    switch(wrap)
    {
        case Wrap::REPEAT:                  return GL_REPEAT;
        case Wrap::MIRRORED_REPEAT:         return GL_MIRRORED_REPEAT;
        case Wrap::CLAMP_TO_EDGE:           return GL_CLAMP_TO_EDGE;
        case Wrap::CLAMP_TO_BORDER:         return GL_CLAMP_TO_BORDER;
        case Wrap::MIRROR_CLAMP_TO_EDGE:    return 0; //GL_MIRROR_CLAMP_TO_EDGE;
        default:
            exit(1);
    }
};
