// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "TeleportCore/CommonNetworking.h"
#include "libavstream/geometry/mesh_interface.hpp"

#include "TeleportClient/basic_linear_algebra.h"

#include "ClientRender/UniformBuffer.h"
#include "Material.h"
#include "Mesh.h"
#include "NodeComponents/AnimationComponent.h"
#include "NodeComponents/VisibilityComponent.h"
#include "TextCanvas.h"
#include "Skin.h"
#include "SkinInstance.h"
#include "Transform.h"
#include "ResourceManager.h"

namespace clientrender
{
	class Node: public IncompleteNode
	{
	public:
		const std::string name;

		// distance from the viewer - so we can sort nodes from front to back.
		float distance=1.0f;
		mutable float countdown = 2.0f;
		VisibilityComponent visibility;
		AnimationComponent animationComponent;

		Node(avs::uid id, const std::string& name);

		virtual ~Node() = default;

		void SetStatic(bool s);
		bool IsStatic() const;
		void SetHolderClientId(avs::uid h);
		avs::uid GetHolderClientId() const;
		void SetPriority(int p);
		int GetPriority() const;
		void UpdateModelMatrix(vec3 translation, quat rotation, vec3 scale);
		// force update of model matrices - should not be necessary, but is.
		void UpdateModelMatrix();
		//Requests global transform of node, and node's children, be recalculated.
		void RequestTransformUpdate();

		void SetLastMovement(const teleport::core::MovementUpdate& update);
		//Updates the transform by extrapolating data from the last confirmed timestamp.
		void TickExtrapolatedTransform(double serverTimeS);

		void Update( float deltaTime);
		void UpdateExtrapolatedPositions(double serverTimeS);

		void SetParent(std::shared_ptr<Node> parent);
		std::weak_ptr<Node> GetParent() const { return parent; }
		bool HasAnyParent(std::shared_ptr<Node> node) const;

		void AddChild(std::shared_ptr<Node> child);
		void RemoveChild(std::shared_ptr<Node> child);
		void RemoveChild(avs::uid childID);
		bool HasChild(avs::uid(childID)) const;
		void ClearChildren();

		const std::vector<std::weak_ptr<Node>>& GetChildren() const { return children; }
		void SetChildrenIDs(const std::vector<avs::uid>& childrenIDs) { childIDs = childrenIDs; }
		const std::vector<avs::uid>& GetChildrenIDs() const { return childIDs; }

		bool IsVisible() const { return visibility.getVisibility(); }
		void SetVisible(bool visible);
		float GetTimeSinceLastVisible() const { return visibility.getTimeSinceLastVisible(); }

		virtual void SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
		std::shared_ptr<Mesh> GetMesh() const { return mesh; }

		void SetTextCanvas(std::shared_ptr<TextCanvas> t) { this->textCanvas = t; }
		std::shared_ptr<TextCanvas> GetTextCanvas() const { return textCanvas; }
		
		virtual void SetSkin(std::shared_ptr<Skin> skin) { skinInstance.reset(new SkinInstance(skin)); }
		const std::shared_ptr<SkinInstance> GetSkinInstance() const { return skinInstance; }
		std::shared_ptr<SkinInstance> GetSkinInstance() { return skinInstance; }

		virtual void SetMaterial(size_t index, std::shared_ptr<Material> material)
		{
			if(index >= materials.size() || index < 0)
			{
				TELEPORT_CERR << "Failed to set material at index " << index << "! Index not valid for material list of size " << materials.size() << " in node \"" << name.c_str() << "\"(Node_" << id << ")!\n";
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
				TELEPORT_CERR << "Failed to get material at index " << index << "! Index not valid for material list of size " << materials.size() << " in node \"" << name.c_str() << "\"(Node_" << id << ")!\n";
				return nullptr;
			}

			return materials[index];
		}

		virtual void SetMaterialListSize(size_t size);
		virtual void SetMaterialList(std::vector<std::shared_ptr<Material>>& materials);
		const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return materials; }
		size_t GetNumMaterials() const { return materials.size(); }

		void SetLocalTransform(const Transform& transform);
		const Transform& GetLocalTransform() const { return localTransform; }
		Transform& GetLocalTransform() { return localTransform; }
		
		//! Sets the global transform so it need not be calculated from local.
		void SetGlobalTransform(const Transform& g);
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

		void SetLocalPosition(const vec3& pos);
		const vec3& GetLocalPosition() const { return GetLocalTransform().m_Translation; }
		const vec3& GetGlobalPosition() const { return GetGlobalTransform().m_Translation; }
		const vec3& GetGlobalVelocity() const { return GetGlobalTransform().m_Velocity; }

		void SetLocalRotation(const clientrender::quat& rot);
		void SetLocalVelocity(const vec3& value);
		const clientrender::quat& GetLocalRotation() const { return GetLocalTransform().m_Rotation; }
		const clientrender::quat& GetGlobalRotation() const { return GetGlobalTransform().m_Rotation; }

		void SetLocalScale(const vec3& scale);
		const vec3& GetLocalScale() const { return GetLocalTransform().m_Scale; }
		const vec3& GetGlobalScale() const { return GetGlobalTransform().m_Scale; }

		virtual void SetHighlighted(bool highlighted);
		bool IsHighlighted() const { return isHighlighted; }

		void SetLightmapScaleOffset(const vec4& lso)
		{
			lightmapScaleOffset=lso;
		}
		const vec4 & GetLightmapScaleOffset() const
		{
			return lightmapScaleOffset;
		}
		avs::uid GetGlobalIlluminationTextureUid() const
		{
			return globalIlluminationTextureUid;
		}
		void SetGlobalIlluminationTextureUid(avs::uid uid)
		{
			globalIlluminationTextureUid=uid;
		}
	protected:
		avs::uid globalIlluminationTextureUid=0;
		std::shared_ptr<Mesh> mesh;
		std::shared_ptr<TextCanvas> textCanvas;
		std::shared_ptr<SkinInstance> skinInstance;
		std::vector<std::shared_ptr<Material>> materials;
		vec4 lightmapScaleOffset;
		Transform localTransform;
		Transform smoothedTransform;
		bool smoothingInitialized = false;

		std::vector<avs::uid> childIDs;

		bool isStatic;
		int priority=0;
		//Cached global transform, and dirty flag; updated when necessary on a request.
		mutable bool isTransformDirty = true;
		mutable Transform globalTransform;

		std::weak_ptr<Node> parent;
		std::vector<std::weak_ptr<Node>> children;
		std::mutex childrenMutex;

		teleport::core::MovementUpdate lastReceivedMovement;

		bool isHighlighted = false;

		void UpdateGlobalTransform() const;

		//Causes the global transforms off all children, and their children, to update whenever their global transform is requested.
		void RequestChildrenUpdateTransforms();

		//Whether we should use the global transform directly for update calculations.
		bool ShouldUseGlobalTransform() const;
		
		avs::uid holderClientId=0;
		bool isGrabbable=false;
	};
}