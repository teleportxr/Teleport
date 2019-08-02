// (C) Copyright 2018-2019 Simul Software Ltd

#include "Object.h"

using namespace scr;

Object::Object(bool staticModel, std::vector<VertexBuffer>& vbos, IndexBuffer& ibo, UniformBuffer& ubo, float modelMatrix[16], Material& material)
	:m_Static(staticModel), m_VBOs(vbos), m_IBO(ibo), m_UBO(ubo), m_Material(material)
{
	UpdateModelMatrix(m_ModelMatrix);
};

void Object::Bind()
{
	for (auto& vbo : m_VBOs)
	{
		vbo.Bind();
	}
	m_IBO.Bind();
	m_UBO.Update(0, m_Size, &m_ModelMatrix[0]);
	m_Material.Bind();
}

void Object::UpdateModelMatrix(float modelMatrix[16])
{
	memcpy(&m_ModelMatrix[0], &modelMatrix[0], m_Size);
}
