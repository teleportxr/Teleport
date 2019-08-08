// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_VertexBuffer.h"

using namespace scr;
using namespace OVR;

GL_VertexBuffer::GL_VertexBuffer(OVR::GlGeometry& geometry)
    :m_Geometry(geometry)
{
}

void GL_VertexBuffer::Create(size_t size, const void* data)
{
    m_Size = size;
    m_Data = data;

    glGenBuffers(1, &m_Geometry.vertexBuffer);
    Bind();
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);

    CalculateCount();
}
void GL_VertexBuffer::Destroy()
{
    glDeleteBuffers(1, &m_Geometry.vertexBuffer);
}

void GL_VertexBuffer::Bind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Geometry.vertexBuffer);
}
void GL_VertexBuffer::Unbind() const
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
void GL_VertexBuffer::CreateVAO(const VertexBufferLayout* layout)
{
    if(m_Layout == nullptr || layout == nullptr)
        SCR_COUT_BREAK("Can not create VAO. No VertexBufferLayout has been submitted");

    CalculateCount();

    glGenVertexArrays(1, &m_Geometry.vertexArrayObject);
    glBindVertexArray(m_Geometry.vertexArrayObject);
    Bind();

    size_t offset = 0;
    for(auto& attrib : m_Layout->m_Attributes)
    {
        GLenum type;
        switch (attrib.type)
        {
            case VertexBufferLayout::Type::FLOAT:   type = GL_FLOAT;        break;
            case VertexBufferLayout::Type::INT:     type = GL_INT;          break;
            case VertexBufferLayout::Type::UINT:    type = GL_UNSIGNED_INT; break;
        }
        glEnableVertexAttribArray(attrib.location);
        glVertexAttribPointer(attrib.location, (GLint)attrib.compenentCount, type, GL_FALSE, (GLsizei)m_Stride, (void*)offset);

        offset += 4 * (int)attrib.compenentCount;
    }

    Unbind();
    glBindVertexArray(0);
}

void GL_VertexBuffer::CalculateCount()
{
    if(m_Layout != nullptr) {
        assert(m_Size % m_Stride == 0);
        m_Count = m_Size / m_Stride;
        m_Geometry.vertexCount = (int)m_Count;
    }
}