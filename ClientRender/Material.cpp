// (C) Copyright 2018-2022 Simul Software Ltd

#include "Material.h"

using namespace clientrender;

Material::Material(platform::crossplatform::RenderPlatform* r,const MaterialCreateInfo& pMaterialCreateInfo)
{
	SetMaterialCreateInfo(r,pMaterialCreateInfo);
}

void Material::SetMaterialCreateInfo( platform::crossplatform::RenderPlatform* r,const MaterialCreateInfo& pMaterialCreateInfo)
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

	m_MaterialData.u_SpecularColour 			= vec3(1, 1 ,1);

	m_MaterialData.u_DiffuseTexCoordIndex		= m_CI.diffuse.texCoordIndex;
	m_MaterialData.u_NormalTexCoordIndex		= m_CI.normal.texCoordIndex;
	m_MaterialData.u_CombinedTexCoordIndex		= m_CI.combined.texCoordIndex;
	m_MaterialData.u_EmissiveTexCoordIndex		= m_CI.emissive.texCoordIndex;
}


void Material::SetShaderOverride(const char *c)
{
	m_CI.shader=c;
}