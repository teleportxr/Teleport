// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Sampler.h>
#include <OVR_GlUtils.h>

namespace scr
{
	//Interface for Sampler
	class GL_Sampler final : public Sampler
	{
	public:
		void Create(Filter filterMinMag[2], Wrap wrapUVW[3], float minLod, float maxLod, bool anisotropyEnable, float maxAnisotropy) override;
		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;

		GLenum ToGLFilterType(Filter filter) const;
		GLenum ToGLWrapType(Wrap wrap) const;
	};
}