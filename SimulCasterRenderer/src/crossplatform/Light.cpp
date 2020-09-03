// (C) Copyright 2018-2019 Simul Software Ltd

#include "Light.h"

using namespace scr;

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
		ub_ci.bindingLocation = 2;
		ub_ci.size = sizeof(LightData) * s_MaxLights;
		ub_ci.data = &s_LightData;

		s_UB = m_CI.renderPlatform->InstantiateUniformBuffer();
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
	s_LightData[m_LightID].colour = pLightCreateInfo->lightColour;
	s_LightData[m_LightID].power = 1.0f; //i.e 100W
	UpdateLightSpaceTransform();

	if (m_CI.shadowMapTexture)
	{
		m_ShadowMapSampler = m_CI.renderPlatform->InstantiateSampler();
		Sampler::SamplerCreateInfo sci;
		sci.minFilter = Sampler::Filter::LINEAR;
		sci.magFilter = Sampler::Filter::LINEAR;
		sci.wrapU = Sampler::Wrap::CLAMP_TO_EDGE;
		sci.wrapV = Sampler::Wrap::CLAMP_TO_EDGE;
		sci.wrapW = Sampler::Wrap::CLAMP_TO_EDGE;
		sci.minLod = 0;
		sci.maxLod = 0;
		sci.anisotropyEnable = false;
		sci.maxAnisotropy = 1.0f;
		m_ShadowMapSampler->Create(&sci);
	
		m_CI.shadowMapTexture->UseSampler(m_ShadowMapSampler);

		m_ShaderResourceLayout.AddBinding(2, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(19, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(20, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(21, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(22, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(23, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(24, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(25, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
		m_ShaderResourceLayout.AddBinding(26, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);

		m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
		m_ShaderResource.AddBuffer(0, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "u_LightsUB", { s_UB.get(), 0, (s_MaxLights * sizeof(LightData)) });
	
		std::string shaderResourceName = std::string("u_ShadowMap") + std::to_string(m_LightID);
		m_ShaderResource.AddImage(0, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 19 + (uint32_t)m_LightID, shaderResourceName.c_str(), { m_CI.shadowMapTexture->GetSampler(), m_CI.shadowMapTexture});
	}
}

void Light::UpdatePosition(const avs::vec3& position) 
{
	m_CI.position = position;
	UpdateLightSpaceTransform();
}
void Light::UpdateOrientation(const quat& orientation)
{ 
	m_CI.orientation = orientation;
	UpdateLightSpaceTransform();
}
const Light::LightData& Light::GetLightData() const 
{
	return s_LightData[m_LightID];
}
void Light::UpdateLightSpaceTransform()
{
	if (IsValid())
	{
		avs::vec3 defaultDirection = { 0.0f, 0.0f, -1.0f };

		s_LightData[m_LightID].position = m_CI.position;
		s_LightData[m_LightID].direction = ((m_CI.orientation * defaultDirection) * m_CI.orientation.Conjugate()).GetIJK(); //p = Im(q * p0 * q^-1)
		s_LightData[m_LightID].lightSpaceTransform = mat4::Translation(m_CI.position) * mat4::Rotation(m_CI.orientation);
	}
	else
		return;
}
