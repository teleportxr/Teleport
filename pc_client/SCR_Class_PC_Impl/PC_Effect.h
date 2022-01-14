// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <ClientRender/Effect.h>

namespace pc_client
{
	//Implementation of FrameBuffer wrapping over ovrEyeBuffers
	class PC_Effect final : public clientrender::Effect
	{
	private:

	public:
		PC_Effect(const clientrender::RenderPlatform*const r) :clientrender::Effect(r) {}
		// Inherited via Effect
		virtual void Create(EffectCreateInfo * pEffectCreateInfo) override;
		virtual void CreatePass(EffectPassCreateInfo* pEffectPassCreateInfo) override;
		virtual void LinkShaders(const char* effectPassName, const std::vector<clientrender::ShaderResource>& shaderResources) override;
		virtual void Bind(const char* effectPassName) const override;
		virtual void Unbind(const char* effectPassName) const override;
	};
}