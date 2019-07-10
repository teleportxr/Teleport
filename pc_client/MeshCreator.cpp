#include "MeshCreator.h"

using namespace avs;

unsigned int MeshCreator::s_TotalSubMeshes = 0;

MeshCreator::MeshCreator(simul::crossplatform::RenderPlatform* renderPlatform)
	:m_RP(renderPlatform)
{
	m_Mesh = std::make_unique<simul::crossplatform::Mesh>();
	//m_Material;
}

MeshCreator::~MeshCreator()
{
	m_RP = nullptr;
	m_Mesh = nullptr;
}

void MeshCreator::ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3* vertices)
{
	if (!SetAndCheckShapeUID(shape_uid))
	{
		SIMUL_BREAK_ONCE("shape_uid is already set or is incorrect.\n");
		return;
	}

	numVertices = vertexCount * 3;
	memcpy((void*)lVertices, vertices, numVertices);

	lPolygonCount = numVertices;
}

void MeshCreator::ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3* faces)
{
	if (!SetAndCheckShapeUID(shape_uid))
	{
		SIMUL_BREAK_ONCE("shape_uid is already set or is incorrect.\n");
		return;
	}
	//?
}

void MeshCreator::ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, const unsigned int* indices)
{
	if (!SetAndCheckShapeUID(shape_uid))
	{
		SIMUL_BREAK_ONCE("shape_uid is already set or is incorrect.\n");
		return;
	}

	numIndices = indexCount;
	memcpy((void*)lIndices, (void*)indices, numIndices);

	if (indexCount % 3 > 0)
	{
		SIMUL_BREAK_ONCE("indexCount is not a multiple of 3.\n");
		return;
	}
	lPolygonCount = indexCount / 3;

	m_Mesh->SetSubMesh(s_TotalSubMeshes, startIndex, indexCount, m_Material);

}

void MeshCreator::Create()
{
	if (m_RP == nullptr || lVertices == nullptr || lIndices == nullptr
		|| lPolygonVertexCount == 0 || lPolygonCount == 0)
	{
		SIMUL_COUT << "Insufficient data to create mesh.\n";
		return;
	}

	m_Mesh->Initialize(m_RP, lPolygonVertexCount, lVertices, lNormals, lUVs, lPolygonCount, lIndices, nullptr);
}

void MeshCreator::BeginDraw(simul::crossplatform::DeviceContext& deviceContext, simul::crossplatform::ShadingMode shadingMode)
{
	m_ShadingMode = shadingMode;
	m_Mesh->BeginDraw(deviceContext, m_ShadingMode);
}

void MeshCreator::Draw(simul::crossplatform::DeviceContext& deviceContext, int meshIndex)
{
	m_Mesh->Draw(deviceContext, meshIndex);
}

void MeshCreator::EndDraw(simul::crossplatform::DeviceContext& deviceContext)
{
	m_Mesh->EndDraw(deviceContext);
}

void MeshCreator::SetMaterialForSubMesh(int meshIndex, simul::crossplatform::Material* material)
{
	simul::crossplatform::Mesh::SubMesh* sm = m_Mesh->GetSubMesh(meshIndex);

	if (sm)
		sm->material = material;
	else
	{
		SIMUL_COUT << "Invalid meshIndex.\n";
		return;
	}
}
