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
		/// An array of these is used to create the pipeline.
		struct PipelineCreateInfo
		{
			PipelineType m_PipelineType;
			size_t m_Count;
			Shader::ShaderCreateInfo m_ShaderCreateInfo[8];
		};
		struct Pipeline
		{
			PipelineType m_Type = PipelineType::PIPELINE_TYPE_UNKNOWN;
			std::shared_ptr<Shader> m_Shaders[8];
			size_t m_ShaderCount;

			Pipeline()=default;
			Pipeline(RenderPlatform *rp, const PipelineCreateInfo *pc);
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
