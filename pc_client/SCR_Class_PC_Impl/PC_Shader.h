// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/Shader.h>

namespace pc_client
{
	//Interface for Shader
	class PC_Shader final : public clientrender::Shader
	{
	public:
		PC_Shader(const clientrender::RenderPlatform*const r):clientrender::Shader(r) {}

		void Compile() override;

		// Inherited via Shader
		void Create(const clientrender::Shader::ShaderCreateInfo* pShaderCreateInfo) override;
	};
}