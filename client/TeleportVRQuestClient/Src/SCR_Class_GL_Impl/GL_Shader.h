// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/Shader.h>

namespace scc
{
	//Interface for Shader
	class GL_Shader final : public clientrender::Shader
	{
		GLuint glShader;
		std::string source;
	public:
		GL_Shader(const clientrender::RenderPlatform*const r)
			:clientrender::Shader(r) {}

		void Create(const ShaderCreateInfo* pShaderCreateInfo) override;
		void Compile() override;
	};
}