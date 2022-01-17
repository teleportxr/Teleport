// (C) Copyright 2018-2019 Simul Software Ltd
#include <GLES3/gl3.h>
#include "GL_Shader.h"

using namespace scc;
using namespace clientrender;

void GL_Shader::Create(const ShaderCreateInfo* pShaderCreateInfo)
{
    m_CI = *pShaderCreateInfo;
}

void GL_Shader::Compile()
{
	if(glShader)
		glDeleteShader(glShader);
	glShader = glCreateShader((GLenum)m_CI.stage);
	glShaderSource(glShader, 1,(const GLchar * const*)&m_CI.sourceCode, nullptr);
	glCompileShader(glShader);
}