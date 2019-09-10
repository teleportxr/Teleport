// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

#include "GL_DeviceContext.h"
#include "GL_Effect.h"
#include "GL_FrameBuffer.h"
#include "GL_IndexBuffer.h"
#include "GL_Sampler.h"
#include "GL_Shader.h"
#include "GL_ShaderStorageBuffer.h"
#include "GL_Texture.h"
#include "GL_UniformBuffer.h"
#include "GL_VertexBuffer.h"


namespace scc
{
class GL_RenderPlatform final : public scr::RenderPlatform
    {
    public:
        GL_RenderPlatform()
        	:scr::RenderPlatform() {}
        ~GL_RenderPlatform() {}

		std::shared_ptr<scr::FrameBuffer>			InstantiateFrameBuffer() override;
		std::shared_ptr<scr::IndexBuffer>			InstantiateIndexBuffer() override;
		std::shared_ptr<scr::Effect>				InstantiateEffect() override;
		std::shared_ptr<scr::Sampler>				InstantiateSampler() override;
		std::shared_ptr<scr::Shader>				InstantiateShader() override;
		std::shared_ptr<scr::ShaderStorageBuffer> 	InstantiateShaderStorageBuffer() override;
		std::shared_ptr<scr::Texture>				InstantiateTexture() override;
		std::shared_ptr<scr::UniformBuffer>			InstantiateUniformBuffer() override;
		std::shared_ptr<scr::VertexBuffer>			InstantiateVertexBuffer() override;

		scr::API::APIType GetAPI() const override
		{
			return scr::API::APIType::OPENGLES;
		}
    };
}
