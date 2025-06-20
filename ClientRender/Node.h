// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include "TeleportCore/CommonNetworking.h"
#include "libavstream/geometry/mesh_interface.hpp"

#include "TeleportClient/basic_linear_algebra.h"

#include "ClientRender/UniformBuffer.h"
#include "Material.h"
#include "Mesh.h"
#include "NodeComponents/AnimationComponent.h"
#include "NodeComponents/Component.h"
#include "NodeComponents/VisibilityComponent.h"
#include "ResourceManager.h"
#include "Skeleton.h"
#include "TextCanvas.h"
#include "Transform.h"

namespace teleport
{
	namespace clientrender
	{
		struct PassCache
		{
			uint64_t cachedEffectPassValidity = 0;
			platform::crossplatform::EffectPass *pass = nullptr;
			bool anim = false;
		};
		//! A renderable node in a hierarchy.
		class Node : public std::enable_shared_from_this<Node>, public IncompleteNode
		{
			std::vector<std::shared_ptr<Component>> components;

		public:
			const std::string name;

			// distance from the viewer - so we can sort nodes from front to back.
			float distance = 1.0f;
			mutable float countdown = 0.0f;
			void InitDigitizing()
			{
				countdown = 1.2f;
			}
			VisibilityComponent visibility;
			const std::vector<std::shared_ptr<Component>> &GetComponents() const
			{
				return components;
			}
			template <typename T>
			std::shared_ptr<T> GetComponent()
			{
				std::shared_ptr<T> u;
				for (auto c : components)
				{
					std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(c);
					if (t.get() != nullptr)
						return t;
				}
				return u;
			}
			template <typename T>
			std::shared_ptr<T> GetOrAddComponent()
			{
				std::shared_ptr<T> u;
				for (auto &c= components.begin();c!=components.end();c++)
				{
					std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(c);
					if (t.get() != nullptr)
						return t;
					c= std::make_shared<T>();
					return *c;
				}
				return AddComponent<T>();
			}
			template <typename T>
			std::shared_ptr<T> AddComponent()
			{
				auto u = std::make_shared<T>();
				components.push_back(u);
				return u;
			}
			Node(avs::uid id, const std::string &name);

			virtual ~Node() = default;
			static const char *getTypeName()
			{
				return "Node";
			}
			template <typename T>
			std::shared_ptr<T> GetOrCreateComponent()
			{
				for (auto c : components)
				{
					std::shared_ptr<T> t = std::dynamic_pointer_cast<T>(c);
					if (t.get() != nullptr)
						return t;
				}
				std::shared_ptr<T> u = std::make_shared<T>(*this);
				components.push_back(std::static_pointer_cast<Component>(u));
				return u;
			}
			template <typename T>
			std::shared_ptr<const T> GetComponent() const
			{
				for (auto c : components)
				{
					auto t = std::dynamic_pointer_cast<T>(c);
					if (t)
						return t;
				}
				std::shared_ptr<const T> u;
				return u;
			}
			void SetStatic(bool s);
			bool IsStatic() const;
			void SetHolderClientId(avs::uid h);
			avs::uid GetHolderClientId() const;
			void SetPriority(int p);
			int GetPriority() const;
			void UpdateModelMatrix(vec3 translation, quat rotation, vec3 scale);
			// force update of model matrices - should not be necessary, but is.
			void UpdateModelMatrix();
			// Requests global transform of node, and node's children, be recalculated.
			void RequestTransformUpdate();

			void SetLastMovement(const teleport::core::MovementUpdate &update);
			// Updates the transform by extrapolating data from the last confirmed timestamp.
			void TickExtrapolatedTransform(double serverTimeS);

			void Update(std::chrono::microseconds timestamp_us);
			void UpdateExtrapolatedPositions(double serverTimeS);

			void SetParent(std::shared_ptr<Node> parent);
			std::weak_ptr<Node> GetParent() const { return parent; }
			bool HasAnyParent(std::shared_ptr<Node> node) const;

