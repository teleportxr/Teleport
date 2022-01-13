// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/Sampler.h>

namespace pc_client
{
	//Interface for Sampler
	class PC_Sampler final : public scr::Sampler
	{
	public:
		PC_Sampler(const scr::RenderPlatform*const r):scr::Sampler(r) {}

		void Destroy() override;
		
		void Bind() const override;
		void Unbind() const override;

		// Inherited via Sampler
		void Create(SamplerCreateInfo * pSamplerCreateInfo) override;
	};
}