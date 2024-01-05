// (C) Copyright 2018-2024 Simul Software Ltd

#include "Material.h"
#include "GeometryCache.h"

using namespace teleport;
using namespace clientrender;

Material::Material(const MaterialCreateInfo &pMaterialCreateInfo)
{
	SetMaterialCreateInfo(pMaterialCreateInfo);
	pbrMaterialConstants.RestoreDeviceObjects(GeometryCache::GetRenderPlatform());
}

Material::~Material()
{
	pbrMaterialConstants.InvalidateDeviceObjects();
}

void Material::SetMaterialCreateInfo( const MaterialCreateInfo& pMaterialCreateInfo)
{
	m_CI= pMaterialCreateInfo;
	m_MaterialData.diffuseOutputScalar			= m_CI.diffuse.textureOutputScalar;
	m_MaterialData.diffuseTexCoordsScale		= m_CI.diffuse.texCoordsScale;

	m_MaterialData.normalOutputScalar			= m_CI.normal.textureOutputScalar;
	m_MaterialData.normalTexCoordsScale			= m_CI.normal.texCoordsScale;

	m_MaterialData.combinedOutputScalarRoughMetalOcclusion			= m_CI.combined.textureOutputScalar;
	m_MaterialData.combinedTexCoordsScale		= m_CI.combined.texCoordsScale;

	m_MaterialData.emissiveOutputScalar			= m_CI.emissive.textureOutputScalar;
	m_MaterialData.emissiveTexCoordsScale		= m_CI.emissive.texCoordsScale;

	m_MaterialData.u_SpecularColour 			= vec3(1, 1 ,1);

	m_MaterialData.u_DiffuseTexCoordIndex		= m_CI.diffuse.texCoordIndex;
	m_MaterialData.u_NormalTexCoordIndex		= m_CI.normal.texCoordIndex;
	m_MaterialData.u_CombinedTexCoordIndex		= m_CI.combined.texCoordIndex;
	m_MaterialData.u_EmissiveTexCoordIndex		= m_CI.emissive.texCoordIndex;
	m_MaterialData.u_LightmapTexCoordIndex = m_CI.lightmapTexCoordIndex;
	const clientrender::Material::MaterialData &md = GetMaterialData();
	memcpy(&pbrMaterialConstants.diffuseOutputScalar, &md, sizeof(md));
	// Ensure that this is uploaded to GPU.
	pbrMaterialConstants.SetHasChanged();
}


void Material::SetShaderOverride(const char *c)
{
	m_CI.shader=c;
}
