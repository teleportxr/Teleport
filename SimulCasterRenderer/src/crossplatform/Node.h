// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <libavstream/geometry/mesh_interface.hpp>

#include "ActorComponents.h"
#include "api/UniformBuffer.h"
#include "basic_linear_algebra.h"
#include "Material.h"
#include "Mesh.h"
#include "Skin.h"
#include "Transform.h"

namespace scr
{
class Node
{
public:
	const avs::uid id;
	const std::string name;

	VisibilityComponent visibility;
	AnimationComponent animationComponent;

	Node(avs::uid id, const std::string& name);

	virtual ~Node() = default;

	void UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);
	//Requests global transform of actor, and actor's children, be recalculated.
	void RequestTransformUpdate();

	void SetLastMovement(const avs::MovementUpdate& update);
	//Updates the transform by extrapolating data from the last confirmed timestamp.
	void TickExtrapolatedTransform(float deltaTime);

	void Update(float deltaTime);

	void SetParent(std::weak_ptr<Node> parent);
	void AddChild(std::weak_ptr<Node> child);
	void RemoveChild(std::weak_ptr<Node> child);

	bool IsVisible() const { return visibility.getVisibility(); }
	void SetVisible(bool visible);
	float GetTimeSinceLastVisible() const { return visibility.getTimeSinceLastVisible(); }

	std::weak_ptr<Node> GetParent() const { return parent; }
	const std::vector<std::weak_ptr<Node>>& GetChildren() const { return children; }

	void SetChildrenIDs(std::vector<avs::uid>& childrenIDs) { childIDs = childrenIDs; }
	const std::vector<avs::uid>& GetChildrenIDs() const { return childIDs; }

	virtual void SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
	std::shared_ptr<Mesh> GetMesh() const { return mesh; }

	virtual void SetSkin(std::shared_ptr<Skin> skin) { this->skin = skin; }
	const std::shared_ptr<Skin> GetSkin() const { return skin; }
	std::shared_ptr<Skin> GetSkin() { return skin; }

	virtual void SetMaterial(size_t index, std::shared_ptr<Material> material)
	{
		if(index < materials.size()) materials[index] = material;
		else SCR_COUT << "ERROR: Attempted to add material at index <" << index << "> past the end of the material list of Node<" << id << "> of size: " << materials.size() << std::endl;
	}
	virtual void SetMaterialListSize(size_t size) { materials.resize(size); }
	virtual void SetMaterialList(std::vector<std::shared_ptr<Material>>& materials) { this->materials = materials; }
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
		if(isTransformDirty)
			UpdateGlobalTransform();
		return globalTransform;
	}
protected:
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Skin> skin;
	std::vector<std::shared_ptr<Material>> materials;
	Transform localTransform;

	std::vector<avs::uid> childIDs;

	//Cached global transform, and dirty flag; updated when necessary on a request.
	mutable bool isTransformDirty = true;
	mutable Transform globalTransform;

	std::weak_ptr<Node> parent;
	std::vector<std::weak_ptr<Node>> children;

	avs::MovementUpdate lastReceivedMovement;

	void UpdateGlobalTransform() const;
};
}