// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

#include "API.h"
#include "Camera.h"

#include "api/FrameBuffer.h"
#include "api/UniformBuffer.h"

namespace scr
{
	class Light
	{
	public:
		enum class Type : uint32_t
		{
			POINT,
			DIRECTIONAL,
			SPOT,
			AREA
		};
	
	private:

		static uint32_t s_NumOfLights;
		const static uint32_t s_MaxLights;
		uint32_t m_LightID;

		Type m_Type;
		struct LightData //Layout conformant to GLSL std140
		{
			vec4 m_Colour;
			vec3 m_Position;
			float m_Power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
			vec3 m_Direction;
			float m_SpotAngle;
		}m_LightData;
		
		static bool s_UninitialisedUBO;
		std::unique_ptr<UniformBuffer> m_UBO;

		DescriptorSetLayout m_SetLayout;
		DescriptorSet m_Set;
		
		uint32_t m_ShadowMapSize = 256;
		std::unique_ptr<FrameBuffer>m_ShadowMapFBO = nullptr; 
		std::unique_ptr<Camera> m_ShadowCamera = nullptr;

	public:
		Light(Type type, const vec3& position, const vec3& direction, const vec4& colour, float power, float spotAngle);

		void UpdatePosition(const vec3& position);
		void UpdateDirection(const vec3& direction);
		void UpdateColour(const vec4& colour);
		void UpdatePower(float power);
		void UpdateSpotAngle(float spotAngle);

		void UpdateLightUBO();

		inline const DescriptorSet& GetDescriptorSet() const { return m_Set; }

		inline std::unique_ptr<FrameBuffer>& GetShadowMapFBO() { return m_ShadowMapFBO; }

	private:
		void Point();
		void Directional();
		void Spot();
		void Area();
	};
}