// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{
	//Interface for Shader
	class Shader
	{
	public:
		enum class Stage : uint32_t
		{
			SHADER_STAGE_UNKNOWN,
			SHADER_STAGE_VERTEX,
			SHADER_STAGE_TESSELLATION_CONTROL,
			SHADER_STAGE_TESSELLATION_EVALUATION,
			SHADER_STAGE_GEOMETRY,
			SHADER_STAGE_FRAGMENT,
			SHADER_STAGE_COMPUTE,

			SHADER_STAGE_HULL = SHADER_STAGE_TESSELLATION_CONTROL,
			SHADER_STAGE_DOMAIN = SHADER_STAGE_TESSELLATION_EVALUATION,
			SHADER_STAGE_PIXEL = SHADER_STAGE_FRAGMENT,
		};
		struct ShaderCreateInfo
		{
			const char* sourceCode;
			const char* filepath;
			const char* entryPoint;
			Shader::Stage stage;
		};
	
	protected:
		ShaderCreateInfo m_CI;

	public:
		virtual ~Shader()
		{
			m_CI.sourceCode = nullptr;
			m_CI.filepath = nullptr;
			m_CI.entryPoint = nullptr;
			m_CI.stage = Stage::SHADER_STAGE_UNKNOWN;
		}

		virtual void Create(ShaderCreateInfo* pShaderCreateInfo) = 0;
		virtual void Compile() = 0;

		inline const ShaderCreateInfo& GetShaderCreateInfo() const {return m_CI;}
	};
}