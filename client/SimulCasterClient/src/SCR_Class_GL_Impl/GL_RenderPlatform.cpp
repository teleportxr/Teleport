// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_RenderPlatform.h"

using namespace scc;
using namespace scr;

std::shared_ptr<FrameBuffer> GL_RenderPlatform::InstantiateFrameBuffer()
{
    return std::make_shared<GL_FrameBuffer>(this);
}
std::shared_ptr<IndexBuffer> GL_RenderPlatform::InstantiateIndexBuffer()
{
    return std::make_shared<GL_IndexBuffer>(this);
}
std::shared_ptr<Effect> GL_RenderPlatform::InstantiateEffect()
{
    return std::make_shared<GL_Effect>(this);
}
std::shared_ptr<Sampler> GL_RenderPlatform::InstantiateSampler()
{
    return std::make_shared<GL_Sampler>(this);
}
std::shared_ptr<Shader> GL_RenderPlatform::InstantiateShader()
{
    return std::make_shared<GL_Shader>(this);
}
std::shared_ptr<Texture> GL_RenderPlatform::InstantiateTexture()
{
    return std::make_shared<GL_Texture>(this);
}
std::shared_ptr<UniformBuffer> GL_RenderPlatform::InstantiateUniformBuffer()
{
    return std::make_shared<GL_UniformBuffer>(this);
}
std::shared_ptr<VertexBuffer>	GL_RenderPlatform::InstantiateVertexBuffer()
{
    return std::make_shared<GL_VertexBuffer>(this);
}