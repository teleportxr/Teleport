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
	applyScale = (m_Scale.x != 1.f || m_Scale.y != 1.0f || m_Scale.z != 1.0f);
}

Transform::Transform(const avs::Transform& transform)
	:m_Translation(transform.position), m_Rotation(transform.rotation), m_Scale(transform.scale)
{
	applyScale = (m_Scale.x != 1.f || m_Scale.y != 1.0f || m_Scale.z != 1.0f);
	m_ModelMatrix = mat4::translation(m_Translation) * mat4::rotation(*((vec4 *)&m_Rotation)) * mat4::scale(m_Scale);
}

Transform& Transform::operator= (const avs::Transform& transform)
{
	m_Translation = transform.position;
	m_Rotation = transform.rotation;
	m_Scale = transform.scale;
	applyScale = (m_Scale.x != 1.f || m_Scale.y != 1.0f || m_Scale.z != 1.0f);

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

// e.g. A is local, B is parent, R is global of A
void Transform::Multiply(Transform &R, const Transform &A, const Transform &B)
{
	R.m_ModelMatrix = B.m_ModelMatrix * A.m_ModelMatrix;
	R.MatrixToComponents();
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

void Transform::MatrixToComponents()
{
 // Extract translation
    m_Translation = m_ModelMatrix.GetTranslation();
  /*  
    // Extract scale and rotation
    // This requires a polar decomposition of the upper 3x3 matrix
    // into a rotation and a symmetric scale matrix
    
    // First, extract the 3x3 submatrix
    mat4 rotScale={0};
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            rotScale.m[i][j] = matrix.m[i][j];
        }
    }
    
    // Perform polar decomposition
    Matrix3x3 rotMat;
    Matrix3x3 scaleMat;
    PolarDecomposition(rotScale, &rotMat, &scaleMat);
    
    // Convert rotation matrix to quaternion
    *rotation = Quaternion::CreateFromRotationMatrix(rotMat);
    
    // Extract scale from the scale matrix
    // For a symmetric matrix, the eigenvalues are the scales
    // along the principal axes
    ExtractScaleFromSymmetricMatrix(scaleMat, scale);*/
}

bool Transform::UpdateModelMatrix(const vec3& translation, const quat& rotation, const vec3& scale)
{
	if (m_Translation != translation || m_Rotation != rotation || m_Scale != scale)
	{
		m_Translation = translation;
		m_Rotation = rotation;
		m_Scale = scale;
		applyScale = (m_Scale.x != 1.f || m_Scale.y != 1.0f || m_Scale.z != 1.0f);
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