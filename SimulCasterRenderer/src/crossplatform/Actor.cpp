// (C) Copyright 2018-2019 Simul Software Ltd

#include "Actor.h"

using namespace scr;

//Transform
bool Transform::s_UninitialisedUB = false;

Transform::Transform()
{
	if (s_UninitialisedUB)
	{
		const float zero[sizeof(TransformData)] = { 0 };
		
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.bindingLocation = 1;
		ub_ci.size = sizeof(TransformData);
		ub_ci.data = zero;

		m_UB->Create(&ub_ci);
		s_UninitialisedUB = true;
	}

	m_SetLayout.AddBinding(1, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_VERTEX);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddBuffer(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, 1, "u_ActorUBO", { m_UB.get(), 0, sizeof(TransformData) });
}
void Transform::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_Translation = translation;
	m_Rotation = rotation;
	m_Scale = scale;
	m_TransformData.m_ModelMatrix = mat4::Translation(translation) * mat4::Rotation(rotation) * mat4::Scale(scale);
}
void Transform::UpdateModelUBO() const
{
	m_UB->Update(0, sizeof(TransformData), &m_TransformData);
}

//Actor
Actor::Actor(ActorCreateInfo* pActorCreateInfo)
	:m_CI(*pActorCreateInfo)
{
};

void Actor::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_CI.transform->UpdateModelMatrix(translation, rotation, scale);
}
