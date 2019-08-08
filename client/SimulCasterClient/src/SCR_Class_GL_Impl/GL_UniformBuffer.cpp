// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_UniformBuffer.h"

using namespace scr;
using namespace OVR;

GL_UniformBuffer::GL_UniformBuffer(OVR::GlBuffer &ubo)
    :m_UBO(ubo)
{
}

void GL_UniformBuffer::Create(size_t size, const void* data, uint32_t bindingLocation)
{
    m_BindingLocation = bindingLocation;
    m_Size = size;
    m_Data = data;

    m_UBO.Create(GLBUFFER_TYPE_UNIFORM, size, data);

    Bind();
    glBindBufferRange(GL_UNIFORM_BUFFER, bindingLocation, m_UBO.GetBuffer(), 0, size);
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
    glBufferSubData(GL_UNIFORM_BUFFER, 0, m_Size, m_Data);
    Unbind();
}
