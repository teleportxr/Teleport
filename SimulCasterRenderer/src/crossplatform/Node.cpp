// (C) Copyright 2018-2019 Simul Software Ltd

#include "Node.h"

namespace scr
{
	Node::Node(avs::uid id, const std::string& name)
		:id(id), name(name)
	{}

	void Node::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
	{
		if (ShouldUseGlobalTransform())
		{
			if (globalTransform.UpdateModelMatrix(translation, rotation, scale))
			{
				RequestChildrenUpdateTransforms();
			}
		}
		else if (localTransform.UpdateModelMatrix(translation, rotation, scale))
		{
			RequestTransformUpdate();
		}
	}

	void Node::RequestTransformUpdate()
	{
		isTransformDirty = true;

		//The node's children need to update their transforms, as their parent's transform has been updated.
		RequestChildrenUpdateTransforms();
	}

	void Node::SetLastMovement(const avs::MovementUpdate& update)
	{
		lastReceivedMovement = update;
		UpdateModelMatrix(update.position, update.rotation, localTransform.m_Scale);
	}

	void Node::TickExtrapolatedTransform(float deltaTime)
	{
		deltaTime /= 1000;
		Transform& transform = (ShouldUseGlobalTransform() ? GetGlobalTransform() : GetLocalTransform());

		transform.m_Translation += static_cast<avs::vec3>(lastReceivedMovement.velocity)* deltaTime;

		if (lastReceivedMovement.angularVelocityAngle != 0)
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

		//Attempt to animate, if we have a skin.
		if(skin)
		{
			animationComponent.update(skin->GetBones(), deltaTime);
		}

		for (std::weak_ptr<Node> child : children)
		{
			child.lock()->Update(deltaTime);
		}
	}

	void Node::SetParent(std::shared_ptr<Node> node)
	{

		std::shared_ptr<Node> oldParent = parent.lock();

		parent = node;

		//Remove self from parent list of existing parent, if we have a parent.
		//Prevent stack overflow by doing this after setting the new parent.
		if (oldParent)
		{
			oldParent->RemoveChild(id);
		}
	}

	void Node::AddChild(std::shared_ptr<Node> child)
	{
		children.push_back(child);
	}

	void Node::RemoveChild(std::shared_ptr<Node> node)
	{
		for (auto it = children.begin(); it != children.end(); it++)
		{
			std::shared_ptr<Node> child = it->lock();
			if (child == node)
			{
				children.erase(it);
				child->SetParent(nullptr);
				return;
			}
		}
	}

	void Node::RemoveChild(avs::uid childID)
	{
		for (auto it = children.begin(); it != children.end(); it++)
		{
			std::shared_ptr<Node> child = it->lock();
			if (child && child->id == childID)
			{
				children.erase(it);
				child->SetParent(nullptr);
				return;
			}
		}
	}

	void Node::ClearChildren()
	{
		children.clear();
	}

	void Node::SetVisible(bool visible)
	{
		visibility.setVisibility(visible);
	}

	void Node::SetLocalTransform(const Transform& transform)
	{
		if (abs(transform.m_Scale.x) < 0.0001f)
		{
			SCR_CERR << "Failed to update local transform of Node_" << id << "(" << name.c_str() << ")! Scale.x is zero!\n";
			return;
		}

		localTransform = transform;
	}

	void Node::SetLocalPosition(const avs::vec3& value)
	{
		localTransform.m_Translation = value;
		isTransformDirty = true;
	}

	void Node::SetLocalRotation(const scr::quat& value)
	{
		localTransform.m_Rotation = value;
		isTransformDirty = true;
	}

	void Node::SetLocalScale(const avs::vec3& value)
	{
		localTransform.m_Scale = value;
		isTransformDirty = true;
	}

	void Node::UpdateGlobalTransform() const
	{
		std::shared_ptr<Node> parentPtr = parent.lock();
		globalTransform = parentPtr ? localTransform * parentPtr->GetGlobalTransform() : localTransform;

		isTransformDirty = false;

		for (std::weak_ptr<Node> child : children)
		{
			child.lock()->UpdateGlobalTransform();
		}
	}

	void Node::RequestChildrenUpdateTransforms()
	{
		for (auto childIt = children.begin(); childIt != children.end();)
		{
			std::shared_ptr<Node> child = childIt->lock();

			//Erase weak pointer from list, if the child node has been removed.
			if (child)
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
	bool Node::ShouldUseGlobalTransform() const
	{
		//Only use the global transform if we are receiving global transforms for this node and it has a parent.
		return lastReceivedMovement.isGlobal && GetParent().lock();
	}
}