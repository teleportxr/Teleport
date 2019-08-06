// (C) Copyright 2018-2019 Simul Software Ltd

#include "Camera.h"

using namespace scr;

bool Camera::s_UninitialisedUBO = false;

Camera::Camera(ProjectionType type, const vec3& position, const quat& orientation)
	:m_Type(type)
{
	if (s_UninitialisedUBO)
	{
		const float zero[sizeof(CameraData)] = { 0 };
		m_UBO->Create(sizeof(CameraData), zero, 0);
		s_UninitialisedUBO = true;
	}
	
	UpdatePosition(position);
	UpdateOrientation(orientation);
	UpdateView();

	m_SetLayout.AddBinding(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_VERTEX);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddBuffer(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, 0, { m_UBO.get(), 0, sizeof(CameraData) });
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
	if (m_Type != Camera::ProjectionType::PERSPECTIVE)
	{
		SCR_COUT("Invalid ProjectionType.");
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4::Perspective(horizontalFOV, aspectRatio, zNear, zFar);
}
void Camera::UpdateProjection(float left, float right, float bottom, float top, float near, float far)
{
	if (m_Type != Camera::ProjectionType::ORTHOGRAPHIC)
	{
		SCR_COUT("Invalid ProjectionType.");
		return;
	}
	m_CameraData.m_ProjectionMatrix = mat4::Orthographic(left, right, bottom, top, near, far);
}
void Camera::UpdateCameraUBO()
{
	m_UBO->Update(0, sizeof(CameraData), &m_CameraData);
}