			void AddChild(std::shared_ptr<Node> child);
			void RemoveChild(std::shared_ptr<Node> child);
			void RemoveChild(avs::uid childID);
			bool HasChild(avs::uid(childID)) const;
			void ClearChildren();

			const std::vector<std::weak_ptr<Node>> &GetChildren() const { return children; }

			const std::weak_ptr<const Node> GetSkeletonNode() const
			{
				return skeletonNode;
			}
			std::weak_ptr<Node> GetSkeletonNode();
			void SetSkeletonNode(std::weak_ptr<Node> n)
			{
				skeletonNode = n;
			}
			bool IsVisible() const { return visibility.getVisibility(); }
			void SetVisible(bool visible);
			float GetTimeSinceLastVisibleS(std::chrono::microseconds timestamp) const { return visibility.getTimeSinceLastVisibleS(timestamp.count()); }

			virtual void SetMesh(std::shared_ptr<Mesh> mesh);
			std::shared_ptr<Mesh> GetMesh() const;

			void SetTextCanvas(std::shared_ptr<TextCanvas> t) { this->textCanvas = t; }
			std::shared_ptr<TextCanvas> GetTextCanvas() const { return textCanvas; }

			virtual void SetSkeleton(std::shared_ptr<Skeleton> s) { skeleton=s; }
			const std::shared_ptr<Skeleton> GetSkeleton() const { return skeleton; }
			std::shared_ptr<Skeleton> GetSkeleton() { return skeleton; }

			void SetInverseBindMatrices(const std::vector<mat4> &ib)
			{
				inverseBindMatrices=ib;
			}
			const std::vector<mat4> &GetInverseBindMatrices() const
			{
				return inverseBindMatrices;
			}

			void SetJointIndices(const std::vector<int16_t> j)
			{
				jointIndices = j;
			}
			const std::vector<int16_t> &GetJointIndices() const
			{
				return jointIndices;
			}

			virtual void SetMaterial(size_t index, std::shared_ptr<Material> material)
			{
				if (index >= materials.size() || index < 0)
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
				if (index >= materials.size() || index < 0)
				{
					TELEPORT_CERR << "Failed to get material at index " << index << "! Index not valid for material list of size " << materials.size() << " in node \"" << name.c_str() << "\"(Node_" << id << ")!\n";
					return nullptr;
				}

				return materials[index];
			}
			uint64_t GetCachedEffectPassValidity(size_t submesh_index)
			{
				if (cachedEffectPasses.size() < materials.size())
					cachedEffectPasses.resize(materials.size());
				if (submesh_index >= cachedEffectPasses.size())
					return 0;
				return cachedEffectPasses[submesh_index].cachedEffectPassValidity;
			}
			void ResetCachedPass(size_t submesh_index)
			{
				SetCachedEffectPass(submesh_index, nullptr, false, 0);
			}
			void ResetCachedPasses()
			{
				for (size_t i = 0; i < cachedEffectPasses.size(); i++)
				{
					cachedEffectPasses[i].pass = 0;
					cachedEffectPasses[i].anim = false;
					cachedEffectPasses[i].cachedEffectPassValidity = 0;
				}
			}
			const PassCache *GetCachedEffectPass(size_t submesh_index) const
			{
				if (submesh_index >= cachedEffectPasses.size())
					return nullptr;
				return &(cachedEffectPasses[submesh_index]);
			}
			void SetCachedEffectPass(size_t submesh_index, platform::crossplatform::EffectPass *p, bool anim, uint64_t validity)
			{
				if (cachedEffectPasses.size() < materials.size())
					cachedEffectPasses.resize(materials.size());
				if (submesh_index >= cachedEffectPasses.size())
					return;
				cachedEffectPasses[submesh_index].pass = p;
				cachedEffectPasses[submesh_index].anim = anim;
				cachedEffectPasses[submesh_index].cachedEffectPassValidity = validity;
			}
			virtual void SetMaterialListSize(size_t size);
			virtual void SetMaterialList(std::vector<std::shared_ptr<Material>> &materials);
			const std::vector<std::shared_ptr<Material>> &GetMaterials() const { return materials; }
			size_t GetNumMaterials() const { return materials.size(); }

