// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <libavstream/geometry/mesh_interface.hpp>

#include "ActorComponents.h"
#include "api/UniformBuffer.h"
#include "basic_linear_algebra.h"
#include "Material.h"
#include "Mesh.h"
#include "Transform.h"

namespace scr
{

class Actor
{
public:
	struct ActorCreateInfo
	{
		bool staticMesh;	//Will the mesh move throughout the scene?
		bool animatedMesh;	//Will the mesh deform?
		std::shared_ptr<Mesh> mesh;
		std::vector<std::shared_ptr<Material>> materials;
		Transform localTransform;

		std::vector<avs::uid> childIDs;
	};

	const avs::uid id;
	
	Actor(avs::uid id);

	virtual void Init(const ActorCreateInfo& pActorCreateInfo);

	void UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);
	//Requests global transform of actor, and actor's children, be recalculated.
	void RequestTransformUpdate();

	void SetLastMovement(const avs::MovementUpdate& update);
	//Updates the transform by extrapolating data from the last confirmed timestamp.
	void TickExtrapolatedTransform(float deltaTime);

	void Update(float deltaTime);

	void SetParent(std::weak_ptr<Actor> parent);
	void AddChild(std::weak_ptr<Actor> child);
	void RemoveChild(std::weak_ptr<Actor> child);

	bool IsVisible() const { return visibility.getVisibility(); }
	void SetVisible(bool visible);
	float GetTimeSinceLastVisible() const { return visibility.getTimeSinceLastVisible(); }

	std::weak_ptr<Actor> GetParent() const { return parent; }
	const std::vector<std::weak_ptr<Actor>>& GetChildren() const { return children; }
	const std::vector<avs::uid>& GetChildrenIDs() const { return m_CI.childIDs; }

	std::shared_ptr<Mesh> GetMesh() const { return m_CI.mesh; }
	const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return m_CI.materials; }

	const Transform& GetLocalTransform() const { return m_CI.localTransform; } const
	Transform& GetLocalTransform() { return m_CI.localTransform; }

	const Transform& GetGlobalTransform() const
	{
		if(isTransformDirty) UpdateGlobalTransform();
		return globalTransform;
	}

	Transform& GetGlobalTransform()
	{
		if(isTransformDirty) UpdateGlobalTransform();
		return globalTransform;
	}

private:
	ActorCreateInfo m_CI;

	//Cached global transform, and dirty flag; updated when necessary on a request.
	mutable bool isTransformDirty = true;
	mutable Transform globalTransform;

	std::weak_ptr<Actor> parent;
	std::vector<std::weak_ptr<Actor>> children;

	avs::MovementUpdate lastReceivedMovement;

	VisibilityComponent visibility;

	void UpdateGlobalTransform() const;
};

}