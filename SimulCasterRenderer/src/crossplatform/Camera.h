// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"
#include "api/UniformBuffer.h"
#include "basic_linear_algebra.h"
#include "DescriptorSet.h"

namespace scr
{
	class Camera
	{
	public:
		enum class ProjectionType : uint32_t
		{
			ORTHOGRAPHIC,
			PERSPECTIVE
		};
		struct CameraCreateInfo
		{
			ProjectionType type;
			quat orientation;
			vec3 position;
		};

	private:
		struct CameraData //Layout conformant to GLSL std140
		{
			mat4 m_ProjectionMatrix;
			mat4 m_ViewMatrix;
			quat m_Orientation;
			vec3 m_Position;
			float _pad;
		} m_CameraData;

		CameraCreateInfo m_CI;

		static bool s_UninitialisedUB;
		std::shared_ptr<UniformBuffer> m_UB;

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;

	public:
		Camera(CameraCreateInfo* pCameraCreateInfo);

		void UpdatePosition(const vec3& position);
		void UpdateOrientation(const quat& orientation);
		void UpdateView();
		
		inline void UpdateView(const mat4& viewMatrix) { m_CameraData.m_ViewMatrix = viewMatrix; }

		void UpdateProjection(float horizontalFOV, float aspectRatio, float zNear, float zFar);
		void UpdateProjection(float left, float right, float bottom, float top, float near, float far);

		void UpdateCameraUBO();
		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }

		inline const vec3& GetPosition() const { return m_CameraData.m_Position; }
	};
};