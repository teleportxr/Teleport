// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_UniformBuffer.h"

using namespace scc;
using namespace scr;
using namespace OVR;

void GL_UniformBuffer::Create(UniformBufferCreateInfo* pUniformBufferCreateInfo)
{
	m_CI = *pUniformBufferCreateInfo;

    m_UBO = GlBuffer();
    m_UBO.Create(GLBUFFER_TYPE_UNIFORM, m_CI.size, m_CI.data);

    Bind();
    glBindBufferRange(GL_UNIFORM_BUFFER, m_CI.bindingLocation, m_UBO.GetBuffer(), 0, m_CI.size);
    Unbind();
}
void GL_UniformBuffer::Destroy()
{
    m_UBO.Destroy();
}

void GL_UniformBuffer::Bind() const
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_UBO.GetBuffer());
}
void GL_UniformBuffer::Unbind() const
{
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GL_UniformBuffer::Submit() const
{
    Bind();
    glBufferSubData(GL_UNIFORM_BUFFER, 0, m_CI.size, m_CI.data);
    Unbind();
}
