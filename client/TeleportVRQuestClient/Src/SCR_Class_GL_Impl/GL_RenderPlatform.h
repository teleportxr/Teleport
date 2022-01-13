// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../ClientRender/RenderPlatform.h"

namespace scc
{
class GL_RenderPlatform final : public clientrender::RenderPlatform
    {
    public:
        GL_RenderPlatform()
        	:clientrender::RenderPlatform() {}
        ~GL_RenderPlatform() {}

		std::shared_ptr<clientrender::FrameBuffer>			InstantiateFrameBuffer() const override;
		std::shared_ptr<clientrender::IndexBuffer>			InstantiateIndexBuffer() const override;
		std::shared_ptr<clientrender::Effect>				InstantiateEffect() const override;
		std::shared_ptr<clientrender::Sampler>				InstantiateSampler() const override;
		std::shared_ptr<clientrender::Shader>				InstantiateShader() const override;
		std::shared_ptr<clientrender::ShaderStorageBuffer> 	InstantiateShaderStorageBuffer() const override;
		std::shared_ptr<clientrender::Skin>					InstantiateSkin(const std::string& name) const override;
		std::shared_ptr<clientrender::Skin>					InstantiateSkin(const std::string& name, const std::vector<clientrender::mat4>& inverseBindMatrices, size_t boneAmount, const clientrender::Transform& skinTransform) const override;
		std::shared_ptr<clientrender::Texture>				InstantiateTexture() const override;
		std::shared_ptr<clientrender::UniformBuffer>			InstantiateUniformBuffer() const override;
		std::shared_ptr<clientrender::VertexBuffer>			InstantiateVertexBuffer() const override;

		clientrender::API::APIType GetAPI() const override
		{
			return clientrender::API::APIType::OPENGLES;
		}
    };
}
