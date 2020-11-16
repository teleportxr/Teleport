// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

namespace scc
{
class GL_RenderPlatform final : public scr::RenderPlatform
    {
    public:
        GL_RenderPlatform()
        	:scr::RenderPlatform() {}
        ~GL_RenderPlatform() {}

		std::shared_ptr<scr::FrameBuffer>			InstantiateFrameBuffer() const override;
		std::shared_ptr<scr::IndexBuffer>			InstantiateIndexBuffer() const override;
		std::shared_ptr<scr::Effect>				InstantiateEffect() const override;
		std::shared_ptr<scr::Sampler>				InstantiateSampler() const override;
		std::shared_ptr<scr::Shader>				InstantiateShader() const override;
		std::shared_ptr<scr::ShaderStorageBuffer> 	InstantiateShaderStorageBuffer() const override;
		std::shared_ptr<scr::Skin>					InstantiateSkin() const override;
		std::shared_ptr<scr::Texture>				InstantiateTexture() const override;
		std::shared_ptr<scr::UniformBuffer>			InstantiateUniformBuffer() const override;
		std::shared_ptr<scr::VertexBuffer>			InstantiateVertexBuffer() const override;

		scr::API::APIType GetAPI() const override
		{
			return scr::API::APIType::OPENGLES;
		}
    };
}
