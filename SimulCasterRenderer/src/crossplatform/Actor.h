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
	const avs::uid id;

	Actor(avs::uid id);

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

	void SetChildrenIDs(std::vector<avs::uid>& childrenIDs) { childIDs = childrenIDs; }
	const std::vector<avs::uid>& GetChildrenIDs() const { return childIDs; }

	void SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
	std::shared_ptr<Mesh> GetMesh() const { return mesh; }

	void SetMaterial(size_t index, std::shared_ptr<Material> material)
	{
		if(index < materials.size()) materials[index] = material;
		else SCR_COUT << "ERROR: Attempted to add material at index <" << index << "> past the end of the material list of Actor<" << id << "> of size: " << materials.size() << std::endl;
	}
	void SetMaterialListSize(size_t size) { materials.resize(size); }
	void SetMaterialList(std::vector<std::shared_ptr<Material>>& materials) { this->materials = materials; }
	const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return materials; }

	void SetLocalTransform(const Transform& transform) { localTransform = transform; }
	const Transform& GetLocalTransform() const { return localTransform; }
	Transform& GetLocalTransform() { return localTransform; }

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
	std::shared_ptr<Mesh> mesh;
	std::vector<std::shared_ptr<Material>> materials;
	Transform localTransform;

	std::vector<avs::uid> childIDs;

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