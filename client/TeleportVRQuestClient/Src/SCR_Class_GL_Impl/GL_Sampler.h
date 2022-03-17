// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/Sampler.h>
#include <GLES3/gl3.h>


namespace scc
{
	//Interface for Sampler
	class GL_Sampler final : public clientrender::Sampler
	{
	public:
		GL_Sampler(const clientrender::RenderPlatform*const r)
			:clientrender::Sampler(r) {}

		void Create(SamplerCreateInfo* pSamplerCreateInfo) override;
		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;

		GLenum ToGLFilterType(Filter filter) const;
		GLenum ToGLWrapType(Wrap wrap) const;
	};
}