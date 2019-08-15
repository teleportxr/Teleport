// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Effect.h>
#include <GlProgram.h>

namespace scc
{
class GL_Effect final : public scr::Effect
	{
	private:
		OVR::GlProgram m_Program;

	public:
		GL_Effect(scr::RenderPlatform* r)
			:scr::Effect(r) {}

		void Create(EffectCreateInfo* pEffectCreateInfo) override;

		void Bind() const override;
		void Unbind() const override;

		void LinkShaders() override;

	private:
		GLenum ToGLTopology(TopologyType topology) const;
        GLenum ToGLCullMode(CullMode cullMode) const;
        GLenum ToGLCompareOp(CompareOp op) const;
        GLenum ToGLStencilCompareOp(StencilCompareOp op) const;
        GLenum ToGLBlendFactor(BlendFactor factor) const;
        GLenum ToGLBlendOp(BlendOp op) const;
	};
}