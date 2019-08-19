#include "ResourceCreator.h"

#include "Material.h"

using namespace avs;

ResourceCreator::ResourceCreator()
{
}

ResourceCreator::~ResourceCreator()
{
}

void ResourceCreator::SetRenderPlatform(scr::RenderPlatform* r)
{
	m_API.SetAPI(r->GetAPI());
	m_pRenderPlatform = r;

	// Removed circular dependencies.
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

void ResourceCreator::ensureTangentNormals(unsigned long long shape_uid, int startNormal, int tnCount, size_t tnSize, const uint8_t* tn)
{
	CHECK_SHAPE_UID(shape_uid);
	assert(tnCount == m_VertexCount);
	m_TangentNormalSize = tnSize;
	m_TangentNormals = tn;
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

void ResourceCreator::ensureIndices(unsigned long long shape_uid, int startIndex, int indexCount, int indexSize, const unsigned char* indices)
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
	m_IndexSize = indexSize;
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

	std::shared_ptr<VertexBufferLayout> layout(new VertexBufferLayout);
	if (m_Vertices)	{ layout->AddAttribute((uint32_t)AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);	}
	if (m_Normals||m_TangentNormals)
	{
		layout->AddAttribute((uint32_t)AttributeSemantic::NORMAL, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
	}
	if (m_Tangents||m_TangentNormals)
	{
		layout->AddAttribute((uint32_t)AttributeSemantic::TANGENT, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
	}
	if (m_UV0s)		{ layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);	}
	if (m_UV1s)		{ layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);	}
	if (m_Colors)	{ layout->AddAttribute((uint32_t)AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);		}
	if (m_Joints)	{ layout->AddAttribute((uint32_t)AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	}
	if (m_Weights)	{ layout->AddAttribute((uint32_t)AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	}
	layout->CalculateStride();

	size_t interleavedVBSize = 0;
	std::unique_ptr<float[]> interleavedVB = nullptr;
	interleavedVBSize = layout->m_Stride * m_VertexCount;
	interleavedVB = std::make_unique<float[]>(interleavedVBSize);

	for (size_t i = 0; i < m_VertexCount; i++)
	{
		size_t intraStrideOffset = 0;
		if(m_Vertices)	{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Vertices + i, sizeof(avs::vec3));intraStrideOffset +=3;}
		if (m_TangentNormals)
		{
			avs::vec3 normal;
			avs::vec4 tangent;
			char *nt =(char*)( m_TangentNormals + (m_TangentNormalSize*i));
			// tangentx tangentz
			if (m_TangentNormalSize == 8)
			{
				Vec4<char> &x8 = *((avs::Vec4<char>*)(nt));
				tangent.x = x8.x / 127.0f;
				tangent.y = x8.y / 127.0f;
				tangent.z = x8.z / 127.0f;
				tangent.w = x8.w / 127.0f;
				Vec4<char> &n8=*((avs::Vec4<char>*)(nt+4));
				normal.x = n8.x / 127.0f;
				normal.y = n8.y / 127.0f;
				normal.z = n8.z / 127.0f;
			}
			else
			{
				Vec4<short> &x8 = *((avs::Vec4<short>*)(nt));
				tangent.x = x8.x / 32767.0f;
				tangent.y = x8.y / 32767.0f;
				tangent.z = x8.z / 32767.0f;
				tangent.w = x8.w / 32767.0f;
				Vec4<short> &n8 = *((avs::Vec4<short>*)(nt + 8));
				normal.x = n8.x / 32767.0f;
				normal.y = n8.y / 32767.0f;
				normal.z = n8.z / 32767.0f;
			}
			memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset,&normal, sizeof(avs::vec3));
			intraStrideOffset += 3;
			memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset,&tangent , sizeof(avs::vec4));
			intraStrideOffset += 4;
		}
		else
		{
			if (m_Normals) { memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Normals + i, sizeof(avs::vec3));	intraStrideOffset += 3; }
			if (m_Tangents) { memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Tangents + i, sizeof(avs::vec4)); intraStrideOffset += 4; }
		}
		if(m_UV0s)		{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_UV0s + i, sizeof(avs::vec2));	intraStrideOffset +=2;}
		if(m_UV1s)		{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_UV1s + i, sizeof(avs::vec2));	intraStrideOffset +=2;}
		if(m_Colors)	{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Colors + i, sizeof(avs::vec4));	intraStrideOffset +=4;}
		if(m_Joints)	{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Joints + i, sizeof(avs::vec4));	intraStrideOffset +=4;}
		if(m_Weights)	{memcpy(interleavedVB.get() + (layout->m_Stride * i) + intraStrideOffset, m_Weights + i, sizeof(avs::vec4));	intraStrideOffset +=4;}
	}

	if (interleavedVBSize == 0 || interleavedVB == nullptr || m_IndexCount == 0 || m_Indices == nullptr)
	{
		SCR_CERR("Unable to construct vertex and index buffers.");
		return avs::Result::GeometryDecoder_ClientRendererError;
	}

	std::shared_ptr<VertexBuffer> vb = m_pRenderPlatform->InstantiateVertexBuffer();
	VertexBuffer::VertexBufferCreateInfo vb_ci;
	vb_ci.layout = std::move(layout);
	vb_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
	vb_ci.size = interleavedVBSize;
	vb_ci.data = (const void*)interleavedVB.get();
	vb->Create(&vb_ci);
	
	std::shared_ptr<IndexBuffer> ib = m_pRenderPlatform->InstantiateIndexBuffer();
	IndexBuffer::IndexBufferCreateInfo ib_ci;
	ib_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
	ib_ci.indexCount = m_IndexCount;
	ib_ci.stride = m_IndexSize;
	ib_ci.data = m_Indices;
	ib->Create(&ib_ci);

	m_VertexBufferManager->Add(shape_uid, std::move(vb), m_PostUseLifetime);
	m_IndexBufferManager->Add(shape_uid, std::move(ib), m_PostUseLifetime);

	m_Vertices = nullptr;
	m_Normals = nullptr;
	m_Tangents = nullptr;
	m_UV0s = nullptr;
	m_UV1s = nullptr;
	m_Colors = nullptr;
	m_Joints = nullptr;
	m_Weights = nullptr;
	m_Indices = nullptr;

    return avs::Result::OK;
}

