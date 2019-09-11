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
		virtual void CreatePass(EffectPassCreateInfo* pEffectPassCreateInfo) override;
		virtual void LinkShaders(const char* effectPassName, const std::vector<scr::ShaderResource>& shaderResources) override;
		virtual void Bind(const char* effectPassName) const override;
		virtual void Unbind(const char* effectPassName) const override;
	};
}