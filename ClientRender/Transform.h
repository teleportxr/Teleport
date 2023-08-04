#pragma once

#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "TeleportClient/basic_linear_algebra.h"
#include "Common.h"

namespace clientrender
{

	class Transform
	{
	public:
		vec3 m_Translation={0,0,0};
		quat m_Rotation={0,0,0,0};
		vec3 m_Scale={0,0,0};
		vec3 m_Velocity={0,0,0};
	private:
		mat4 m_ModelMatrix;
	public:
		Transform();
		Transform(vec3 translation, quat rotation, vec3 scale);
		Transform(mat4 matrix);
		Transform(const avs::Transform& transform);

		Transform& operator= (const avs::Transform& transform);
		Transform& operator= (const Transform& transform);

		Transform operator*(const Transform& other) const;
		vec3 LocalToGlobal(const vec3& local);
		void UpdateModelMatrix();
		bool UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale);

		const mat4& GetTransformMatrix() const;

		Transform GetInverse() const;
	};

}