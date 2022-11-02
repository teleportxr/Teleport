#pragma once

#include "Platform/Shaders/SL/CppSl.sl"
#include "TeleportClient/basic_linear_algebra.h"

#include "ShaderResource.h"

namespace clientrender
{

	class Transform
	{
	public:
		avs::vec3 m_Translation={0,0,0};
		quat m_Rotation={0,0,0,0};
		avs::vec3 m_Scale={0,0,0};
		vec3 m_Velocity={0,0,0};
	private:
		mat4 m_ModelMatrix;
		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;
	public:
		Transform();
		Transform(avs::vec3 translation, quat rotation, avs::vec3 scale);
		Transform(mat4 matrix);
		Transform(const avs::Transform& transform);

		Transform& operator= (const avs::Transform& transform);
		Transform& operator= (const Transform& transform);

		Transform operator*(const Transform& other) const;
		avs::vec3 LocalToGlobal(const avs::vec3& local);
		void UpdateModelMatrix();
		bool UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);

		const mat4& GetTransformMatrix() const;

		Transform GetInverse() const;
	};

}