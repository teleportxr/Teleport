// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <api/Shader.h>

namespace scr
{
	//Interface for Shader
	class PC_Shader final : public Shader
	{
	public:
		PC_Shader() {}

		void Create(const char* sourceCode, const char* entryPoint, Stage stage) override;
		void Compile() override;
	};
}