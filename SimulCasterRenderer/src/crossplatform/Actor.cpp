// (C) Copyright 2018-2019 Simul Software Ltd

#include "Actor.h"

namespace scr
{
Actor::Actor(avs::uid id)
	:id(id)
{}

void Actor::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	if(lastReceivedMovement.isGlobal)
	{
		globalTransform.UpdateModelMatrix(translation, rotation, scale);
	}
	else
	{
		localTransform.UpdateModelMatrix(translation, rotation, scale);
		RequestTransformUpdate();
	}
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

	UpdateModelMatrix(update.position, update.rotation, localTransform.m_Scale);
}

void Actor::TickExtrapolatedTransform(float deltaTime)
{
	deltaTime /= 1000;
	scr::Transform& transform = (lastReceivedMovement.isGlobal ? globalTransform : localTransform);

	transform.m_Translation += static_cast<avs::vec3>(lastReceivedMovement.velocity) * deltaTime;

	if(lastReceivedMovement.angularVelocityAngle != 0)
	{
		quat deltaRotation(lastReceivedMovement.angularVelocityAngle * deltaTime, lastReceivedMovement.angularVelocityAxis);
		transform.m_Rotation *= deltaRotation;
	}

	UpdateModelMatrix(transform.m_Translation, transform.m_Rotation, transform.m_Scale);
}

void Actor::Update(float deltaTime)
{
	TickExtrapolatedTransform(deltaTime);
	visibility.update(deltaTime);

	for(std::weak_ptr<Actor> child : children)
	{
		child.lock()->Update(deltaTime);
	}
}

void Actor::SetParent(std::weak_ptr<Actor> parent)
{
	this->parent = parent;
}

void Actor::AddChild(std::weak_ptr<Actor> child)
{
	children.push_back(child);
}

void Actor::RemoveChild(std::weak_ptr<Actor> actorPtr)
{
	std::shared_ptr<Actor> actor = actorPtr.lock();
	for(auto it = children.begin(); it != children.end(); it++)
	{
		if(it->lock() == actor)
		{
			children.erase(it);
			return;
		}
	}
}

void Actor::SetVisible(bool visible)
{
	visibility.setVisibility(visible);
}

void Actor::UpdateGlobalTransform() const
{
	std::shared_ptr<Actor> parentPtr = parent.lock();
	globalTransform = parentPtr ? localTransform * parentPtr->GetGlobalTransform() : localTransform;

	isTransformDirty = false;

	for(std::weak_ptr<Actor> child : children)
	{
		child.lock()->UpdateGlobalTransform();
	}
}

}