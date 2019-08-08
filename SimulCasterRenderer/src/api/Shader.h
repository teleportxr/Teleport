// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"

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
	protected:
		const char* m_SourceCode;
		const char* m_EntryPoint; 
		Stage m_Stage;

	public:
		virtual ~Shader()
		{
			m_SourceCode = nullptr;
			m_EntryPoint = nullptr;
			m_Stage = Stage::SHADER_STAGE_UNKNOWN;
		}

		virtual void Create(const char* sourceCode, const char* entryPoint, Stage stage) = 0;
		virtual void Compile() = 0;

		inline const char* GetSourceCode() const {return m_SourceCode;}
		inline const char* GetEntryPoint() const {return m_EntryPoint;}
		inline const Stage& GetStage() const {return m_Stage;}
	};
}