			void SetLocalTransform(const Transform &transform);
			const Transform &GetLocalTransform() const { return localTransform; }
			Transform &GetLocalTransform() { return localTransform; }

			//! Sets the global transform so it need not be calculated from local.
			void SetGlobalTransform(const Transform &g);
			const Transform &GetGlobalTransform() const
			{
				if (isTransformDirty)
				{
					UpdateGlobalTransform();
				}

				return globalTransform;
			}

			Transform &GetGlobalTransform()
			{
				if (isTransformDirty)
				{
					UpdateGlobalTransform();
				}

				return globalTransform;
			}

		
			void SetLocalPosition(const vec3 &pos);
			const vec3 &GetLocalPosition() const { return GetLocalTransform().m_Translation; }
			const vec3 &GetGlobalPosition() const { return GetGlobalTransform().m_Translation; }
			const vec3 &GetGlobalVelocity() const { return GetGlobalTransform().m_Velocity; }

			void SetLocalRotation(const clientrender::quat &rot);
			void SetLocalVelocity(const vec3 &value);
			const clientrender::quat &GetLocalRotation() const { return GetLocalTransform().m_Rotation; }
			const clientrender::quat &GetGlobalRotation() const { return GetGlobalTransform().m_Rotation; }

			void SetLocalScale(const vec3 &scale);
			const vec3 &GetLocalScale() const { return GetLocalTransform().m_Scale; }
			const vec3 &GetGlobalScale() const { return GetGlobalTransform().m_Scale; }

			virtual void SetHighlighted(bool highlighted);
			bool IsHighlighted() const { return isHighlighted; }

			void SetLightmapScaleOffset(const vec4 &lso)
			{
				lightmapScaleOffset = lso;
			}
			const vec4 &GetLightmapScaleOffset() const
			{
				return lightmapScaleOffset;
			}
			avs::uid GetGlobalIlluminationTextureUid() const
			{
				return globalIlluminationTextureUid;
			}
			void SetGlobalIlluminationTextureUid(avs::uid uid)
			{
				globalIlluminationTextureUid = uid;
			}
			void AddLink(const std::string &u)
			{
				url = u;
			}
			const std::string &GetURL() const
			{
				return url;
			}

		protected:
			std::string url;
			avs::uid globalIlluminationTextureUid = 0;
			std::shared_ptr<TextCanvas> textCanvas;
			std::shared_ptr<Skeleton> skeleton;
			std::vector<std::shared_ptr<Material>> materials;
			vec4 lightmapScaleOffset;
			Transform localTransform;
			Transform smoothedTransform;
			bool smoothingInitialized = false;
			bool smoothingEnabled = false;
			std::vector<mat4> inverseBindMatrices;
			bool isStatic;
			int priority = 0;
			// Cached global transform, and dirty flag; updated when necessary on a request.
			mutable bool isTransformDirty = true;
			mutable Transform globalTransform;

			std::weak_ptr<Node> parent;
			std::vector<std::weak_ptr<Node>> children;
			mutable std::mutex childrenMutex;

			teleport::core::MovementUpdate lastReceivedMovement;

			bool isHighlighted = false;

			void UpdateGlobalTransform() const;
			void RecursiveUpdateGlobalTransform(const Transform &parentGlobalTransform) const;

			// Causes the global transforms off all children, and their children, to update whenever their global transform is requested.
			void RequestChildrenUpdateTransforms();

			// Whether we should use the global transform directly for update calculations.
			bool ShouldUseGlobalTransform() const;

			avs::uid holderClientId = 0;
			bool isGrabbable = false;
			std::vector<PassCache> cachedEffectPasses;
			std::weak_ptr<Node> skeletonNode;
			std::vector<int16_t> jointIndices;
		};
	}
}