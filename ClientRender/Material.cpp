// (C) Copyright 2018-2019 Simul Software Ltd

#include "Material.h"
#include "RenderPlatform.h"

using namespace clientrender;

Material::Material(RenderPlatform* renderPlatform,const MaterialCreateInfo& pMaterialCreateInfo)
{
	SetMaterialCreateInfo(renderPlatform,pMaterialCreateInfo);
}

void Material::SetMaterialCreateInfo( RenderPlatform* renderPlatform,const MaterialCreateInfo& pMaterialCreateInfo)
{
	m_CI= pMaterialCreateInfo;
	m_MaterialData.diffuseOutputScalar			= m_CI.diffuse.textureOutputScalar;
	m_MaterialData.diffuseTexCoordsScalar_R		= m_CI.diffuse.texCoordsScalar[0];
	m_MaterialData.diffuseTexCoordsScalar_G		= m_CI.diffuse.texCoordsScalar[1];
	m_MaterialData.diffuseTexCoordsScalar_B		= m_CI.diffuse.texCoordsScalar[2];
	m_MaterialData.diffuseTexCoordsScalar_A		= m_CI.diffuse.texCoordsScalar[3];

	m_MaterialData.normalOutputScalar			= m_CI.normal.textureOutputScalar;
	m_MaterialData.normalTexCoordsScalar_R		= m_CI.normal.texCoordsScalar[0];
	m_MaterialData.normalTexCoordsScalar_G		= m_CI.normal.texCoordsScalar[1];
	m_MaterialData.normalTexCoordsScalar_B		= m_CI.normal.texCoordsScalar[2];
	m_MaterialData.normalTexCoordsScalar_A		= m_CI.normal.texCoordsScalar[3];

	m_MaterialData.combinedOutputScalarRoughMetalOcclusion			= m_CI.combined.textureOutputScalar;
	m_MaterialData.combinedTexCoordsScalar_R	= m_CI.combined.texCoordsScalar[0];
	m_MaterialData.combinedTexCoordsScalar_G	= m_CI.combined.texCoordsScalar[1];
	m_MaterialData.combinedTexCoordsScalar_B	= m_CI.combined.texCoordsScalar[2];
	m_MaterialData.combinedTexCoordsScalar_A	= m_CI.combined.texCoordsScalar[3];

	m_MaterialData.emissiveOutputScalar			= m_CI.emissive.textureOutputScalar;
	m_MaterialData.emissiveTexCoordsScalar_R	= m_CI.emissive.texCoordsScalar[0];
	m_MaterialData.emissiveTexCoordsScalar_G	= m_CI.emissive.texCoordsScalar[1];
	m_MaterialData.emissiveTexCoordsScalar_B	= m_CI.emissive.texCoordsScalar[2];
	m_MaterialData.emissiveTexCoordsScalar_A	= m_CI.emissive.texCoordsScalar[3];

	m_MaterialData.u_SpecularColour 			= avs::vec3(1, 1 ,1);

	m_MaterialData.u_DiffuseTexCoordIndex		= m_CI.diffuse.texCoordIndex;
	m_MaterialData.u_NormalTexCoordIndex		= m_CI.normal.texCoordIndex;
	m_MaterialData.u_CombinedTexCoordIndex		= m_CI.combined.texCoordIndex;
	m_MaterialData.u_EmissiveTexCoordIndex		= m_CI.emissive.texCoordIndex;

	//Set up UB
	if(!m_UB.get())
	{
		UniformBuffer::UniformBufferCreateInfo ub_ci;
		ub_ci.name="u_MaterialData";
		ub_ci.bindingLocation = 3;
		ub_ci.size = sizeof(MaterialData);
		ub_ci.data = &m_MaterialData;

		m_UB = renderPlatform->InstantiateUniformBuffer();
		m_UB->Create(&ub_ci);
	}
	else
	{
		m_UB->Update();
	}

	//Set up Descriptor Set for Textures and UB
	//UB from 0 - 9, Texture/Samplers 10+
	m_ShaderResourceLayout.AddBinding(2, ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_ShaderResourceLayout.AddBinding(10, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_ShaderResourceLayout.AddBinding(11, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_ShaderResourceLayout.AddBinding(12, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);
	m_ShaderResourceLayout.AddBinding(13, ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, Shader::Stage::SHADER_STAGE_FRAGMENT);

	m_ShaderResource = ShaderResource({ m_ShaderResourceLayout });
	m_ShaderResource.AddBuffer( ShaderResourceLayout::ShaderResourceType::UNIFORM_BUFFER, 2, "u_MaterialData", { m_UB.get(), 0, sizeof(MaterialData)});
	m_ShaderResource.AddImage( ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 10, "u_DiffuseTexture",  { m_CI.diffuse.texture ? m_CI.diffuse.texture->GetSampler() : nullptr, m_CI.diffuse.texture });
	m_ShaderResource.AddImage( ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 11, "u_NormalTexture",   { m_CI.normal.texture ? m_CI.normal.texture->GetSampler() : nullptr, m_CI.normal.texture });
	m_ShaderResource.AddImage( ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 12, "u_CombinedTexture", { m_CI.combined.texture ? m_CI.combined.texture->GetSampler() : nullptr, m_CI.combined.texture });
	m_ShaderResource.AddImage( ShaderResourceLayout::ShaderResourceType::COMBINED_IMAGE_SAMPLER, 13, "u_EmissiveTexture", {m_CI.emissive.texture ? m_CI.emissive.texture->GetSampler() : nullptr, m_CI.emissive.texture});
}