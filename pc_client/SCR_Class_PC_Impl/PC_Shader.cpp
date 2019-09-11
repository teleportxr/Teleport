// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Shader.h"

using namespace pc_client;
using namespace scr;

void PC_Shader::Compile()
{
    //NULL;
}

void pc_client::PC_Shader::Create(const ShaderCreateInfo* pShaderCreateInfo)
{
	m_CI = *pShaderCreateInfo;
}
