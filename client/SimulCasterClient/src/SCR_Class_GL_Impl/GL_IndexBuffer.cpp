// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_IndexBuffer.h"

using namespace scr;
using namespace OVR;

GL_IndexBuffer::GL_IndexBuffer(OVR::GlGeometry& geometry)
    :m_Geometry(geometry)
{
}

void GL_IndexBuffer::Create(size_t size, const uint32_t* data)
{
    m_Size = size;
    m_Data = data;
    
    assert(size % 4 == 0);
    m_Count = size / sizeof(uint32_t);

    glGenBuffers(1, &m_Geometry.indexBuffer);
    Bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, (const void*)data, GL_STATIC_DRAW);

    m_Geometry.indexCount = (int)m_Count;
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
