// (C) Copyright 2018-2019 Simul Software Ltd

#include "Object.h"

using namespace scr;

bool Object::s_UninitialisedUBO = false;

Object::Object(bool staticModel, VertexBuffer& vbo, IndexBuffer& ibo, vec3& translation, quat& rotation, vec3& scale, Material& material)
	:m_Static(staticModel), m_VBO(vbo), m_IBO(ibo), m_Material(material), m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	if (s_UninitialisedUBO)
	{
		const float zero[sizeof(ModelData)] = { 0 };
		m_UBO->Create(sizeof(ModelData), zero, 1);
		s_UninitialisedUBO = true;
	}
	UpdateModelMatrix(translation, rotation, scale);

	m_SetLayout.AddBinding(1, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_VERTEX);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddBuffer(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, 1, { m_UBO.get(), 0, sizeof(ModelData) });
};

void Object::BindGeometries()
{
	m_VBO.Bind();
	m_IBO.Bind();
}
void Object::UpdateModelUBO()
{
	m_UBO->Update(0, sizeof(ModelData), &m_ModelData);
}

void Object::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_Translation = translation;
	m_Rotation = rotation;
	m_Scale = scale;
	m_ModelData.m_ModelMatrix = mat4::Translation(m_Translation) * mat4::Rotation(m_Rotation) * mat4::Scale(m_Scale);
}