void ResourceCreator::passTexture(avs::uid texture_uid, const avs::Texture & texture)
{
	std::shared_ptr<scr::Texture> scrTexture = m_pRenderPlatform->InstantiateTexture();
	
	scr::Texture::TextureCreateInfo texInfo =
	{
		texture.width,
		texture.height,
		texture.depth,
		texture.bytesPerPixel,
		texture.arrayCount,
		texture.mipCount,
		scr::Texture::Slot::UNKNOWN,
		scr::Texture::Type::TEXTURE_2D,
		scr::Texture::Format::FORMAT_UNKNOWN,
		scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
		texture.width * texture.height * 4, //Width * Height * Channels
		texture.data
	};

	scrTexture->Create(&texInfo);
	
	m_TextureManager->Add(texture_uid, std::move(scrTexture));
}


void ResourceCreator::passMaterial(avs::uid material_uid, const avs::Material & material)
{
	scr::Material::MaterialCreateInfo materialInfo;

	//scr::Material scrMaterial(&materialInfo);
}

void ResourceCreator::passNode(avs::uid node_uid, avs::DataNode& node)
{
	// It may just be an update of a node's transform and/or children 
	if (nodes.find(node_uid) == nodes.end())
	{
		nodes[node_uid] = std::make_shared<avs::DataNode>(std::move(node));
	}
	else
	{
		nodes[node_uid]->childrenUids = node.childrenUids;
		nodes[node_uid]->transform = node.transform;
	}
}