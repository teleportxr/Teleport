// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include "api/Shader.h"

namespace scr
{
	class ShaderSystem
	{
	public:
		//Pipelines

		enum class PipelineType : uint32_t
		{
			PIPELINE_TYPE_UNKNOWN,
			PIPELINE_TYPE_GRAPHICS,
			PIPELINE_TYPE_COMPUTE,
		};
		struct Pipeline
		{
			PipelineType m_Type = PipelineType::PIPELINE_TYPE_UNKNOWN;
			Shader* m_Shaders;
			size_t m_ShaderCount;

			virtual ~Pipeline() = default;
		};
		struct GraphicsPipeline : public Pipeline
		{
			GraphicsPipeline(Shader* shaders, size_t shaderCount)
			{
				m_Type = PipelineType::PIPELINE_TYPE_GRAPHICS;
				m_Shaders = shaders;
				m_ShaderCount = shaderCount;
			}

		};
		struct ComputePipeline : public Pipeline
		{
			ComputePipeline(Shader* computeShader)
			{
				m_Type = PipelineType::PIPELINE_TYPE_COMPUTE;
				m_Shaders = computeShader;
				m_ShaderCount = 1;
			}
		};

		//Pass Variables for 
		enum class PassVariablesStructureType : uint32_t
		{
			PASS_VARIABLES_STRUCTURE_TYPE_BASE,
		};
		struct PassVariables
		{
			PassVariablesStructureType type = PassVariablesStructureType::PASS_VARIABLES_STRUCTURE_TYPE_BASE;
			bool reverseDepth;
			bool msaa;
			bool mask;

			virtual ~PassVariables() = default;
		};
	};
}
