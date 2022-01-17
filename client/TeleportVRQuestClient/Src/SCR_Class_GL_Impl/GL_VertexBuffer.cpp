// (C) Copyright 2018-2019 Simul Software Ltd
#include "GL_VertexBuffer.h"

using namespace scc;
using namespace clientrender;
using namespace OVR;

void GL_VertexBuffer::Create(VertexBufferCreateInfo* pVertexBufferCreateInfo)
{
    m_CI = *pVertexBufferCreateInfo;

    glGenBuffers(1, &m_VertexID);
    glBindBuffer(GL_ARRAY_BUFFER, m_VertexID);
    glBufferData(GL_ARRAY_BUFFER, m_CI.size, m_CI.data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void GL_VertexBuffer::Destroy()
{
    glDeleteBuffers(1, &m_VertexID);
}

void GL_VertexBuffer::Bind() const
{
    glBindVertexArray(m_VertexArrayID);
    glBindBuffer(GL_ARRAY_BUFFER, m_VertexID);
}
void GL_VertexBuffer::Unbind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
void GL_VertexBuffer::CreateVAO(GLuint indexBufferID)
{
	glGenVertexArrays(1, &m_VertexArrayID);
	glBindVertexArray(m_VertexArrayID);

	//VBO and Layout
	Bind();
	size_t offset = 0;
	for (auto& attrib : m_CI.layout->m_Attributes)
	{
		GLenum type;
		switch (attrib.type)
		{
		case VertexBufferLayout::Type::HALF:
			type = GL_HALF_FLOAT;
			break;
		case VertexBufferLayout::Type::FLOAT:
			type = GL_FLOAT;
			break;
		case VertexBufferLayout::Type::DOUBLE:
			type = 0;
			SCR_COUT_BREAK("OpenGL ES 3.0 does not support GL_DOUBLE", -1);
			break;
		case VertexBufferLayout::Type::UINT:
			type = GL_UNSIGNED_INT;
			break;
		case VertexBufferLayout::Type::USHORT:
			type = GL_UNSIGNED_SHORT;
			break;
		case VertexBufferLayout::Type::UBYTE:
			type = GL_UNSIGNED_BYTE;
			break;
		case VertexBufferLayout::Type::INT:
			type = GL_INT;
			break;
		case VertexBufferLayout::Type::SHORT:
			type = GL_SHORT;
			break;
		case VertexBufferLayout::Type::BYTE:
			type = GL_BYTE;
			break;
		default:
			SCR_COUT_BREAK("OpenGL ES 3.0 does not support this type", -1);
			break;

		}

		glEnableVertexAttribArray(attrib.location);
		size_t this_size = 4 * (int)attrib.componentCount;
		if(m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::INTERLEAVED)
		{
			glVertexAttribPointer(attrib.location, (GLint) attrib.componentCount, type, GL_FALSE, (GLsizei) m_CI.layout->m_Stride, (void*) offset);
			offset += this_size;
		}
		else if (m_CI.layout->m_PackingStyle == VertexBufferLayout::PackingStyle::GROUPED)
		{
			glVertexAttribPointer(attrib.location, (GLint) attrib.componentCount, type, GL_FALSE, (GLsizei)this_size, (void*) offset); //Stride could be 0; as buffer is tightly packed
			offset += this_size * m_CI.vertexCount;
		}
		else
		{
			SCR_COUT_BREAK("Unknown VertexBufferLayout::PackingStyle", -1);
		}
	}

    //IBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
    glBindVertexArray(0);
}