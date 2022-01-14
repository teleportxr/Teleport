// (C) Copyright 2018-2022 Simul Software Ltd
#include "PC_RenderPlatform.h"

#include "ClientRender/Skin.h"

using namespace clientrender;

namespace pc_client
{
void PC_RenderPlatform::SetSimulRenderPlatform(simul::crossplatform::RenderPlatform* r)
{
	renderPlatform = r;
}

std::shared_ptr<clientrender::FrameBuffer> PC_RenderPlatform::InstantiateFrameBuffer() const
{
	return std::make_shared<PC_FrameBuffer>(this);
}
std::shared_ptr<clientrender::IndexBuffer> PC_RenderPlatform::InstantiateIndexBuffer() const
{
	return std::make_shared<PC_IndexBuffer>(this);
}
std::shared_ptr<clientrender::Effect> PC_RenderPlatform::InstantiateEffect() const
{
	return std::make_shared<PC_Effect>(this);
}
std::shared_ptr<clientrender::Sampler> PC_RenderPlatform::InstantiateSampler() const
{
	return std::make_shared<PC_Sampler>(this);
}
std::shared_ptr<clientrender::Shader> PC_RenderPlatform::InstantiateShader() const
{
	return std::make_shared<PC_Shader>(this);
}
std::shared_ptr<clientrender::ShaderStorageBuffer> PC_RenderPlatform::InstantiateShaderStorageBuffer() const
{
	return std::make_shared<PC_ShaderStorageBuffer>(this);
}

std::shared_ptr<clientrender::Skin> PC_RenderPlatform::InstantiateSkin(const std::string& name) const
{
	return std::make_shared<clientrender::Skin>(name);
}

std::shared_ptr<clientrender::Skin> PC_RenderPlatform::InstantiateSkin(const std::string& name, const std::vector<clientrender::mat4>& inverseBindMatrices, size_t boneAmount, const Transform& skinTransform) const
{
	return std::make_shared<clientrender::Skin>(name, inverseBindMatrices, boneAmount, skinTransform);
}

std::shared_ptr<clientrender::Texture> PC_RenderPlatform::InstantiateTexture() const
{
	return std::make_shared<PC_Texture>(this);
}
std::shared_ptr<clientrender::UniformBuffer> PC_RenderPlatform::InstantiateUniformBuffer() const
{
	return std::make_shared<PC_UniformBuffer>(this);
}
std::shared_ptr<clientrender::VertexBuffer> PC_RenderPlatform::InstantiateVertexBuffer() const
{
	return std::make_shared<PC_VertexBuffer>(this);
}
}
