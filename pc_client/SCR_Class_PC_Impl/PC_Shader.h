// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Shader.h>

namespace pc_client
{
	//Interface for Shader
	class PC_Shader final : public scr::Shader
	{
	public:
		PC_Shader(const scr::RenderPlatform*const r):scr::Shader(r) {}

		void Compile() override;

		// Inherited via Shader
		void Create(const scr::Shader::ShaderCreateInfo* pShaderCreateInfo) override;
	};
}