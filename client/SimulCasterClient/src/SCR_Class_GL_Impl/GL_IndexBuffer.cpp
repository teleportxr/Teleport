// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_IndexBuffer.h"

using namespace scc;
using namespace scr;
using namespace OVR;

void GL_IndexBuffer::Create(IndexBufferCreateInfo* pIndexBufferCreateInfo)
{
    m_CI = *pIndexBufferCreateInfo;

    size_t size = m_CI.indexCount * m_CI.stride;
    assert(size % 4 == 0);

    //TODO: Deal with GlGeometry
    glGenBuffers(1, &m_Geometry.indexBuffer);
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, (const void*)m_CI.data, GL_STATIC_DRAW);

    m_Geometry.indexCount = (int)m_CI.indexCount;
}
void GL_IndexBuffer::Destroy()
{
    glDeleteBuffers(1, &m_Geometry.indexBuffer);
}

void GL_IndexBuffer::Bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Geometry.indexBuffer);
}
void GL_IndexBuffer::Unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
