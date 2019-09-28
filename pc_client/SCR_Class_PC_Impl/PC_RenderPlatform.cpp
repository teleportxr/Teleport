// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_RenderPlatform.h"

using namespace pc_client;
using namespace scr;

void PC_RenderPlatform::SetSimulRenderPlatform(simul::crossplatform::RenderPlatform *r)
{
	renderPlatform = r;
}

std::shared_ptr<scr::FrameBuffer> PC_RenderPlatform::InstantiateFrameBuffer() const
{
	return std::make_shared<PC_FrameBuffer>(this);
}
std::shared_ptr<scr::IndexBuffer> PC_RenderPlatform::InstantiateIndexBuffer() const
{
	return std::make_shared<PC_IndexBuffer>(this);
}
std::shared_ptr<scr::Effect> PC_RenderPlatform::InstantiateEffect() const
{
	return std::make_shared<PC_Effect>(this);
}
std::shared_ptr<scr::Sampler> PC_RenderPlatform::InstantiateSampler() const
{
	return std::make_shared<PC_Sampler>(this);
}
std::shared_ptr<scr::Shader> PC_RenderPlatform::InstantiateShader() const
{
	return std::make_shared<PC_Shader>(this);
}
std::shared_ptr<scr::ShaderStorageBuffer> PC_RenderPlatform::InstantiateShaderStorageBuffer() const
{
	return std::shared_ptr<PC_ShaderStorageBuffer>();
}
std::shared_ptr<scr::Texture> PC_RenderPlatform::InstantiateTexture() const
{
	return std::make_shared<PC_Texture>(this);
}
std::shared_ptr<scr::UniformBuffer> PC_RenderPlatform::InstantiateUniformBuffer() const
{
	return std::make_shared<PC_UniformBuffer>(this);
}
std::shared_ptr<scr::VertexBuffer> PC_RenderPlatform::InstantiateVertexBuffer() const
{
	return std::make_shared<PC_VertexBuffer>(this);
}