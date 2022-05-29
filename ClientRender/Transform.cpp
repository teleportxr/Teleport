#include "Transform.h"
#include "TeleportClient/Log.h" 
#include "TeleportCore/ErrorHandling.h"

namespace clientrender
{
	Transform::Transform()
		: Transform(avs::vec3(), quat(0, 0, 0, 1.0f), avs::vec3(1.0f, 1.0f, 1.0f))
	{}

	Transform::Transform(avs::vec3 translation, quat rotation, avs::vec3 scale)
		: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
	{
		m_ShaderResourceLayout.AddBinding(1, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, ShaderStage::SHADER_STAGE_VERTEX);

		m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
		m_ShaderResource.AddBuffer(ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 1, "u_NodeUBO", { &m_TransformData, 0, sizeof(TransformData) });

		m_TransformData.m_ModelMatrix = mat4_deprecated::Translation(translation) * mat4_deprecated::Rotation(rotation) * mat4_deprecated::Scale(scale);
	}

	Transform::Transform(mat4 matrix)
	{
		m_TransformData.m_ModelMatrix = matrix;

		m_Translation = mat4_deprecated::GetTranslation(matrix);
		m_Rotation = mat4_deprecated::GetRotation(matrix);
		m_Scale = mat4_deprecated::GetScale(matrix);
	}

	Transform::Transform(const avs::Transform& transform)
		:m_Translation(transform.position), m_Rotation(transform.rotation), m_Scale(transform.scale)
	{
		m_TransformData.m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);
	}

	void Transform::UpdateModelMatrix()
	{
		m_TransformData.m_ModelMatrix = mat4_deprecated::Translation(m_Translation) * mat4_deprecated::Rotation(m_Rotation) * mat4_deprecated::Scale(m_Scale);
	}

	bool Transform::UpdateModelMatrix(const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
	{
		if (abs(scale.x) < 0.0001f)
		{
			TELEPORT_CERR << "Failed to update model matrix of transform! Scale.x is zero!\n";
			return false;
		}

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
}