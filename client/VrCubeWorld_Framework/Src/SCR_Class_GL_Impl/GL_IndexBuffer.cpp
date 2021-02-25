// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_IndexBuffer.h"

using namespace scc;
using namespace scr;
using namespace OVR;

void GL_IndexBuffer::Create(IndexBufferCreateInfo* pIndexBufferCreateInfo)
{
    m_CI = *pIndexBufferCreateInfo;

    size_t size = m_CI.indexCount * m_CI.stride;

    glGenBuffers(1, &m_IndexID);
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, (const void*)m_CI.data, GL_STATIC_DRAW);
    Unbind();
}
void GL_IndexBuffer::Destroy()
{
    glDeleteBuffers(1, &m_IndexID);
}

void GL_IndexBuffer::Bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexID);
}
void GL_IndexBuffer::Unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
