// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Sampler.h>
#include <OVR_GlUtils.h>

namespace scc
{
	//Interface for Sampler
class GL_Sampler final : public scr::Sampler
	{
	public:
		GL_Sampler(scr::RenderPlatform* r)
			:scr::Sampler(r) {}

		void Create(SamplerCreateInfo* pSamplerCreateInfo) override;
		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;

		GLenum ToGLFilterType(Filter filter) const;
		GLenum ToGLWrapType(Wrap wrap) const;
	};
}