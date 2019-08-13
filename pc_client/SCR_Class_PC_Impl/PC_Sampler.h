// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Sampler.h>

namespace pc_client
{
	//Interface for Sampler
	class PC_Sampler final : public scr::Sampler
	{
	public:
		PC_Sampler() {}

		void Create(scr::Sampler::Filter filterMinMag[2], scr::Sampler::Wrap wrapUVW[3], float minLod, float maxLod, bool anisotropyEnable, float maxAnisotropy) override;
		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;
	};
}