// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "ClientRender/RenderPlatform.h"

#include "PC_FrameBuffer.h"
#include "PC_IndexBuffer.h"
#include "PC_Effect.h"
#include "PC_Sampler.h"
#include "PC_Shader.h"
#include "PC_ShaderStorageBuffer.h"
#include "PC_Texture.h"
#include "PC_UniformBuffer.h"
#include "PC_VertexBuffer.h"
#include "basic_linear_algebra.h"

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
	}
}

namespace pc_client
{
    class PC_RenderPlatform final : public scr::RenderPlatform
    {
		simul::crossplatform::RenderPlatform* renderPlatform = nullptr;
    public:
        PC_RenderPlatform():scr::RenderPlatform() {}
        ~PC_RenderPlatform() {}

		std::shared_ptr<scr::FrameBuffer>			InstantiateFrameBuffer() const override;
		std::shared_ptr<scr::IndexBuffer>			InstantiateIndexBuffer() const override;
		std::shared_ptr<scr::Effect>				InstantiateEffect() const override;
		std::shared_ptr<scr::Sampler>				InstantiateSampler() const override;
		std::shared_ptr<scr::Shader>				InstantiateShader() const override;
		std::shared_ptr<scr::ShaderStorageBuffer>	InstantiateShaderStorageBuffer() const override;
		std::shared_ptr<scr::Skin>					InstantiateSkin(const std::string& name) const override;
		std::shared_ptr<scr::Skin>					InstantiateSkin(const std::string& name, const std::vector<scr::mat4>& inverseBindMatrices, size_t boneAmount, const scr::Transform& skinTransform) const override;
		std::shared_ptr<scr::Texture>				InstantiateTexture() const override;
		std::shared_ptr<scr::UniformBuffer>			InstantiateUniformBuffer() const override;
		std::shared_ptr<scr::VertexBuffer>			InstantiateVertexBuffer() const override;

		// Inherited via RenderPlatform
		virtual scr::API::APIType GetAPI() const override
		{
			return scr::API::APIType::UNKNOWN;
		}

		void SetSimulRenderPlatform(simul::crossplatform::RenderPlatform *r);
		simul::crossplatform::RenderPlatform *GetSimulRenderPlatform() const
		{
			return renderPlatform;
		}
	};
}
