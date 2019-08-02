// (C) Copyright 2018-2019 Simul Software Ltd

#include "Light.h"

using namespace scr;

Light::Light(Type type, const vec3& position, const vec3& direction, const vec4& colour, float power, float spotAngle, UniformBuffer& ubo)
	:m_Type(type), m_UBO(ubo)
{
	m_LightData.m_Position = position;
	m_LightData.m_Direction = direction;
	m_LightData.m_Colour = colour;
	m_LightData.m_Power = power;
	m_LightData.m_SpotAngle = spotAngle;

	switch (m_Type)
	{
	case Light::Type::POINT:
		break;
	case Light::Type::DIRECTIONAL:
		break;
	case Light::Type::SPOT:
		break;
	case Light::Type::AREA:
		break;
	default:
		break;
	}
}