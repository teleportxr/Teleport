// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <ClientRender/Shader.h>

namespace scc
{
	//Interface for Shader
	class GL_Shader final : public scr::Shader
	{
		GLuint glShader;
		std::string source;
	public:
		GL_Shader(const scr::RenderPlatform*const r)
			:scr::Shader(r) {}

		void Create(const ShaderCreateInfo* pShaderCreateInfo) override;
		void Compile() override;
	};
}