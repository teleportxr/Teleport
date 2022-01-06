// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

namespace scr
{
	//Interface for Shader
	class Shader : public APIObject
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
			std::string sourceCode;
			std::string filepath;
			std::string entryPoint;
			Stage stage = Stage::SHADER_STAGE_UNKNOWN;
		};
	
	protected:
		ShaderCreateInfo m_CI;

	public:
		Shader(const scr::RenderPlatform* const r)
			:APIObject(r), m_CI()
		{}

		virtual ~Shader()
		{
			m_CI.sourceCode.clear();
			m_CI.filepath.clear();
			m_CI.entryPoint.clear();
			m_CI.stage = Stage::SHADER_STAGE_UNKNOWN;
		}

		virtual void Create(const ShaderCreateInfo* pShaderCreateInfo) = 0;
		virtual void Compile() = 0;

		inline const ShaderCreateInfo& GetShaderCreateInfo() const {return m_CI;}
	};
}