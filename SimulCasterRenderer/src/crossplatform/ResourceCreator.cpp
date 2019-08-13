#include "ResourceCreator.h"

using namespace avs;

ResourceCreator::ResourceCreator()
{
}

ResourceCreator::~ResourceCreator()
{
}

void ResourceCreator::SetRenderPlatform(scr::API::APIType api)
{
	m_API.SetAPI(api);
	
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined(WIN64)
	switch (m_API.GetAPI())
	{
	case scr::API::APIType::UNKNOWN:	m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::D3D11:		m_pRenderPlatform = std::make_unique<scr::PC_RenderPlatform>(); break;
	case scr::API::APIType::D3D12:		m_pRenderPlatform = std::make_unique<scr::PC_RenderPlatform>(); break;
	case scr::API::APIType::OPENGL:		m_pRenderPlatform = std::make_unique<scr::PC_RenderPlatform>(); break;
	case scr::API::APIType::OPENGLES:	m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::VULKAN:		m_pRenderPlatform = std::make_unique<scr::PC_RenderPlatform>(); break;

	default:							m_pRenderPlatform = nullptr; break;
	}
#elif defined(__ANDROID__)
	switch (m_API.GetAPI())
	{
	case scr::API::APIType::UNKNOWN:	m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::D3D11:		m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::D3D12:		m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::OPENGL:		m_pRenderPlatform = nullptr; break;
	case scr::API::APIType::OPENGLES:	m_pRenderPlatform = std::make_unique<scr::GL_RenderPlatform>(); break;
	case scr::API::APIType::VULKAN:		m_pRenderPlatform = nullptr; break;

	default:							m_pRenderPlatform = nullptr; break;
	}
#endif

}

void ResourceCreator::ensureVertices(unsigned long long shape_uid, int startVertex, int vertexCount, const avs::vec3* vertices)
{
	CHECK_SHAPE_UID(shape_uid);

	m_VertexCount = vertexCount;
	m_Vertices = vertices;
}

void ResourceCreator::ensureNormals(unsigned long long shape_uid, int startNormal, int normalCount, const avs::vec3* normals)
{
	CHECK_SHAPE_UID(shape_uid);
	if (normalCount != m_VertexCount)
		return;

	m_Normals = normals;
}

void ResourceCreator::ensureTangents(unsigned long long shape_uid, int startTangent, int tangentCount, const avs::vec4* tangents)
{
	CHECK_SHAPE_UID(shape_uid);
	if (tangentCount != m_VertexCount)
		return;

	m_Tangents = tangents;
}

void ResourceCreator::ensureTexCoord0(unsigned long long shape_uid, int startTexCoord0, int texCoordCount0, const avs::vec2* texCoords0)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount0 != m_VertexCount)
		return;

	m_UV0s = texCoords0;
}

void ResourceCreator::ensureTexCoord1(unsigned long long shape_uid, int startTexCoord1, int texCoordCount1, const avs::vec2* texCoords1)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount1 != m_VertexCount)
		return;

	m_UV1s = texCoords1;
}

void ResourceCreator::ensureColors(unsigned long long shape_uid, int startColor, int colorCount, const avs::vec4* colors)
{
	CHECK_SHAPE_UID(shape_uid);
	if (colorCount != m_VertexCount)
		return;

	m_Colors = colors;
}

void ResourceCreator::ensureJoints(unsigned long long shape_uid, int startJoint, int jointCount, const avs::vec4* joints)
{
	CHECK_SHAPE_UID(shape_uid);
	if (jointCount != m_VertexCount)
		return;

	m_Joints = joints;
}

void ResourceCreator::ensureWeights(unsigned long long shape_uid, int startWeight, int weightCount, const avs::vec4* weights)
{
	CHECK_SHAPE_UID(shape_uid);
	if (weightCount != m_VertexCount)
		return;

	size_t bufferSize = m_VertexCount * sizeof(avs::vec4);
	m_Weights = weights;
}

