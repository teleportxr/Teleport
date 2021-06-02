// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>

#include <api/Effect.h>
#include <Render/GlProgram.h>

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

		inline OVRFW::GlProgram* GetGlProgram(const char* effectPassName)
		{
			for(auto& programPair : m_EffectPrograms)
			{
				if(strcmp(programPair.first.c_str(), effectPassName) == 0)
				{
					return &programPair.second;
				}
			}

			return nullptr;
		}

		inline const OVRFW::GlProgram* GetGlProgram(const char* effectPassName) const
		{
			for(const auto& programPair : m_EffectPrograms)
			{
				if(strcmp(programPair.first.c_str(), effectPassName) == 0)
				{
					return &programPair.second;
				}
			}

			return nullptr;
		}

		static GLenum ToGLTopology(TopologyType topology);
        static GLenum ToGLCullMode(CullMode cullMode);
		static GLenum ToGLPolygonMode(PolygonMode polygonMode);
        static GLenum ToGLCompareOp(CompareOp op);
        static GLenum ToGLStencilCompareOp(StencilCompareOp op);
        static GLenum ToGLBlendFactor(BlendFactor factor);
        static GLenum ToGLBlendOp(BlendOp op);
		static OVRFW::ovrProgramParmType ToOVRProgramParmType(scr::ShaderResourceLayout::ShaderResourceType type);

	private:
		std::map<std::string, OVRFW::GlProgram> m_EffectPrograms;

		void BuildGraphicsPipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources);
		void BuildComputePipeline(const char* effectPassName, scr::ShaderSystem::Pipeline& pipeline, const std::vector<scr::ShaderResource>& shaderResources);
	};
}