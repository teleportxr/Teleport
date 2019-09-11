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

		vec3 m_Translation;
		quat m_Rotation;
		vec3 m_Scale;

	private:
		struct TransformData
		{
			mat4 m_ModelMatrix;
		} m_TransformData;

		TransformCreateInfo m_CI;

		static bool s_UninitialisedUB;
		std::shared_ptr<UniformBuffer> m_UB;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;
	
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
		Transform(TransformCreateInfo* pTransformCreateInfo);

		void UpdateModelUBO() const;
		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

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
			std::shared_ptr<Transform> transform;
		};

	private:
		ActorCreateInfo m_CI;

	public:
		Actor(ActorCreateInfo* pActorCreateInfo);

		void UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);
		bool IsComplete() const;

		inline std::shared_ptr<Mesh> GetMesh() { return m_CI.mesh; }
		inline const std::vector<std::shared_ptr<Material>> GetMaterials() const { return m_CI.materials; }
		inline std::shared_ptr<Transform> GetTransform() { return m_CI.transform; }
	};
}