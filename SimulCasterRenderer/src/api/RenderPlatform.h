// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

#include "FrameBuffer.h"
#include "IndexBuffer.h"
#include "Effect.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "VertexBuffer.h"
#include "crossplatform/API.h"

namespace scr
{
	class RenderPlatform
	{
	public:
		virtual ~RenderPlatform() {}

		virtual API::APIType GetAPI() const = 0;

		virtual std::shared_ptr<FrameBuffer>	InstantiateFrameBuffer() = 0;
		virtual std::shared_ptr<IndexBuffer>	InstantiateIndexBuffer() = 0;
		virtual std::shared_ptr<Effect>			InstantiateEffect() = 0;
		virtual std::shared_ptr<Sampler>		InstantiateSampler() = 0;
		virtual std::shared_ptr<Shader>			InstantiateShader() = 0;
		virtual std::shared_ptr<Texture>		InstantiateTexture() = 0;
		virtual std::shared_ptr<UniformBuffer>	InstantiateUniformBuffer() = 0;
		virtual std::shared_ptr<VertexBuffer>	InstantiateVertexBuffer() = 0;
	};
}