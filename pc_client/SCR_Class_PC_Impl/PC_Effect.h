// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Effect.h>

namespace pc_client
{
	//Implementation of FrameBuffer wrapping over ovrEyeBuffers
	class PC_Effect final : public scr::Effect
	{
	private:

	public:
		PC_Effect(scr::RenderPlatform *r) :scr::Effect(r) {}
		// Inherited via Effect
		virtual void Create(EffectCreateInfo * pEffectCreateInfo) override;
		virtual void LinkShaders() override;
		virtual void Bind() const override;
		virtual void Unbind() const override;
	};
}