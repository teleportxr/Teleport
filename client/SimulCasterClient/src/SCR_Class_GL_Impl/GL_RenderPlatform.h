// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

#include "GL_FrameBuffer.h"
#include "GL_IndexBuffer.h"
#include "GL_Pipeline.h"
#include "GL_Sampler.h"
#include "GL_Shader.h"
#include "GL_Texture.h"
#include "GL_UniformBuffer.h"
#include "GL_VertexBuffer.h"


namespace scr
{
    class GL_RenderPlatform final : public RenderPlatform
    {
    public:
        GL_RenderPlatform() {}
        ~GL_RenderPlatform() {}

		std::shared_ptr<FrameBuffer>	InstantiateFrameBuffer()	{ return std::make_shared<GL_FrameBuffer>(); }
		std::shared_ptr<IndexBuffer>	InstantiateIndexBuffer()	{ return std::make_shared<GL_IndexBuffer>(); }
		std::shared_ptr<Pipeline>		InstantiatePipeline()		{ return std::make_shared<GL_Pipeline>(); }
		std::shared_ptr<Sampler>		InstantiateSampler()		{ return std::make_shared<GL_Sampler>(); }
		std::shared_ptr<Shader>			InstantiateShader()			{ return std::make_shared<GL_Shader>(); }
		std::shared_ptr<Texture>		InstantiateTexture()		{ return std::make_shared<GL_Texture>(); }
		std::shared_ptr<UniformBuffer>	InstantiateUniformBuffer()	{ return std::make_shared<GL_UniformBuffer>(); }
		std::shared_ptr<VertexBuffer>	InstantiateVertexBuffer()	{ return std::make_shared<GL_VertexBuffer>(); }
    };
}
