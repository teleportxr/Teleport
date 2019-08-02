// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "../Common.h"
#include "../api/FrameBuffer.h"
#include "../api/UniformBuffer.h"
#include "basic_linear_algebra.h"

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
		Type m_Type;
		struct LightData //Layout conformant to GLSL std140
		{
			vec4 m_Colour;
			vec3 m_Position;
			float m_Power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
			vec3 m_Direction;
			float m_SpotAngle;
		}m_LightData;

		UniformBuffer& m_UBO;
		//std::unique_ptr<FrameBuffer>m_ShadowMapFBO = nullptr; //TODO: Link up to FBO. 

	public:
		Light(Type type, const vec3& position, const vec3& direction, const vec4& colour, float power, float spotAngle, UniformBuffer& ubo);

		inline void UpdatePosition(const vec3& position) { m_LightData.m_Position = position; }
		inline void UpdateDirection(const vec3& direction) { m_LightData.m_Direction = direction; }
		inline void UpdateColour(const vec4& colour) { m_LightData.m_Colour = colour; }
		inline void UpdatePower(float power) { m_LightData.m_Power = power; }

		inline void UpdateLightData()
		{
			m_UBO.Update(0, sizeof(LightData), &m_LightData);
		}
	};
}