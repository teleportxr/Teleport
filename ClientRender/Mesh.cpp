// (C) Copyright 2018-2022 Simul Software Ltd
#include "Mesh.h"

using namespace teleport;
using namespace clientrender;

Mesh::Mesh(const MeshCreateInfo& pMeshCreateInfo)
	:m_CI(pMeshCreateInfo)
{
	for (auto &m : m_CI.internalMaterials)
	{
		internalMaterials.push_back(std::make_shared<Material>(*m));
	}
}

Mesh::Mesh(const SubSceneCreate &subSceneCreate)
{
	m_CI.subscene_cache_uid = subSceneCreate.subscene_cache_uid;
}

Mesh::~Mesh()
{
	std::cerr << "Destroying mesh " << m_CI.name << "\n";
}