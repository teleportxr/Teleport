// (C) Copyright 2018-2019 Simul Software Ltd

#include "Node.h"

using namespace scr;

Node::Node(avs::uid id)
	:id(id)
{}

void Node::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
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

void Node::RequestTransformUpdate()
{
	isTransformDirty = true;

	//The actor's children need to update their transforms, as their parent's transform has been updated.
	for(auto childIt = children.begin(); childIt != children.end();)
	{
		std::shared_ptr<Node> child = childIt->lock();

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

void Node::SetLastMovement(const avs::MovementUpdate& update)
{
	lastReceivedMovement = update;

	UpdateModelMatrix(update.position, update.rotation, localTransform.m_Scale);
}

void Node::TickExtrapolatedTransform(float deltaTime)
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

void Node::Update(float deltaTime)
{
	TickExtrapolatedTransform(deltaTime);
	visibility.update(deltaTime);

	for(std::weak_ptr<Node> child : children)
	{
		child.lock()->Update(deltaTime);
	}
}

void Node::SetParent(std::weak_ptr<Node> parent)
{
	this->parent = parent;
}

void Node::AddChild(std::weak_ptr<Node> child)
{
	children.push_back(child);
}

void Node::RemoveChild(std::weak_ptr<Node> actorPtr)
{
	std::shared_ptr<Node> actor = actorPtr.lock();
	for(auto it = children.begin(); it != children.end(); it++)
	{
		if(it->lock() == actor)
		{
			children.erase(it);
			return;
		}
	}
}

void Node::SetVisible(bool visible)
{
	visibility.setVisibility(visible);
}

void Node::UpdateGlobalTransform() const
{
	std::shared_ptr<Node> parentPtr = parent.lock();
	globalTransform = parentPtr ? localTransform * parentPtr->GetGlobalTransform() : localTransform;

	isTransformDirty = false;

	for(std::weak_ptr<Node> child : children)
	{
		child.lock()->UpdateGlobalTransform();
	}
}

