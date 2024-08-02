// (C) Copyright 2018-2023 Simul Software Ltd
#include "Node.h"
#include "TeleportClient/basic_linear_algebra.h"

using namespace teleport;
using namespace clientrender;

using InvisibilityReason = VisibilityComponent::InvisibilityReason;

Node::Node(avs::uid id, const std::string& name)
	:IncompleteNode(id), name(name), isStatic(false)
{}

void Node::SetStatic(bool s)
{
	isStatic=s;
}

void Node::SetHolderClientId(avs::uid h)
{
	holderClientId=h;
}

avs::uid Node::GetHolderClientId() const
{
	return holderClientId;
}

void Node::SetPriority(int p)
{
	priority=p;
}

int Node::GetPriority()const
{
	return priority;
}

bool Node::IsStatic() const
{
	return isStatic;
}

void Node::UpdateModelMatrix(vec3 translation, quat rotation, vec3 scale)
{
	if(ShouldUseGlobalTransform())
	{
		if(globalTransform.UpdateModelMatrix(translation, rotation, scale))
		{
			RequestChildrenUpdateTransforms();
		}
	}
	else if(localTransform.UpdateModelMatrix(translation, rotation, scale))
	{
		RequestTransformUpdate();
	}
}

void Node::UpdateModelMatrix()
{
	globalTransform.UpdateModelMatrix();
	localTransform.UpdateModelMatrix();	
}

void Node::RequestTransformUpdate()
{
	isTransformDirty = true;

	//The node's children need to update their transforms, as their parent's transform has been updated.
	RequestChildrenUpdateTransforms();
}

void Node::SetLastMovement(const teleport::core::MovementUpdate& update)
{
	lastReceivedMovement = update;

	//Set transform, then tick based on difference in time since the update was sent and now.
	UpdateModelMatrix(update.position, *((quat*)&update.rotation), update.scale);
	//TickExtrapolatedTransform(static_cast<float>(teleport::client::ServerTimestamp::getCurrentTimestampUTCUnixMs()));
}
#pragma optimize("",off)
// Here we will extrapolate the transform based on the last received movement update.
void Node::TickExtrapolatedTransform(double serverTimeS)
{
	if (isStatic)
		return;
	// timestamp received was in milliseconds Unix time.
	if (!lastReceivedMovement.server_time_us)
		return;
	// We want to extrapolate the transform from the last timestamp to the "server time" we received as a parameter.
	// so we want the difference in seconds between these.
	double lastTimeS = double(lastReceivedMovement.server_time_us) * 0.000001;
	double time_offset = serverTimeS - lastTimeS;
	if (time_offset < -5.0)
		time_offset = -5.0;
	if (time_offset > 5.0)
		time_offset = 5.0;
	const Transform& transform = (ShouldUseGlobalTransform() ? GetGlobalTransform() : GetLocalTransform());
	vec3 p0 = lastReceivedMovement.position;
	vec3 newTranslation = p0;
	vec3 v = lastReceivedMovement.velocity;
	if (length(v) > 0)
	{
		newTranslation +=v * (float)time_offset;
		smoothingEnabled = true;
	}
	clientrender::quat newRotation = *((clientrender::quat*)&lastReceivedMovement.rotation);
	if(lastReceivedMovement.angularVelocityAngle != 0)
	{
		quat deltaRotation(lastReceivedMovement.angularVelocityAngle * (float)time_offset, lastReceivedMovement.angularVelocityAxis);
		newRotation = newRotation* deltaRotation;
		smoothingEnabled = true;
	}
	if (!smoothingInitialized||!smoothingEnabled)
	{
		smoothedTransform.m_Translation =  newTranslation;
		if(smoothingEnabled)
			smoothingInitialized = true;
	}
	else
	{
		static float smoothing_rate = 1.0f;;
		float interp= 1.0f - 1.0f/(1.0f+smoothing_rate* 0.01688f);
		smoothedTransform.m_Translation = lerp(smoothedTransform.m_Translation, newTranslation, interp);
		smoothedTransform.m_Rotation	= platform::crossplatform::slerp(smoothedTransform.m_Rotation, newRotation, interp);
	}
	UpdateModelMatrix(smoothedTransform.m_Translation, newRotation, transform.m_Scale);
}

void Node::UpdateExtrapolatedPositions(double serverTimeS)
{
	TickExtrapolatedTransform(serverTimeS);
}

void Node::Update( std::chrono::microseconds timestamp_us)
{
	visibility.update(timestamp_us.count());

	// Attempt to animate, if we have a skeleton.
	if(skeletonInstance&&skeletonInstance->GetSkeleton())
	{
		auto animC=GetOrCreateComponent<AnimationComponent>();
		animC->update(skeletonInstance->GetSkeleton()->GetExternalBones(), timestamp_us.count());
	}

	for(std::weak_ptr<Node> child : children)
	{
		std::shared_ptr n = child.lock();
		if(n)
			n->Update(timestamp_us);
	}
}

