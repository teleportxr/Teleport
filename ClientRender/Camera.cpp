// (C) Copyright 2018-2022 Simul Software Ltd

#include "Camera.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "ShaderResource.h"

using namespace clientrender;

// A couple of globals... wtf?
bool Camera::s_UninitialisedUB = true;
std::shared_ptr<UniformBuffer> Camera::s_UB = nullptr;

Camera::Camera(CameraCreateInfo* pCameraCreateInfo)
	:m_CI(*pCameraCreateInfo)
{
	if (s_UninitialisedUB)
	{
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.name="u_cameraData";
		ub_ci.bindingLocation = 0;
		ub_ci.size = sizeof(CameraData);
		ub_ci.data =  &m_CameraData;

		s_UB = std::make_shared<clientrender::UniformBuffer>(m_CI.renderPlatform);
		s_UB->Create(&ub_ci);
		s_UninitialisedUB = false;
	}
	
	UpdatePosition(m_CI.position);
	UpdateOrientation(m_CI.orientation);

	UpdateView();

	UpdateDrawDistance(m_CI.drawDistance);

	m_ShaderResourceLayout.AddBinding(0, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, ShaderStage::SHADER_STAGE_VERTEX);

	m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
	m_ShaderResource.AddBuffer( ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 0, "u_CameraData", { s_UB.get(), 0, sizeof(CameraData) });
}

void Camera::UpdatePosition(const avs::vec3& position)
{
	m_CameraData.m_Position = position;
}

void Camera::UpdateOrientation(const quat& orientation)
{
	m_CameraData.m_Orientation = orientation;
}

void Camera::UpdateDrawDistance(float distance)
{
	m_CameraData.m_DrawDistance = distance;
}

const ShaderResource& Camera::GetShaderResource() const
{
	return m_ShaderResource;
}

void Camera::UpdateView()
{
	//Inverse for a translation matrix is a -position input. Inverse for a rotation matrix is its transpose.
	mat4 invrot=mat4_deprecated::Rotation(m_CameraData.m_Orientation);
	invrot.transpose();
	m_CameraData.m_ViewMatrix = mat4_deprecated::Translation((m_CameraData.m_Position * -1)) * invrot;
}
void Camera::UpdateProjection(float horizontalFOV, float aspectRatio, float zNear, float zFar)
{
	if (m_CI.type != Camera::ProjectionType::PERSPECTIVE)
	{
		TELEPORT_CERR<<"Invalid ProjectionType.\n";
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4_deprecated::Perspective(horizontalFOV, aspectRatio, zNear, zFar);
}

void Camera::UpdateProjection(float left, float right, float bottom, float top, float nearf, float farf)
{
	if (m_CI.type != Camera::ProjectionType::ORTHOGRAPHIC)
	{
		TELEPORT_CERR<<"Invalid ProjectionType.\n";
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4_deprecated::Orthographic(left, right, bottom, top, nearf, farf);
}