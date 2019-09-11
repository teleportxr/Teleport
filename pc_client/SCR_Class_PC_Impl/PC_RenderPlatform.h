// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

#include "PC_FrameBuffer.h"
#include "PC_IndexBuffer.h"
#include "PC_Effect.h"
#include "PC_Sampler.h"
#include "PC_Shader.h"
#include "PC_ShaderStorageBuffer.h"
#include "PC_Texture.h"
#include "PC_UniformBuffer.h"
#include "PC_VertexBuffer.h"

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
		simul::crossplatform::RenderPlatform *renderPlatform;
    public:
        PC_RenderPlatform():scr::RenderPlatform() {}
        ~PC_RenderPlatform() {}

		std::shared_ptr<scr::FrameBuffer>			InstantiateFrameBuffer()		;
		std::shared_ptr<scr::IndexBuffer>			InstantiateIndexBuffer()		;
		std::shared_ptr<scr::Effect>				InstantiateEffect()				;
		std::shared_ptr<scr::Sampler>				InstantiateSampler()			;
		std::shared_ptr<scr::Shader>				InstantiateShader()				;
		std::shared_ptr<scr::ShaderStorageBuffer>	InstantiateShaderStorageBuffer();
		std::shared_ptr<scr::Texture>				InstantiateTexture()			;
		std::shared_ptr<scr::UniformBuffer>			InstantiateUniformBuffer()		;
		std::shared_ptr<scr::VertexBuffer>			InstantiateVertexBuffer()		;

		// Inherited via RenderPlatform
		virtual scr::API::APIType GetAPI() const override
		{
			return scr::API::APIType::UNKNOWN;
		}

		void SetSimulRenderPlatform(simul::crossplatform::RenderPlatform *r);
		simul::crossplatform::RenderPlatform *GetSimulRenderPlatform()
		{
			return renderPlatform;
		}
	};
}
