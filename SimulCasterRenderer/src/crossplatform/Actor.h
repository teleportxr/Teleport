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
		vec3 m_Translation;
		quat m_Rotation;
		vec3 m_Scale;

	private:
		struct TransformData
		{
			mat4 m_ModelMatrix;
		} m_TransformData;
		static bool s_UninitialisedUB;
		std::unique_ptr<UniformBuffer> m_UB;

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;
	
	public:
		Transform& operator= (avs::Transform& transform)
		{
			m_Translation.x = transform.position.x;
			m_Translation.y = transform.position.y;
			m_Translation.z = transform.position.z;

			m_Rotation.s = transform.rotation.w;
			m_Rotation.i = transform.rotation.x;
			m_Rotation.j = transform.rotation.y;
			m_Rotation.k = transform.rotation.z;

			m_Scale.x = transform.scale.x;
			m_Scale.y = transform.scale.y;
			m_Scale.z = transform.scale.z;
			
			return *this;
		}

	public:
		Transform();

		void UpdateModelUBO() const;
		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		inline const mat4& GetTransformMatrix() const { return  m_TransformData.m_ModelMatrix; }
		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }
	};

	class Actor
	{
	public:
		struct ActorCreateInfo
		{
			bool staticMesh;	//Will the mesh move throughout the scene?
			bool animatedMesh;	//Will the mesh deform?
			Mesh* mesh;
			std::vector<Material*> materials;
			Transform* transform;
		};

	private:
		ActorCreateInfo m_CI;

	public:
		Actor(ActorCreateInfo* pActorCreateInfo);

		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		inline Mesh* GetMesh() { return m_CI.mesh; }
		inline const std::vector<Material*> GetMaterials() { return m_CI.materials; }
		inline Transform* GetTransform() { return m_CI.transform; }
	};
}