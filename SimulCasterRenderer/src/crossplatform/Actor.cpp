// (C) Copyright 2018-2019 Simul Software Ltd

#include "Actor.h"

using namespace scr;

//Transform
bool Transform::s_UninitialisedUB = true;
std::shared_ptr<UniformBuffer> Transform::s_UB = nullptr;

Transform::Transform()
	:Transform(TransformCreateInfo{nullptr}, vec3(), quat(), vec3())
{}

Transform::Transform(const TransformCreateInfo& pTransformCreateInfo)
	:Transform(pTransformCreateInfo, vec3(), quat(), vec3())
{}

Transform::Transform(const TransformCreateInfo& pTransformCreateInfo, vec3 translation, quat rotation, vec3 scale)
	:m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	if (false)//s_UninitialisedUB)
	{
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.bindingLocation = 1;
		ub_ci.size = sizeof(TransformData);
		ub_ci.data = &m_TransformData;

		s_UB = m_CI.renderPlatform->InstantiateUniformBuffer();
		s_UB->Create(&ub_ci);
		s_UninitialisedUB = false;
	}

	m_ShaderResourceLayout.AddBinding(1, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_VERTEX);

	m_ShaderResource = ShaderResource({m_ShaderResourceLayout});
	m_ShaderResource.AddBuffer(0, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 1, "u_ActorUBO", {s_UB.get(), 0, sizeof(TransformData)});

	m_TransformData.m_ModelMatrix = mat4::Translation(translation) * mat4::Rotation(rotation) * mat4::Scale(scale);
}

void Transform::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_Translation = translation;
	m_Rotation = rotation;
	m_Scale = scale;
	m_TransformData.m_ModelMatrix = mat4::Translation(translation) * mat4::Rotation(rotation) * mat4::Scale(scale);
}

//Actor
Actor::Actor(const ActorCreateInfo& pActorCreateInfo)
	:m_CI(pActorCreateInfo)
{
}

void Actor::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	m_CI.transform.UpdateModelMatrix(translation, rotation, scale);
}

void scr::Actor::UpdateLastMovement(const avs::MovementUpdate& update)
{
	lastReceivedMovement = update;

	UpdateModelMatrix(update.position, update.rotation, m_CI.transform.m_Scale);
}


void Actor::TickExtrapolatedTransform(float deltaTime)
{
	deltaTime /= 1000;
	m_CI.transform.m_Translation += static_cast<scr::vec3>(lastReceivedMovement.velocity) * deltaTime;

	if(lastReceivedMovement.angularVelocityAngle != 0)
	{
		scr::quat deltaRotation(lastReceivedMovement.angularVelocityAngle * deltaTime, lastReceivedMovement.angularVelocityAxis);
		m_CI.transform.m_Rotation *= deltaRotation;
	}

	UpdateModelMatrix(m_CI.transform.m_Translation, m_CI.transform.m_Rotation, m_CI.transform.m_Scale);
}
