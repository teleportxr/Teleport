// (C) Copyright 2018-2019 Simul Software Ltd
#include "PC_Shader.h"

using namespace scr;

void PC_Shader::Create(const char* sourceCode, const char* entryPoint, Stage stage)
{
    m_SourceCode = sourceCode;
    m_EntryPoint = entryPoint;
    m_Stage = stage;
}
void PC_Shader::Compile()
{
    //NULL;
}