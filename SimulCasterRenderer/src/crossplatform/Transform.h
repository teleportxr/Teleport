#pragma once

#include "basic_linear_algebra.h"
#include "../../SimulCasterRenderer/src/api/RenderPlatform.h"

namespace scr
{

class Transform
{
public:
	struct TransformCreateInfo
	{
		const RenderPlatform* renderPlatform;
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
	Transform(const TransformCreateInfo& pTransformCreateInfo, scr::mat4 matrix);
	Transform(const avs::Transform& transform);

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

	void UpdateModelMatrix();
	void UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale);

	inline const mat4& GetTransformMatrix() const { return  m_TransformData.m_ModelMatrix; }
	inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }
};

}