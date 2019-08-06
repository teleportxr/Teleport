// (C) Copyright 2018-2019 Simul Software Ltd

#include "Light.h"

using namespace scr;

uint32_t Light::s_NumOfLights = 0;
const uint32_t Light::s_MaxLights = 64;
bool Light::s_UninitialisedUBO = false;

Light::Light(Type type, const vec3& position, const vec3& direction, const vec4& colour, float power, float spotAngle)
	:m_Type(type)
{
	if (s_UninitialisedUBO)
	{
		const float zero[s_MaxLights * sizeof(LightData)] = { 0 };
		m_UBO->Create(s_MaxLights * sizeof(LightData), zero, 2);
		s_UninitialisedUBO = true;
	}

	m_SetLayout.AddBinding(2, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_FRAGMENT);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddBuffer(0, DescriptorSetLayout::DescriptorType::UNIFORM_BUFFER, 2, { m_UBO.get(), 0, (s_MaxLights * sizeof(LightData)) });

	assert(s_MaxLights > s_NumOfLights);
	m_LightID = s_NumOfLights;
	s_NumOfLights++;

	UpdatePosition(position);
	UpdateDirection(direction);
	UpdateColour(colour);
	UpdatePower(power);
	UpdateSpotAngle(spotAngle);

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

void Light::Point()
{
	m_ShadowCamera = std::make_unique<Camera>(Camera::ProjectionType::PERSPECTIVE, m_LightData.m_Position, quat(0.0f, m_LightData.m_Direction));
	m_ShadowCamera->UpdateProjection(HALF_PI * 0.5f, 1.0f, 0.01f, 300.0f);

	m_ShadowMapFBO->Create(Texture::Format::DEPTH_COMPONENT32, Texture::SampleCount::SAMPLE_COUNT_1_BIT, m_ShadowMapSize, m_ShadowMapSize);
}
void Light::Directional()
{
	m_ShadowCamera = std::make_unique<Camera>(Camera::ProjectionType::ORTHOGRAPHIC, m_LightData.m_Position, quat(0.0f, m_LightData.m_Direction));
	m_ShadowCamera->UpdateProjection(0.0f, static_cast<float>(m_ShadowMapSize), 0.0f, static_cast<float>(m_ShadowMapSize), -1.0f, 1.0f);

	m_ShadowMapFBO->Create(Texture::Format::DEPTH_COMPONENT32, Texture::SampleCount::SAMPLE_COUNT_1_BIT, m_ShadowMapSize, m_ShadowMapSize);
}
void Light::Spot()
{
	m_ShadowCamera = std::make_unique<Camera>(Camera::ProjectionType::PERSPECTIVE, m_LightData.m_Position, quat(0.0f, m_LightData.m_Direction));
	m_ShadowCamera->UpdateProjection(m_LightData.m_SpotAngle, 1.0f, 0.01f, 300.0f);

	m_ShadowMapFBO->Create(Texture::Format::DEPTH_COMPONENT32, Texture::SampleCount::SAMPLE_COUNT_1_BIT, m_ShadowMapSize, m_ShadowMapSize);
}
void Light::Area()
{
	//NULL
}

void Light::UpdatePosition(const vec3& position) 
{
	m_LightData.m_Position = position; 
}
void Light::UpdateDirection(const vec3& direction)
{ 
	m_LightData.m_Direction = direction; 
}
void Light::UpdateColour(const vec4& colour) 
{
	m_LightData.m_Colour = colour; 
}
void Light::UpdatePower(float power) 
{ 
	m_LightData.m_Power = power; 
}
void Light::UpdateSpotAngle(float spotAngle) 
{ 
	m_LightData.m_Power = spotAngle; 
}
void Light::UpdateLightUBO()
{
	m_UBO->Update(m_LightID * sizeof(LightData), sizeof(LightData), &m_LightData);
}
