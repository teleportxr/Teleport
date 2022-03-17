// (C) Copyright 2018-2022 Simul Software Ltd
#include "PC_Shader.h"

using namespace pc_client;
using namespace clientrender;

void PC_Shader::Compile()
{
    //NULL;
}

void PC_Shader::Create(const clientrender::Shader::ShaderCreateInfo * pShaderCreateInfo)
{
	m_CI = *pShaderCreateInfo;
}
