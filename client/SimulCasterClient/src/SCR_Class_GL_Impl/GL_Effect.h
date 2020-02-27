// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>

#include <api/Effect.h>
#include <GlProgram.h>

namespace scc
{
	class GL_Effect final : public scr::Effect
	{
	public:
		GL_Effect(const scr::RenderPlatform*const r)
			:scr::Effect(r) {}

        virtual ~GL_Effect();

		void Create(EffectCreateInfo* pEffectCreateInfo) override;
		void CreatePass(EffectPassCreateInfo* pEffectCreateInfo) override;

		void Bind(const char* effectPassName) const override;
		void Unbind(const char* effectPassName) const override;

		void LinkShaders(const char* effectPassName, const std::vector<scr::ShaderResource>& shaderResources) override;

		inline OVR::GlProgram& GetGlPlatform(const char* effectPassName) {return m_EffectPrograms[effectPassName];}

		static GLenum ToGLTopology(TopologyType topology);
        static GLenum ToGLCullMode(CullMode cullMode);
		static GLenum ToGLPolygonMode(PolygonMode polygonMode);
        static GLenum ToGLCompareOp(CompareOp op);
        static GLenum ToGLStencilCompareOp(StencilCompareOp op);
        static GLenum ToGLBlendFactor(BlendFactor factor);
        static GLenum ToGLBlendOp(BlendOp op);
		static OVR::ovrProgramParmType ToOVRProgramParmType(scr::ShaderResourceLayout::ShaderResourceType type);

	private:
		std::map<std::string, OVR::GlProgram> m_EffectPrograms;

		void BuildGraphicsPipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources);
		void BuildComputePipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources);
	};
}