#include "Transform.h"
#include "TeleportClient/Log.h" 
#include "TeleportCore/ErrorHandling.h"

using namespace clientrender;

Transform::Transform()
	: Transform(avs::vec3(), quat(0, 0, 0, 1.0f), avs::vec3(1.0f, 1.0f, 1.0f))
{}

Transform::Transform(avs::vec3 translation, quat rotation, avs::vec3 scale)
	: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	m_ShaderResourceLayout.AddBinding(1, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, ShaderStage::SHADER_STAGE_VERTEX);

	m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
	m_ShaderResource.AddBuffer(ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 1, "u_NodeUBO", { &m_ModelMatrix, 0, sizeof(m_ModelMatrix) });

	m_ModelMatrix = mat4_deprecated::Translation(translation) * mat4_deprecated::Rotation(rotation) * mat4_deprecated::Scale(scale);
}

Transform::Transform(mat4 matrix)
{
	m_ModelMatrix = matrix;

	m_Translation = mat4_deprecated::GetTranslation(matrix);
	m_Rotation = mat4_deprecated::GetRotation(matrix);
	m_Scale = mat4_deprecated::GetScale(matrix);
}

Transform::Transform(const avs::Transform& transform)
	:m_Translation(transform.position), m_Rotation(transform.rotation), m_Scale(transform.scale)
{
	m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);
}

Transform& Transform::operator= (const avs::Transform& transform)
{
	m_Translation = transform.position;
	m_Rotation = transform.rotation;
	m_Scale = transform.scale;

	m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);

	return *this;
}

Transform& Transform::operator= (const Transform& transform)
{
	m_Translation = transform.m_Translation;
	m_Rotation = transform.m_Rotation;
	m_Scale = transform.m_Scale;

	m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);

	return *this;
}

Transform Transform::operator*(const Transform& other) const
{
	avs::vec3 scale(m_Scale.x * other.m_Scale.x, m_Scale.y * other.m_Scale.y, m_Scale.z * other.m_Scale.z);
	quat rotation = other.m_Rotation * m_Rotation;
	avs::vec3 translation = other.m_Translation + other.m_Rotation.RotateVector(m_Translation * abs(other.m_Scale));

	return Transform(translation, rotation, scale);
}

avs::vec3 Transform::LocalToGlobal(const avs::vec3& local)
{
	avs::vec3 ret = m_Translation;
	ret+=m_Rotation.RotateVector(local);
	return ret;
}

void Transform::UpdateModelMatrix()
{
	m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);
}

bool Transform::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	// zero scale is valid.
	/*if (abs(scale.x) < 0.0001f)
	{
		TELEPORT_CERR << "Failed to update model matrix of transform! Scale.x is zero!\n";
		return false;
	}*/

	if (m_Translation != translation || m_Rotation != rotation || m_Scale != scale)
	{
		m_Translation = translation;
		m_Rotation = rotation;
		m_Scale = scale;
		UpdateModelMatrix();

		return true;
	}
	return false;
}

const mat4& Transform::GetTransformMatrix() const
{
	return  m_ModelMatrix;
}

Transform Transform::GetInverse() const
{
	Transform I(mat4::inverse(m_ModelMatrix));
	return I;
}