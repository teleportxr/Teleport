// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Sampler.h>

namespace scr
{
	//Interface for Sampler
	class PC_Sampler final : public Sampler
	{
	public:
		PC_Sampler() {}

		void Create(Filter filterMinMag[2], Wrap wrapUVW[3], float minLod, float maxLod, bool anisotropyEnable, float maxAnisotropy) override;
		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;
	};
}