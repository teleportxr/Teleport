// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/UniformBuffer.h"
#include "Material.h"
#include "Mesh.h"
#include "basic_linear_algebra.h"
#include <libavstream/geometry/mesh_interface.hpp>

namespace scr
{
	class Transform
	{
	public:
		struct TransformCreateInfo
		{
			RenderPlatform* renderPlatform;
		};

		avs::vec3 m_Translation;
		quat m_Rotation;
		avs::vec3 m_Scale;

	private:
		struct TransformData
		{
			mat4 m_ModelMatrix;
		} m_TransformData;

		TransformCreateInfo m_CI;

		static bool s_UninitialisedUB;
		static std::shared_ptr<UniformBuffer> s_UB;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;

	public:
		Transform();
		Transform(const TransformCreateInfo& pTransformCreateInfo);
		Transform(const TransformCreateInfo& pTransformCreateInfo, avs::vec3 translation, quat rotation, avs::vec3 scale);

		Transform& operator= (const avs::Transform& transform)
		{
			m_Translation = transform.position;
			m_Rotation = transform.rotation;
			m_Scale = transform.scale;

			m_TransformData.m_ModelMatrix = mat4::Translation(m_Translation) * mat4::Rotation(m_Rotation) * mat4::Scale(m_Scale);

			return *this;
		}

		Transform operator*(const Transform& other) const
		{
			avs::vec3 globalScale(m_Scale.x * other.m_Scale.x, m_Scale.y * other.m_Scale.y, m_Scale.z * other.m_Scale.z);
			quat globalRotation = other.m_Rotation * m_Rotation;
			avs::vec3 globalPosition = other.m_Translation + other.m_Rotation.RotateVector(m_Translation);

			return Transform(m_CI, globalPosition, globalRotation, globalScale);
		}

		void UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);

		inline const mat4& GetTransformMatrix() const { return  m_TransformData.m_ModelMatrix; }
		inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }
	};

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

	private:
		ActorCreateInfo m_CI;

		//Cached global transform, and dirty flag; updated when necessary on a request.
		mutable bool isTransformDirty = true;
		mutable Transform globalTransform;

		std::weak_ptr<Actor> parent;
		std::vector<std::weak_ptr<Actor>> children;

		avs::MovementUpdate lastReceivedMovement;
	public:
		Actor(const ActorCreateInfo &pActorCreateInfo);

		void UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);
		//Requests global transform of actor, and actor's children, be recalculated.
		void RequestTransformUpdate();

		void SetLastMovement(const avs::MovementUpdate& update);
		//Updates the transform by extrapolating data from the last confirmed timestamp.
		void TickExtrapolatedTransform(float deltaTime);

		void SetParent(std::weak_ptr<Actor> parent);
		void AddChild(std::weak_ptr<Actor> child);

		inline std::shared_ptr<Mesh> GetMesh() const { return m_CI.mesh; }
		inline const std::vector<std::shared_ptr<Material>> GetMaterials() const { return m_CI.materials; }

		inline const Transform& GetLocalTransform() const { return m_CI.localTransform; } const
		inline Transform& GetLocalTransform() { return m_CI.localTransform; }

		inline const Transform& GetGlobalTransform() const
		{ 
			if(isTransformDirty) UpdateGlobalTransform();
			return globalTransform; 
		}

		inline Transform& GetGlobalTransform()
		{
			if(isTransformDirty) UpdateGlobalTransform();
			return globalTransform;
		}

	private:
		void UpdateGlobalTransform() const;
	};
}