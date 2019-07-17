#include "MeshCreator.h"

using namespace avs;

unsigned int MeshCreator::s_TotalSubMeshes = 0;

MeshCreator::MeshCreator()
{
	m_Mesh = std::make_unique<simul::crossplatform::Mesh>();
}

MeshCreator::~MeshCreator()
{
	m_Mesh = nullptr;
}

void MeshCreator::ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3* vertices)
{
	CHECK_SHAPE_UID(shape_uid);

	m_VertexCount = vertexCount;
	size_t bufferSize = m_VertexCount * sizeof(avs::vec3);
	m_Vertices = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Vertices.get(), vertices, bufferSize);
}

void MeshCreator::ensureNormals(unsigned long long shape_uid, int startNormal, int normalCount, const avs::vec3* normals)
{
	CHECK_SHAPE_UID(shape_uid);
	if (normalCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec3);
	m_Normals = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Normals.get(), normals, bufferSize);
}

void MeshCreator::ensureTangents(unsigned long long shape_uid, int startTangent, int tangentCount, const avs::vec4* tangents)
{
	CHECK_SHAPE_UID(shape_uid);
	if (tangentCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec4);
	m_Tangents = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Tangents.get(), tangents, bufferSize);
}

void MeshCreator::ensureTexCoord0(unsigned long long shape_uid, int startTexCoord0, int texCoordCount0, const avs::vec2* texCoords0)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount0 != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec2);
	m_UV0s = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_UV0s.get(), texCoords0, bufferSize);
}

void MeshCreator::ensureTexCoord1(unsigned long long shape_uid, int startTexCoord1, int texCoordCount1, const avs::vec2* texCoords1)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount1 != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec2);
	m_UV1s = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_UV1s.get(), texCoords1, bufferSize);
}

void MeshCreator::ensureColors(unsigned long long shape_uid, int startColor, int colorCount, const avs::vec4* colors)
{
	CHECK_SHAPE_UID(shape_uid);
	if (colorCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec4);
	m_Colors = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Colors.get(), colors, bufferSize);
}

void MeshCreator::ensureJoints(unsigned long long shape_uid, int startJoint, int jointCount, const avs::vec4* joints)
{
	CHECK_SHAPE_UID(shape_uid);
	if (jointCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec4);
	m_Joints = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Joints.get(), joints, bufferSize);
}

void MeshCreator::ensureWeights(unsigned long long shape_uid, int startWeight, int weightCount, const avs::vec4* weights)
{
	CHECK_SHAPE_UID(shape_uid);
	if (weightCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec4);
	m_Weights = std::make_unique<float[]>(bufferSize / sizeof(float));
	memcpy((void*)m_Weights.get(), weights, bufferSize);
}

void MeshCreator::ensureFaces(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::int3* faces)
{
	CHECK_SHAPE_UID(shape_uid);
	//?
}

void MeshCreator::ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, const unsigned int* indices)
{
	CHECK_SHAPE_UID(shape_uid);

	if (indexCount % 3 > 0)
	{
		SIMUL_BREAK_ONCE("indexCount is not a multiple of 3.\n");
		return;
	}
	m_PolygonCount = indexCount / 3;
	
	m_IndexCount = indexCount;
	size_t bufferSize = m_IndexCount * sizeof(unsigned int);
	m_Indices = std::make_unique<unsigned int[]>(bufferSize / sizeof(unsigned int));
	memcpy((void*)m_Indices.get(), (void*)indices, bufferSize);

	m_Mesh->SetSubMesh(s_TotalSubMeshes, startIndex, indexCount, nullptr);
}

void MeshCreator::Create(simul::crossplatform::RenderPlatform* renderPlatform)
{
	if (renderPlatform == nullptr || m_Vertices == nullptr || m_Indices == nullptr
		|| m_VertexCount == 0 || m_PolygonCount == 0)
	{
		SIMUL_COUT << "Insufficient data to create mesh.\n";
		return;
	}

	m_Mesh->Initialize(renderPlatform, m_VertexCount, m_Vertices.get(), m_Normals.get(), m_UV0s.get(), m_PolygonCount, m_Indices.get(), nullptr);
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
