// (C) Copyright 2018-2022 Simul Software Ltd

#include "Light.h"

using namespace clientrender;

const uint32_t Light::s_MaxLights = 8; 
std::vector<Light::LightData> Light::s_LightData = {};

bool Light::s_UninitialisedUB = true;
std::shared_ptr<UniformBuffer> Light::s_UB = nullptr;

Light::Light(LightCreateInfo* pLightCreateInfo)
	:m_CI(*pLightCreateInfo)
{
	if (s_UninitialisedUB)
	{
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.name="u_lightData";
		ub_ci.bindingLocation = 2;
		ub_ci.size = sizeof(LightData) * s_MaxLights;
		ub_ci.data = &s_LightData;

		s_UB = std::make_shared<clientrender::UniformBuffer>(m_CI.renderPlatform);
		s_UB->Create(&ub_ci);
		s_UninitialisedUB = false;
	}

	if(!(s_MaxLights >= s_LightData.size() + 1))
	{
		SCR_LOG("Exceeded maximum number of lights(%d). No LightData will be created.", s_MaxLights);
		return;
	}
	m_IsValid = true;

	s_LightData.push_back({});
	m_LightID = s_LightData.size() - 1;

	//Default values
	auto &light=s_LightData[m_LightID];
	light.colour = pLightCreateInfo->lightColour;
	light.radius=pLightCreateInfo->lightRadius;
	light.range=pLightCreateInfo->lightRange;
	light.power = 1.0f; //i.e 100W
	light.is_point=float(pLightCreateInfo->type!=Type::DIRECTIONAL);
	light.is_spot=float(pLightCreateInfo->type==Type::SPOT);
	// Low 32 bits of the uid for matching.
	light.uid32=(unsigned)(m_CI.uid&(uint64_t)0xFFFFFFFF);
	UpdateLightSpaceTransform();

	if (m_CI.shadowMapTexture)
	{
	
		m_ShaderResourceLayout.AddBinding(2,  ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(19, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(20, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(21, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(22, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(23, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(24, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(25, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(26, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, ShaderStage::SHADER_STAGE_FRAGMENT);

		m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
		m_ShaderResource.AddBuffer( ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "u_LightsUB", { s_UB.get(), 0, (s_MaxLights * sizeof(LightData)) });
	
		std::string shaderResourceName = std::string("u_ShadowMap") + std::to_string(m_LightID);
		m_ShaderResource.AddImage( ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 19 + (uint32_t)m_LightID, shaderResourceName.c_str(), {  m_CI.shadowMapTexture});
	}
}

void Light::UpdatePosition(const vec3& position) 
{
	m_CI.position = position;
	UpdateLightSpaceTransform();
}

void Light::UpdateOrientation(const quat& orientation)
{ 
	m_CI.orientation = orientation;
	UpdateLightSpaceTransform();
}

void Light::UpdateLightSpaceTransform()
{
	if (IsValid())
	{
		auto &light=s_LightData[m_LightID];
		// We consider lights to shine in the Z direction.
		//vec3 defaultDirection(0,0,1.0f);
		light.position = m_CI.position;
		light.direction = ((m_CI.orientation * m_CI.direction) * m_CI.orientation.Conjugate()).GetIJK(); //p = Im(q * p0 * q^-1)
		vec3 scale(m_CI.lightRadius,m_CI.lightRadius,m_CI.lightRadius);
		light.lightSpaceTransform = mat4_deprecated::Translation(m_CI.position) * mat4_deprecated::Rotation(m_CI.orientation)*mat4_deprecated::Scale(scale);
	}
}
