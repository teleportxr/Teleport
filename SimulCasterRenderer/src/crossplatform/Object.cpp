// (C) Copyright 2018-2019 Simul Software Ltd

#include "Object.h"

using namespace scr;

bool Object::s_UninitialisedUBO = false;

Object::Object(bool staticModel, std::vector<VertexBuffer>& vbos, IndexBuffer& ibo, vec3& translation, quat& rotation, vec3& scale, Material& material)
	:m_Static(staticModel), m_VBOs(vbos), m_IBO(ibo), m_Material(material), m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	if (s_UninitialisedUBO)
	{
		const float zero[sizeof(ModelData)] = { 0 };
		m_UBO->Create(sizeof(ModelData), zero, 1);
		s_UninitialisedUBO = true;
	}
	UpdateModelMatrix(translation, rotation, scale);
};

void Object::Bind()
{
	for (auto& vbo : m_VBOs)
	{
		vbo.Bind();
	}
	m_IBO.Bind();
	m_UBO->Update(0, sizeof(ModelData), &m_ModelData);
	m_Material.Bind();
}

void Object::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_Translation = translation;
	m_Rotation = rotation;
	m_Scale = scale;
	m_ModelData.m_ModelMatrix = mat4::Translation(m_Translation) * mat4::Rotation(m_Rotation) * mat4::Scale(m_Scale);
}