bool Node::HasAnyParent(std::shared_ptr<Node> node) const
{
	auto p = parent;
	while (auto l=p.lock())
	{
		if (node == l)
			return true;
		p = l->parent;
	}
	return false;
}

void Node::SetParent(std::shared_ptr<Node> newParent)
{
	if(parent.use_count())
	{
		std::shared_ptr<Node> oldParent = parent.lock();
		if (oldParent == newParent)
			return;
		parent = newParent;
		//Remove self from parent list of existing parent, if we have a parent.
		//Prevent stack overflow by doing this after setting the new parent.
		//
		// This would cause a mutex double-lock for childrenMutex, can't do this.
		/*if(oldParent)
		{
			oldParent->RemoveChild(id);
		}*/
	}
	else
		parent=newParent;
	// New parent may have different position.
	RequestTransformUpdate();
}

void Node::AddChild(std::shared_ptr<Node> child)
{
	// avoid nasty loops.
	if (HasAnyParent(child))
		return;
	children.push_back(child);
}

void Node::RemoveChild(std::shared_ptr<Node> node)
{
	std::lock_guard lock(childrenMutex);
	for(auto it = children.begin(); it != children.end(); it++)
	{
		std::shared_ptr<Node> child = it->lock();
		if(child == node)
		{
			children.erase(it);
			child->SetParent(nullptr);
			return;
		}
	}
}

void Node::RemoveChild(avs::uid childID)
{
	std::lock_guard lock(childrenMutex);
	for(auto it = children.begin(); it != children.end(); it++)
	{
		std::shared_ptr<Node> child = it->lock();
		if(child && child->id == childID)
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
	if (visible == visibility.getVisibility())
		return;
	visibility.setVisibility(visible, InvisibilityReason::OUT_OF_BOUNDS);
}

void Node::SetLocalTransform(const Transform& transform)
{
	if(abs(transform.m_Scale.x) < 0.0001f)
	{
		//TELEPORT_CERR << "Failed to update local transform of Node_" << id << "(" << name.c_str() << ")! Scale.x is zero!\n";
		return;
	}
	localTransform = transform;
	isTransformDirty = true;
	// The node's children need to update their transforms, as their parent's transform has been updated.
	RequestChildrenUpdateTransforms();
}

void Node::SetGlobalTransform(const Transform& transform)
{
	globalTransform=transform;
	isTransformDirty = false;
	//The node's children need to update their transforms, as their parent's transform has been updated.
	RequestChildrenUpdateTransforms();
}

void Node::SetLocalPosition(const vec3& value)
{
	localTransform.m_Translation = value;
	RequestTransformUpdate();
}

void Node::SetLocalRotation(const clientrender::quat& value)
{
	localTransform.m_Rotation = value;
	RequestTransformUpdate();
}

void Node::SetLocalVelocity(const vec3& value)
{
	localTransform.m_Velocity = value;
}

void Node::SetLocalScale(const vec3& value)
{
	localTransform.m_Scale = value;
	RequestTransformUpdate();
}

void Node::SetHighlighted(bool highlighted)
{
	isHighlighted = highlighted;
}

void Node::UpdateGlobalTransform() const
{
	std::shared_ptr<Node> parentPtr = parent.lock();
	if(parentPtr)
	{
		if(parentPtr->parent.lock().get()==this)
		{
			TELEPORT_BREAK_ONCE("Nodes in loop.");
			return;
		}
		Transform::Multiply(globalTransform,localTransform,parentPtr->GetGlobalTransform());
	}
	else
		globalTransform =  localTransform;

	isTransformDirty = false;
}

void Node::RecursiveUpdateGlobalTransform(const Transform &parentGlobalTransform) const
{
	std::lock_guard lock(childrenMutex);
	Transform::Multiply(globalTransform, localTransform, parentGlobalTransform);
	for (auto it = children.begin(); it != children.end();it++)
	{
		std::shared_ptr<Node> child = it->lock();
		// Erase weak pointer from list, if the child node has been removed.
		if (child)
		{
			child->RecursiveUpdateGlobalTransform(globalTransform);
		}
	}
	isTransformDirty = false;
}

void Node::RequestChildrenUpdateTransforms()
{
	std::lock_guard lock(childrenMutex);
	for (size_t i =0; i<children.size();i++)
	{
		std::shared_ptr<Node> child = children[i].lock();

		//Erase weak pointer from list, if the child node has been removed.
		if(child)
		{
			child->RequestTransformUpdate();
		}
		else
		{
			children.erase(children.begin()+i);
			i--;
		}
	}
}

bool Node::ShouldUseGlobalTransform() const
{
	//Only use the global transform if we are receiving global transforms for this node and it has a parent.
	return lastReceivedMovement.isGlobal && GetParent().lock();
}

void Node::SetMaterialListSize(size_t size)
{
	materials.resize(size);
}

void Node::SetMaterialList(std::vector<std::shared_ptr<Material>>& m)
{
	materials = m;
}