// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <libavstream/geometry/mesh_interface.hpp>

#include "NodeComponents.h"
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
	//Requests global transform of node, and node's children, be recalculated.
	void RequestTransformUpdate();

	void SetLastMovement(const avs::MovementUpdate& update);
	//Updates the transform by extrapolating data from the last confirmed timestamp.
	void TickExtrapolatedTransform(float deltaTime);

	void Update(float deltaTime);

	void SetParent(std::shared_ptr<Node> parent);
	std::weak_ptr<Node> GetParent() const { return parent; }

	void AddChild(std::shared_ptr<Node> child);
	void RemoveChild(std::shared_ptr<Node> child);
	void RemoveChild(avs::uid childID);
	void ClearChildren();

	const std::vector<std::weak_ptr<Node>>& GetChildren() const { return children; }
	void SetChildrenIDs(std::vector<avs::uid>& childrenIDs) { childIDs = childrenIDs; }
	const std::vector<avs::uid>& GetChildrenIDs() const { return childIDs; }

	bool IsVisible() const { return visibility.getVisibility(); }
	void SetVisible(bool visible);
	float GetTimeSinceLastVisible() const { return visibility.getTimeSinceLastVisible(); }

	virtual void SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
	std::shared_ptr<Mesh> GetMesh() const { return mesh; }

	virtual void SetSkin(std::shared_ptr<Skin> skin) { this->skin = skin; }
	const std::shared_ptr<Skin> GetSkin() const { return skin; }
	std::shared_ptr<Skin> GetSkin() { return skin; }

	virtual void SetMaterial(size_t index, std::shared_ptr<Material> material)
	{
		if(index >= materials.size() || index < 0)
		{
			SCR_CERR << "Failed to set material at index " << index << "! Index not valid for material list of size " << materials.size() << " in node \"" << name.c_str() << "\"(Node_" << id << ")!\n";
		}
		else
		{
			materials[index] = material;
		}
	}
	std::shared_ptr<Material> GetMaterial(size_t index)
	{
		if(index >= materials.size() || index < 0)
		{
			SCR_CERR << "Failed to get material at index " << index << "! Index not valid for material list of size " << materials.size() << " in node \"" << name.c_str() << "\"(Node_" << id << ")!\n";
			return nullptr;
		}

		return materials[index];
	}

	virtual void SetMaterialListSize(size_t size) { materials.resize(size); }
	virtual void SetMaterialList(std::vector<std::shared_ptr<Material>>& materials) { this->materials = materials; }
	const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return materials; }
	size_t GetMaterialAmount() const { return materials.size(); }

	void SetLocalTransform(const Transform& transform);
	const Transform& GetLocalTransform() const { return localTransform; }
	Transform& GetLocalTransform() { return localTransform; }

	const Transform& GetGlobalTransform() const
	{
		if(isTransformDirty)
		{
			UpdateGlobalTransform();
		}

		return globalTransform;
	}

	Transform& GetGlobalTransform()
	{
		if(isTransformDirty)
		{
			UpdateGlobalTransform();
		}

		return globalTransform;
	}

	void SetLocalPosition(const avs::vec3& value);
	const avs::vec3& GetLocalPosition() const { return GetLocalTransform().m_Translation; }
	const avs::vec3& GetGlobalPosition() const { return GetGlobalTransform().m_Translation; }
	
	void SetLocalRotation(const scr::quat& value);
	const scr::quat& GetLocalRotation() const { return GetLocalTransform().m_Rotation; }
	const scr::quat& GetGlobalRotation() const { return GetGlobalTransform().m_Rotation; }

	void SetLocalScale(const avs::vec3& value);
	const avs::vec3& GetLocalScale() const { return GetLocalTransform().m_Scale; }
	const avs::vec3& GetGlobalScale() const { return GetGlobalTransform().m_Scale; }


private:
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

	//Causes the global transforms off all children, and their children, to update whenever their global transform is requested.
	void RequestChildrenUpdateTransforms();

	//Whether we should use the global transform directly for update calculations.
	bool ShouldUseGlobalTransform() const;
};
}