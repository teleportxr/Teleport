// (C) Copyright 2018-2019 Simul Software Ltd

#include "Camera.h"

using namespace scr;

bool Camera::s_UninitialisedUB = false;

Camera::Camera(CameraCreateInfo* pCameraCreateInfo)
	:m_CI(*pCameraCreateInfo)
{
	if (s_UninitialisedUB)
	{
		const float zero[sizeof(CameraData)] = { 0 };

		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.bindingLocation = 0;
		ub_ci.size = sizeof(CameraData);
		ub_ci.data = zero;

		m_UB->Create(&ub_ci);
		s_UninitialisedUB = true;
	}
	
	UpdatePosition(m_CI.position);
	UpdateOrientation(m_CI.orientation);
	UpdateView();

	m_SetLayout.AddBinding(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_VERTEX);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddBuffer(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, 0, "u_CameraData", { m_UB, 0, sizeof(CameraData) });
}

void Camera::UpdatePosition(const vec3& position)
{
	m_CameraData.m_Position = position;
}
void Camera::UpdateOrientation(const quat& orientation)
{
	m_CameraData.m_Orientation = orientation;
}
void Camera::UpdateView()
{
	m_CameraData.m_ViewMatrix = mat4::Rotation(m_CameraData.m_Orientation) * mat4::Translation(m_CameraData.m_Position);
}
void Camera::UpdateProjection(float horizontalFOV, float aspectRatio, float zNear, float zFar)
{
	if (m_CI.type != Camera::ProjectionType::PERSPECTIVE)
	{
		SCR_COUT("Invalid ProjectionType.");
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4::Perspective(horizontalFOV, aspectRatio, zNear, zFar);
}
void Camera::UpdateProjection(float left, float right, float bottom, float top, float near, float far)
{
	if (m_CI.type != Camera::ProjectionType::ORTHOGRAPHIC)
	{
		SCR_COUT("Invalid ProjectionType.");
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4::Orthographic(left, right, bottom, top, near, far);
}
void Camera::UpdateCameraUBO()
{
	m_UB->Update(0, sizeof(CameraData), &m_CameraData);
}