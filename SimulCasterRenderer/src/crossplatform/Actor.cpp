// (C) Copyright 2018-2019 Simul Software Ltd

#include "Actor.h"

namespace scr
{

//Transform
bool Transform::s_UninitialisedUB = true;
std::shared_ptr<UniformBuffer> Transform::s_UB = nullptr;

Transform::Transform()
	:Transform(TransformCreateInfo{nullptr}, avs::vec3(), quat(), avs::vec3())
{}

Transform::Transform(const TransformCreateInfo& pTransformCreateInfo)
	: Transform(pTransformCreateInfo, avs::vec3(), quat(), avs::vec3())
{}

Transform::Transform(const TransformCreateInfo& pTransformCreateInfo, avs::vec3 translation, quat rotation, avs::vec3 scale)
	: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	if(false)//s_UninitialisedUB)
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

void Transform::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	m_Translation = translation;
	m_Rotation = rotation;
	m_Scale = scale;
	m_TransformData.m_ModelMatrix = mat4::Translation(translation) * mat4::Rotation(rotation) * mat4::Scale(scale);
}

//Actor
Actor::Actor(const ActorCreateInfo& pActorCreateInfo)
	:m_CI(pActorCreateInfo)
{}

void Actor::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	m_CI.localTransform.UpdateModelMatrix(translation, rotation, scale);
	RequestTransformUpdate();
}

void Actor::RequestTransformUpdate()
{
	isTransformDirty = true;

	//The actor's children need to update their transforms, as their parent's transform has been updated.
	for(auto childIt = children.begin(); childIt != children.end();)
	{
		std::shared_ptr<Actor> child = childIt->lock();

		//Erase weak pointer from list, if the child actor has been removed.
		if(child)
		{
			child->RequestTransformUpdate();
			++childIt;
		}
		else
		{
			childIt = children.erase(childIt);
		}
	}
}

void Actor::SetLastMovement(const avs::MovementUpdate& update)
{
	lastReceivedMovement = update;

	UpdateModelMatrix(update.position, update.rotation, m_CI.localTransform.m_Scale);
}

void Actor::TickExtrapolatedTransform(float deltaTime)
{
	deltaTime /= 1000;
	m_CI.localTransform.m_Translation += static_cast<avs::vec3>(lastReceivedMovement.velocity) * deltaTime;

	if(lastReceivedMovement.angularVelocityAngle != 0)
	{
		quat deltaRotation(lastReceivedMovement.angularVelocityAngle * deltaTime, lastReceivedMovement.angularVelocityAxis);
		m_CI.localTransform.m_Rotation *= deltaRotation;
	}

	UpdateModelMatrix(m_CI.localTransform.m_Translation, m_CI.localTransform.m_Rotation, m_CI.localTransform.m_Scale);
}

void Actor::SetParent(std::weak_ptr<Actor> parent)
{
	this->parent = parent;
}

void Actor::AddChild(std::weak_ptr<Actor> child)
{
	children.push_back(child);
}

void Actor::UpdateGlobalTransform() const
{
	std::shared_ptr<Actor> parentPtr = parent.lock();
	globalTransform = parentPtr ? m_CI.localTransform * parentPtr->GetGlobalTransform() : m_CI.localTransform;

	isTransformDirty = false;
}

}