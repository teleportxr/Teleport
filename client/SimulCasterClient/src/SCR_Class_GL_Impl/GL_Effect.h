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
		void CreatePass(EffectPassCreateInfo* pEffectCreateInfo) override;

		void Bind(const char* effectPassName) const override;
		void Unbind(const char* effectPassName) const override;

		void LinkShaders(const char* effectPassName, const std::vector<scr::DescriptorSet>& descriptorSets) override;

		inline OVR::GlProgram& GetGlPlatform() {return m_Program;}

		static GLenum ToGLTopology(TopologyType topology);
        static GLenum ToGLCullMode(CullMode cullMode);
		static GLenum ToGLPolygonMode(PolygonMode polygonMode);
        static GLenum ToGLCompareOp(CompareOp op);
        static GLenum ToGLStencilCompareOp(StencilCompareOp op);
        static GLenum ToGLBlendFactor(BlendFactor factor);
        static GLenum ToGLBlendOp(BlendOp op);
		static OVR::ovrProgramParmType ToOVRProgramParmType(scr::DescriptorSetLayout::DescriptorType type);
	};
}