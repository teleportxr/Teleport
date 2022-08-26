#pragma once

#include "TeleportClient/basic_linear_algebra.h"

#include "ShaderResource.h"

namespace clientrender
{

	class Transform
	{
	public:
		avs::vec3 m_Translation;
		quat m_Rotation;
		avs::vec3 m_Scale;

	private:
		struct TransformData
		{
			mat4 m_ModelMatrix;
		} m_TransformData;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;

	public:
		Transform();
		Transform(avs::vec3 translation, quat rotation, avs::vec3 scale);
		Transform(mat4 matrix);
		Transform(const avs::Transform& transform);

		Transform& operator= (const avs::Transform& transform)
		{
			m_Translation = transform.position;
			m_Rotation = transform.rotation;
			m_Scale = transform.scale;

			m_TransformData.m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);

			return *this;
		}

		Transform operator*(const Transform& other) const
		{
			avs::vec3 scale(m_Scale.x * other.m_Scale.x, m_Scale.y * other.m_Scale.y, m_Scale.z * other.m_Scale.z);
			quat rotation = other.m_Rotation * m_Rotation;
			avs::vec3 translation = other.m_Translation + other.m_Rotation.RotateVector(m_Translation * other.m_Scale.GetAbsolute());

			return Transform(translation, rotation, scale);
		}
		avs::vec3 LocalToGlobal(const avs::vec3& local)
		{
			avs::vec3 ret = m_Translation;
			ret+=m_Rotation.RotateVector(local);
			return ret;
		}
		void UpdateModelMatrix();
		bool UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);

		inline const mat4& GetTransformMatrix() const { return  m_TransformData.m_ModelMatrix; }
		inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }

		Transform GetInverse() const;
	};

}