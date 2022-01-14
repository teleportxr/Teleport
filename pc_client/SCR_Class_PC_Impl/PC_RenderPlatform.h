// (C) Copyright 2018-2022 Simul Software Ltd
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
    class PC_RenderPlatform final : public clientrender::RenderPlatform
    {
		simul::crossplatform::RenderPlatform* renderPlatform = nullptr;
    public:
        PC_RenderPlatform():clientrender::RenderPlatform() {}
        ~PC_RenderPlatform() {}

		std::shared_ptr<clientrender::FrameBuffer>			InstantiateFrameBuffer() const override;
		std::shared_ptr<clientrender::IndexBuffer>			InstantiateIndexBuffer() const override;
		std::shared_ptr<clientrender::Effect>				InstantiateEffect() const override;
		std::shared_ptr<clientrender::Sampler>				InstantiateSampler() const override;
		std::shared_ptr<clientrender::Shader>				InstantiateShader() const override;
		std::shared_ptr<clientrender::ShaderStorageBuffer>	InstantiateShaderStorageBuffer() const override;
		std::shared_ptr<clientrender::Skin>					InstantiateSkin(const std::string& name) const override;
		std::shared_ptr<clientrender::Skin>					InstantiateSkin(const std::string& name, const std::vector<clientrender::mat4>& inverseBindMatrices, size_t boneAmount, const clientrender::Transform& skinTransform) const override;
		std::shared_ptr<clientrender::Texture>				InstantiateTexture() const override;
		std::shared_ptr<clientrender::UniformBuffer>			InstantiateUniformBuffer() const override;
		std::shared_ptr<clientrender::VertexBuffer>			InstantiateVertexBuffer() const override;

		// Inherited via RenderPlatform
		virtual clientrender::API::APIType GetAPI() const override
		{
			return clientrender::API::APIType::UNKNOWN;
		}

		void SetSimulRenderPlatform(simul::crossplatform::RenderPlatform *r);
		simul::crossplatform::RenderPlatform *GetSimulRenderPlatform() const
		{
			return renderPlatform;
		}
	};
}
