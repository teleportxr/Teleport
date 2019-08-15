// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Shader.h>

namespace scc
{
	//Interface for Shader
class GL_Shader final : public scr::Shader
	{
	public:
		GL_Shader(scr::RenderPlatform *r)
			:scr::Shader(r) {}

		void Create(ShaderCreateInfo* pShaderCreateInfo) override;
		void Compile() override;
	};
}