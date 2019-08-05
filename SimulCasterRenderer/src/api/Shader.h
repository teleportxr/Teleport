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
			SHADER_STAGE_TESSELLATION_EVALUTION,
			SHADER_STAGE_GEOMETRY,
			SHADER_STAGE_FRAGMENT,
			SHADER_STAGE_COMPUTE,

			SHADER_STAGE_HULL = SHADER_STAGE_TESSELLATION_CONTROL,
			SHADER_STAGE_DOMAIN = SHADER_STAGE_TESSELLATION_EVALUTION,
			SHADER_STAGE_PIXEL = SHADER_STAGE_FRAGMENT,
		};
	protected:
		const char* m_Filepath;
		const char* m_EntryPoint; 
		Stage m_Stage;

	public:
		virtual ~Shader()
		{
			m_Filepath = nullptr;
			m_EntryPoint = nullptr;
			Stage m_Stage = Stage::SHADER_STAGE_UNKNOWN;
		}

		virtual void Create(const char* filepath, const char* entryPoint, Stage stage) = 0;
		virtual void Compile() = 0;
	};
}