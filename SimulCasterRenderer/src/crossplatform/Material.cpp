// (C) Copyright 2018-2019 Simul Software Ltd

#include "Material.h"

using namespace scr;

Material::Material(Texture& diffuse, Texture& normal, Texture& combined)
	:m_Diffuse(diffuse), m_Normal(normal), m_Combined(combined)
{
	m_SetLayout.AddBinding(0, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_SetLayout.AddBinding(1, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_SetLayout.AddBinding(2, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);

	m_Set = DescriptorSet({ m_SetLayout });
	m_Set.AddImage(0, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, 0, "u_Diffuse",  { m_Diffuse.GetSampler(), &m_Diffuse });
	m_Set.AddImage(0, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, 1, "u_Normal",   { m_Normal.GetSampler(), &m_Normal });
	m_Set.AddImage(0, DescriptorSetLayout::DescriptorType::COMBINED_IMAGE_SAMPLER, 2, "u_Combined", { m_Combined.GetSampler(), &m_Combined });
}