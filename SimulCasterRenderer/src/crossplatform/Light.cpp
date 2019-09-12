// (C) Copyright 2018-2019 Simul Software Ltd

#include "Light.h"

using namespace scr;

uint32_t Light::s_NumOfLights = 0;
const uint32_t Light::s_MaxLights = 64;
bool Light::s_UninitialisedUB = false;

Light::Light(LightCreateInfo* pLightCreateInfo)
	:m_CI(*pLightCreateInfo)
{
	//m_LightData.resize(s_MaxLights);
	if (s_UninitialisedUB)
	{
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.bindingLocation = 2;
		ub_ci.size = sizeof(LightData);//s_MaxLights
		ub_ci.data = &m_LightData;

		m_UB->Create(&ub_ci);
		s_UninitialisedUB = true;
	}

	m_ShaderResourceLayout.AddBinding(2, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_FRAGMENT);

	m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
	m_ShaderResource.AddBuffer(0, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "u_LightsUB", { m_UB.get(), 0, (s_MaxLights * sizeof(LightData)) });

	assert(s_MaxLights > s_NumOfLights);
	m_LightID = s_NumOfLights;
	s_NumOfLights++;

	UpdatePosition(m_CI.position);
	UpdateDirection(m_CI.direction);
	UpdateColour(m_CI.colour);
	UpdatePower(m_CI.power);
	UpdateSpotAngle(m_CI.spotAngle);

	switch (m_CI.type)
	{
	case Light::Type::POINT:
		Point(); break;
	case Light::Type::DIRECTIONAL:
		Directional();
		break;
	case Light::Type::SPOT:
		Spot();
		break;
	case Light::Type::AREA:
		Area();
		break;
	default:
		break;
	}
}

void Light::Point()
{
	Camera::CameraCreateInfo c_ci;
	c_ci.type = Camera::ProjectionType::PERSPECTIVE; 
	c_ci.position = m_LightData.m_Position;
	c_ci.orientation = quat(0.0f, m_LightData.m_Direction);

	m_ShadowCamera = std::make_unique<Camera>(&c_ci);
	m_ShadowCamera->UpdateProjection(HALF_PI * 0.5f, 1.0f, 0.01f, 300.0f);

	CreateShadowMap();
}
void Light::Directional()
{
	Camera::CameraCreateInfo c_ci;
	c_ci.type = Camera::ProjectionType::ORTHOGRAPHIC;
	c_ci.position = m_LightData.m_Position;
	c_ci.orientation = quat(0.0f, m_LightData.m_Direction);

	m_ShadowCamera = std::make_unique<Camera>(&c_ci);
	m_ShadowCamera->UpdateProjection(0.0f, static_cast<float>(m_ShadowMapSize), 0.0f, static_cast<float>(m_ShadowMapSize), -1.0f, 1.0f);

	CreateShadowMap();
}
void Light::Spot()
{
	Camera::CameraCreateInfo c_ci;
	c_ci.type = Camera::ProjectionType::PERSPECTIVE;
	c_ci.position = m_LightData.m_Position;
	c_ci.orientation = quat(0.0f, m_LightData.m_Direction);

	m_ShadowCamera = std::make_unique<Camera>(&c_ci);
	m_ShadowCamera->UpdateProjection(m_LightData.m_SpotAngle, 1.0f, 0.01f, 300.0f);

	CreateShadowMap();
}
void Light::Area()
{
	//NULL
}

void Light::CreateShadowMap()
{
	FrameBuffer::FrameBufferCreateInfo fb_ci;
	fb_ci.format = Texture::Format::DEPTH_COMPONENT32;
	fb_ci.type = Texture::Type::TEXTURE_2D;
	fb_ci.sampleCount = Texture::SampleCountBit::SAMPLE_COUNT_1_BIT;
	fb_ci.width = m_ShadowMapSize;
	fb_ci.height = m_ShadowMapSize;
	m_ShadowMapFBO->Create(&fb_ci);
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
}
