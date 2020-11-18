// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_RenderPlatform.h"

#include "GL_DeviceContext.h"
#include "GL_Effect.h"
#include "GL_FrameBuffer.h"
#include "GL_IndexBuffer.h"
#include "GL_Sampler.h"
#include "GL_Shader.h"
#include "GL_ShaderStorageBuffer.h"
#include "GL_Skin.h"
#include "GL_Texture.h"
#include "GL_UniformBuffer.h"
#include "GL_VertexBuffer.h"

using namespace scc;
using namespace scr;

std::shared_ptr<FrameBuffer> GL_RenderPlatform::InstantiateFrameBuffer() const
{
    return std::make_shared<GL_FrameBuffer>(this);
}
std::shared_ptr<IndexBuffer> GL_RenderPlatform::InstantiateIndexBuffer() const
{
    return std::make_shared<GL_IndexBuffer>(this);
}
std::shared_ptr<Effect> GL_RenderPlatform::InstantiateEffect() const
{
    return std::make_shared<GL_Effect>(this);
}
std::shared_ptr<Sampler> GL_RenderPlatform::InstantiateSampler() const
{
    return std::make_shared<GL_Sampler>(this);
}
std::shared_ptr<Shader> GL_RenderPlatform::InstantiateShader() const
{
    return std::make_shared<GL_Shader>(this);
}
std::shared_ptr<ShaderStorageBuffer> GL_RenderPlatform::InstantiateShaderStorageBuffer() const
{
    return std::make_shared<GL_ShaderStorageBuffer>(this);
}

std::shared_ptr<scr::Skin> GL_RenderPlatform::InstantiateSkin(const std::string& name) const
{
    return std::make_shared<GL_Skin>(this, name);
}

std::shared_ptr<scr::Skin> GL_RenderPlatform::InstantiateSkin(const std::string& name, const std::vector<Transform>& inverseBindMatrices, size_t boneAmount, const Transform& skinTransform) const
{
	return std::make_shared<GL_Skin>(this, name, inverseBindMatrices, boneAmount, skinTransform);
}

std::shared_ptr<Texture> GL_RenderPlatform::InstantiateTexture() const
{
    return std::make_shared<GL_Texture>(this);
}
std::shared_ptr<UniformBuffer> GL_RenderPlatform::InstantiateUniformBuffer() const
{
    return std::make_shared<GL_UniformBuffer>(this);
}
std::shared_ptr<VertexBuffer>	GL_RenderPlatform::InstantiateVertexBuffer() const
{
    return std::make_shared<GL_VertexBuffer>(this);
}