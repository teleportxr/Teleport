// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"
#include "api/UniformBuffer.h"
#include "TeleportClient/basic_linear_algebra.h"
#include "ShaderResource.h"
#include "api/RenderPlatform.h"

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
			RenderPlatform* renderPlatform;
			ProjectionType type;
			quat orientation;
			avs::vec3 position;
			float drawDistance;
		};

	private:
		struct CameraData //Layout conformant to GLSL std140
		{
			mat4 m_ProjectionMatrix;
			mat4 m_ViewMatrix;
			quat m_Orientation;
			avs::vec3 m_Position;
			float m_DrawDistance;
		} m_CameraData;

		CameraCreateInfo m_CI;

		static bool s_UninitialisedUB;
		static std::shared_ptr<UniformBuffer> s_UB;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;

	public:
		Camera(CameraCreateInfo* pCameraCreateInfo);

		void UpdatePosition(const avs::vec3& position);
		void UpdateOrientation(const quat& orientation);
		void UpdateDrawDistance(float distance);
		void UpdateView();
		
		inline void UpdateView(const mat4& viewMatrix) { m_CameraData.m_ViewMatrix = viewMatrix; }

		void UpdateProjection(float horizontalFOV, float aspectRatio, float zNear, float zFar);
		void UpdateProjection(float left, float right, float bottom, float top, float near, float far);

		const ShaderResource& GetShaderResource() const;

		inline const avs::vec3& GetPosition() const { return m_CameraData.m_Position; }
	};
};