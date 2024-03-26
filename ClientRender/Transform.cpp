#include "Transform.h"
#include "TeleportClient/Log.h" 
#include "TeleportCore/ErrorHandling.h"

using namespace teleport;
using namespace clientrender;

Transform::Transform()
	: Transform(vec3(), quat(0, 0, 0, 1.0f), vec3(1.0f, 1.0f, 1.0f))
{}

Transform::Transform(vec3 translation, quat rotation, vec3 scale)
	: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
{
	mat4 m = mat4::translation(translation) * mat4::rotation(*((vec4 *)&rotation)) ;
	mat4::mul(m_ModelMatrix, m, mat4::scale(scale));
	applyScale = (scale.x!= 1.f||scale.y!=1.f||scale.z!=1.f);
}

Transform::Transform(mat4 matrix)
{
	m_ModelMatrix = matrix;

	m_Translation = matrix.GetTranslation();
	//m_Rotation = matrix.GetRotation();
	m_Scale = matrix.GetScale();
	applyScale = (length(m_Scale) != 1.f);
}

Transform::Transform(const avs::Transform& transform)
	:m_Translation(transform.position), m_Rotation(transform.rotation), m_Scale(transform.scale)
{
	applyScale = (length(m_Scale) != 1.f);
	m_ModelMatrix = mat4::translation(m_Translation) * mat4::rotation(*((vec4 *)&m_Rotation)) * mat4::scale(m_Scale);
}

Transform& Transform::operator= (const avs::Transform& transform)
{
	m_Translation = transform.position;
	m_Rotation = transform.rotation;
	m_Scale = transform.scale;
	applyScale = (length(m_Scale) != 1.f);

	m_ModelMatrix = mat4::translation(m_Translation) * mat4::rotation(*((vec4 *)&m_Rotation)) * mat4::scale(m_Scale);

	return *this;
}

Transform& Transform::operator= (const Transform& transform)
{
	m_Translation = transform.m_Translation;
	m_Rotation = transform.m_Rotation;
	m_Scale = transform.m_Scale;
	applyScale = transform.applyScale;

	m_ModelMatrix = mat4::translation(m_Translation) * mat4::rotation(*((vec4 *)&m_Rotation)) * mat4::scale(m_Scale);

	return *this;
}

void Transform::Multiply(Transform &R, const Transform &A, const Transform &B)
{
	R.m_Scale = A.m_Scale * B.m_Scale;
	R.applyScale = (length(R.m_Scale) != 1.f);
	R.m_Rotation = B.m_Rotation * A.m_Rotation;
	R.m_Translation = B.m_Translation + B.m_Rotation.RotateVector(A.m_Translation * B.m_Scale);

	R.applyScale = A.applyScale||B.applyScale;
	if (R.applyScale)
		R.m_ModelMatrix.setRotationTranslationScale(*((vec4 *)&R.m_Rotation), R.m_Translation, R.m_Scale);
	else
		R.m_ModelMatrix.setRotationTranslation(*((vec4 *)&R.m_Rotation), R.m_Translation);
	//R.m_ModelMatrix.applyScale(R.m_Scale);
}

Transform Transform::operator*(const Transform& other) const
{
	vec3 scale(m_Scale.x * other.m_Scale.x, m_Scale.y * other.m_Scale.y, m_Scale.z * other.m_Scale.z);
	quat rotation = other.m_Rotation * m_Rotation;
	vec3 translation = other.m_Translation + other.m_Rotation.RotateVector(m_Translation * other.m_Scale);

	return Transform(translation, rotation, scale);
}

vec3 Transform::LocalToGlobal(const vec3& local)
{
	vec3 ret = m_Translation;
	ret+=m_Rotation.RotateVector(local);
	return ret;
}

void Transform::UpdateModelMatrix()
{
	m_ModelMatrix = mat4::translation(m_Translation) * mat4::rotation(*((vec4*)&m_Rotation)) * mat4::scale(m_Scale);
}

bool Transform::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
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
		applyScale = (length(m_Scale) != 1.f);
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