void ResourceCreator::ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, const unsigned int* indices)
{
	CHECK_SHAPE_UID(shape_uid);

	if (indexCount % 3 > 0)
	{
		SCR_CERR_BREAK("indexCount is not a multiple of 3.\n", -1);
		return;
	}
	m_PolygonCount = indexCount / 3;
	
	m_IndexCount = indexCount;
	m_Indices = indices;

}

avs::Result ResourceCreator::Assemble()
{
	using namespace scr;

	if(m_VertexBufferManager->Has(shape_uid) ||	m_IndexBufferManager->Has(shape_uid))
		return avs::Result::OK;

	if (!m_pRenderPlatform)
	{
		SCR_CERR("No valid render platform was found.");
        return avs::Result::GeometryDecoder_ClientRendererError;
	}

	VertexBufferLayout layout;
	size_t stride = 0;
	if (m_Vertices)	{ layout.AddAttribute((uint32_t)AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);	stride += 3; }
	if (m_Normals)	{ layout.AddAttribute((uint32_t)AttributeSemantic::NORMAL, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);		stride += 3; }
	if (m_Tangents)	{ layout.AddAttribute((uint32_t)AttributeSemantic::TANGENT, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);		stride += 4; }
	if (m_UV0s)		{ layout.AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);	stride += 2; }
	if (m_UV1s)		{ layout.AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);	stride += 2; }
	if (m_Colors)	{ layout.AddAttribute((uint32_t)AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);		stride += 4; }
	if (m_Joints)	{ layout.AddAttribute((uint32_t)AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	stride += 4; }
	if (m_Weights)	{ layout.AddAttribute((uint32_t)AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	stride += 4; }

	m_InterleavedVBOSize = 4 * stride * m_VertexCount;
	m_InterleavedVBO = std::make_unique<float[]>(m_InterleavedVBOSize);
	for (size_t i = 0; i < m_VertexCount; i++)
	{
		size_t intraStrideOffset = 0;
		if(m_Vertices)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Vertices + i, sizeof(vec3));	intraStrideOffset +=3;}
		if(m_Normals)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Normals + i, sizeof(vec3));	intraStrideOffset +=3;}
		if(m_Tangents)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Tangents + i, sizeof(vec4));	intraStrideOffset +=4;}
		if(m_UV0s)		{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_UV0s + i, sizeof(vec2));		intraStrideOffset +=2;}
		if(m_UV1s)		{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_UV1s + i, sizeof(vec2));		intraStrideOffset +=2;}
		if(m_Colors)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Colors + i, sizeof(vec4));		intraStrideOffset +=4;}
		if(m_Joints)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Joints + i, sizeof(vec4));		intraStrideOffset +=4;}
		if(m_Weights)	{memcpy(m_InterleavedVBO.get() + (stride * i) + intraStrideOffset, m_Weights + i, sizeof(vec4));	intraStrideOffset +=4;}
	}

	if (m_InterleavedVBOSize == 0 || m_InterleavedVBO == nullptr || m_IndexCount == 0 || m_Indices == nullptr)
	{
		SCR_CERR("Unable to construct vertex and index buffers.");
		return avs::Result::GeometryDecoder_ClientRendererError;
	}

	std::shared_ptr<scr::VertexBuffer> vbo = m_pRenderPlatform->InstantiateVertexBuffer();
	vbo->SetLayout(layout);
	vbo->Create(m_InterleavedVBOSize, (const void*)m_InterleavedVBO.get());

	std::shared_ptr<scr::IndexBuffer> ibo = m_pRenderPlatform->InstantiateIndexBuffer();
	ibo->Create(m_IndexCount, m_Indices);

	m_VertexBufferManager->Add(shape_uid, vbo.get(), m_PostUseLifetime);
	m_IndexBufferManager->Add(shape_uid, ibo.get(), m_PostUseLifetime);

    return avs::Result::OK;
}
