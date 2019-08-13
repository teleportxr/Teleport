// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

#include "PC_FrameBuffer.h"
#include "PC_IndexBuffer.h"
#include "PC_Pipeline.h"
#include "PC_Sampler.h"
#include "PC_Shader.h"
#include "PC_Texture.h"
#include "PC_UniformBuffer.h"
#include "PC_VertexBuffer.h"


namespace scr
{
    class PC_RenderPlatform final : public RenderPlatform
    {
    public:
        PC_RenderPlatform() {}
        ~PC_RenderPlatform() {}

		std::shared_ptr<FrameBuffer>	InstantiateFrameBuffer()	{ return std::make_shared<PC_FrameBuffer>(); }
		std::shared_ptr<IndexBuffer>	InstantiateIndexBuffer()	{ return std::make_shared<PC_IndexBuffer>(); }
		std::shared_ptr<Pipeline>		InstantiatePipeline()		{ return std::make_shared<PC_Pipeline>(); }
		std::shared_ptr<Sampler>		InstantiateSampler()		{ return std::make_shared<PC_Sampler>(); }
		std::shared_ptr<Shader>			InstantiateShader()			{ return std::make_shared<PC_Shader>(); }
		std::shared_ptr<Texture>		InstantiateTexture()		{ return std::make_shared<PC_Texture>(); }
		std::shared_ptr<UniformBuffer>	InstantiateUniformBuffer()	{ return std::make_shared<PC_UniformBuffer>(); }
		std::shared_ptr<VertexBuffer>	InstantiateVertexBuffer()	{ return std::make_shared<PC_VertexBuffer>(); }
    };
}
