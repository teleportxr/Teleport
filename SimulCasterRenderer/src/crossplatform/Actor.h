// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "api/UniformBuffer.h"
#include "Material.h"
#include "Mesh.h"
#include "basic_linear_algebra.h"

namespace scr
{
	class Transform
	{
	private:
		vec3 m_Translation;
		quat m_Rotation;
		vec3 m_Scale;

		struct TransformData
		{
			mat4 m_ModelMatrix;
		} m_TransformData;
		static bool s_UninitialisedUB;
		std::unique_ptr<UniformBuffer> m_UB;

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;

	public:
		Transform();

		void UpdateModelUBO();
		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }
	};

	class Actor
	{
		struct ActorCreateInfo
		{
			bool staticMesh;	//Will the mesh move throughout the scence?
			bool animatedMesh;	//Will the mesh deform?
			Mesh* mesh;
			Material* material;
			Transform* transform;
		};

	private:
		ActorCreateInfo m_CI;

	public:
		Actor(ActorCreateInfo* pActorCreateInfo);

		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		inline const Mesh* GetMesh() const { return m_CI.mesh; }
		inline const Material* GetMaterial() const { return m_CI.material; }
		inline const Transform* GetTransform() const { return m_CI.transform; }
	};
}