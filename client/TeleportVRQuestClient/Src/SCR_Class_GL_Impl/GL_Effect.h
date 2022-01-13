// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>

#include <ClientRender/Effect.h>
#include <Render/GlProgram.h>

namespace scc
{
	class GL_Effect final : public clientrender::Effect
	{
	public:
		GL_Effect(const clientrender::RenderPlatform*const r)
			:clientrender::Effect(r) {}

        virtual ~GL_Effect();

		void Create(EffectCreateInfo* pEffectCreateInfo) override;
		void CreatePass(EffectPassCreateInfo* pEffectCreateInfo) override;

		void Bind(const char* effectPassName) const override;
		void Unbind(const char* effectPassName) const override;

		void LinkShaders(const char* effectPassName, const std::vector<clientrender::ShaderResource>& shaderResources) override;

		struct Pass
		{
			std::vector<OVRFW::ovrProgramParm> uniformParms;
			OVRFW::GlProgram ovrProgram;
			int GetParameterIndex(const char *param_name) const;
		};
		inline const Pass* GetPass(const char* effectPassName) const
		{
			for(const auto& programPair : m_passes)
			{
				if(strcmp(programPair.first.c_str(), effectPassName) == 0)
				{
					return &programPair.second;
				}
			}

			return nullptr;
		}

		inline OVRFW::GlProgram* GetGlProgram(const char* effectPassName)
		{
			for(auto& programPair : m_passes)
			{
				if(strcmp(programPair.first.c_str(), effectPassName) == 0)
				{
					return &programPair.second.ovrProgram;
				}
			}
			return nullptr;
		}
		inline const OVRFW::GlProgram* GetGlProgram(const char* effectPassName) const
		{
			for(const auto& programPair : m_passes)
			{
				if(strcmp(programPair.first.c_str(), effectPassName) == 0)
				{
					return &programPair.second.ovrProgram;
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
		static OVRFW::ovrProgramParmType ToOVRProgramParmType(clientrender::ShaderResourceLayout::ShaderResourceType type);

	private:
		std::map<std::string, Pass> m_passes;

		void BuildGraphicsPipeline(const char* effectPassName, clientrender::ShaderSystem::Pipeline& pipeline, const std::vector<clientrender::ShaderResource>& shaderResources);
		void BuildComputePipeline(const char* effectPassName, clientrender::ShaderSystem::Pipeline& pipeline, const std::vector<clientrender::ShaderResource>& shaderResources);
	};
}