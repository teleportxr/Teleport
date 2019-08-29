// (C) Copyright 2018-2019 Simul Software Ltd
#include "ResourceCreator.h"

#include "Material.h"

using namespace avs;

std::vector<std::pair<avs::uid, avs::uid>> ResourceCreator::m_MeshMaterialUIDPairs;

ResourceCreator::ResourceCreator()
	:basis_codeBook(basist::g_global_selector_cb_size, basist::g_global_selector_cb), basis_transcoder(&basis_codeBook), basis_textureFormat(basist::transcoder_texture_format::cTFBC1)
{
	basist::basisu_transcoder_init();
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

void ResourceCreator::ensureVertices(avs::uid shape_uid, int startVertex, int vertexCount, const avs::vec3* vertices)
{
	CHECK_SHAPE_UID(shape_uid);

	m_VertexCount = vertexCount;
	m_Vertices = vertices;
}

void ResourceCreator::ensureNormals(avs::uid shape_uid, int startNormal, int normalCount, const avs::vec3* normals)
{
	CHECK_SHAPE_UID(shape_uid);
	if ((size_t)normalCount != m_VertexCount)
		return;

	m_Normals = normals;
}

void ResourceCreator::ensureTangentNormals(avs::uid shape_uid, int startNormal, int tnCount, size_t tnSize, const uint8_t* tn)
{
	CHECK_SHAPE_UID(shape_uid);
	assert((size_t)tnCount == m_VertexCount);
	m_TangentNormalSize = tnSize;
	m_TangentNormals = tn;
}

void ResourceCreator::ensureTangents(avs::uid shape_uid, int startTangent, int tangentCount, const avs::vec4* tangents)
{
	CHECK_SHAPE_UID(shape_uid);
	if (tangentCount != (int)m_VertexCount)
		return;

	m_Tangents = tangents;
}

void ResourceCreator::ensureTexCoord0(avs::uid shape_uid, int startTexCoord0, int texCoordCount0, int offset, const avs::vec2* texCoords0)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount0 != (int)m_VertexCount)
		return;

	m_UV0s = texCoords0;
}

void ResourceCreator::ensureTexCoord1(avs::uid shape_uid, int startTexCoord1, int texCoordCount1, int offset, const avs::vec2* texCoords1)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount1 != (int)m_VertexCount)
		return;

	m_UV1s = texCoords1;
}
void ResourceCreator::ensureTexCoord0_Half(avs::uid shape_uid, int startTexCoord0, int texCoordCount0, int offset, const avs::hvec2* texCoords0)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount0 != (int)m_VertexCount)
		return;
	//Extract from interleaved UV buffer
	std::vector<avs::hvec2> uv0;
	for (size_t i = 0; i < m_VertexCount; i++)
		uv0.push_back(texCoords0[i * 2]);

	size_t uv0BufferSize = uv0.size() * sizeof(avs::hvec2);
	m_Half_UV0s = (const avs::hvec2*)malloc(uv0BufferSize);
	memcpy((void*)m_Half_UV0s, uv0.data(), uv0BufferSize);
}

void ResourceCreator::ensureTexCoord1_Half(avs::uid shape_uid, int startTexCoord1, int texCoordCount1, int offset, const avs::hvec2* texCoords1)
{
	CHECK_SHAPE_UID(shape_uid);
	if (texCoordCount1 != (int)m_VertexCount)
		return;

	//Extract from interleaved UV buffer
	std::vector<avs::hvec2> uv1;
	for (size_t i = 0; i < m_VertexCount; i++)
		uv1.push_back(texCoords1[(i * 2) + 1]);

	size_t uv1BufferSize = uv1.size() * sizeof(avs::hvec2);
	m_Half_UV1s = (const avs::hvec2*)malloc(uv1BufferSize);
	memcpy((void*)m_Half_UV1s, uv1.data(), uv1BufferSize);
}

void ResourceCreator::ensureColors(avs::uid shape_uid, int startColor, int colorCount, const avs::vec4* colors)
{
	CHECK_SHAPE_UID(shape_uid);
	if (colorCount != (int)m_VertexCount)
		return;

	m_Colors = colors;
}

void ResourceCreator::ensureJoints(avs::uid shape_uid, int startJoint, int jointCount, const avs::vec4* joints)
{
	CHECK_SHAPE_UID(shape_uid);
	if (jointCount != (int)m_VertexCount)
		return;

	m_Joints = joints;
}

void ResourceCreator::ensureWeights(avs::uid shape_uid, int startWeight, int weightCount, const avs::vec4* weights)
{
	CHECK_SHAPE_UID(shape_uid);
	if (weightCount != (int)m_VertexCount)
		return;

	m_Weights = weights;
}

void ResourceCreator::ensureIndices(avs::uid shape_uid, int startIndex, int indexCount, int indexSize, const unsigned char* indices)
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
void ResourceCreator::ensureMaterialUID(avs::uid shape_uid, avs::uid _material_uid)
{
	CHECK_SHAPE_UID(shape_uid);
	m_MeshMaterialUIDPairs.push_back({ shape_uid, _material_uid });
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
	if (m_Half_UV0s){ layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::HALF);	}
	if (m_Half_UV1s){ layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::HALF);	}
	if (m_Colors)	{ layout->AddAttribute((uint32_t)AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	}
	if (m_Joints)	{ layout->AddAttribute((uint32_t)AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	}
	if (m_Weights)	{ layout->AddAttribute((uint32_t)AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);	}
	layout->CalculateStride();

	size_t interleavedVBSize = layout->m_Stride * m_VertexCount;
	size_t indicesSize = m_IndexCount * m_IndexSize;

	std::unique_ptr<float[]> interleavedVB = std::make_unique<float[]>(interleavedVBSize);
	std::unique_ptr<uint8_t[]> _indices = std::make_unique<uint8_t[]>(indicesSize);
	memcpy(_indices.get(), m_Indices, indicesSize);

	for (size_t i = 0; i < m_VertexCount; i++)
	{
		size_t intraStrideOffset = 0;
		if(m_Vertices)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Vertices + i, sizeof(avs::vec3));intraStrideOffset +=3;}
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
			memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset,&normal, sizeof(avs::vec3));
			intraStrideOffset += 3;
			memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset,&tangent , sizeof(avs::vec4));
			intraStrideOffset += 4;
		}
		else
		{
			if (m_Normals) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Normals + i, sizeof(avs::vec3));	intraStrideOffset += 3; }
			if (m_Tangents) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Tangents + i, sizeof(avs::vec4));	intraStrideOffset += 4; }
		}
		if(m_UV0s)		{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_UV0s + i, sizeof(avs::vec2));			intraStrideOffset +=2;}
		if(m_UV1s)		{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_UV1s + i, sizeof(avs::vec2));			intraStrideOffset +=2;}
		if(m_Half_UV0s)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Half_UV0s + i, sizeof(avs::hvec2));		intraStrideOffset +=1;}
		if(m_Half_UV1s)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Half_UV1s + i, sizeof(avs::hvec2));		intraStrideOffset +=1;}
		if(m_Colors)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Colors + i, sizeof(avs::vec4));			intraStrideOffset +=4;}
		if(m_Joints)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Joints + i, sizeof(avs::vec4));			intraStrideOffset +=4;}
		if(m_Weights)	{memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, m_Weights + i, sizeof(avs::vec4));		intraStrideOffset +=4;}
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
	vb_ci.vertexCount = m_VertexCount;
	vb_ci.size = interleavedVBSize;
	vb_ci.data = (const void*)interleavedVB.get();
	vb->Create(&vb_ci);
	
	std::shared_ptr<IndexBuffer> ib = m_pRenderPlatform->InstantiateIndexBuffer();
	IndexBuffer::IndexBufferCreateInfo ib_ci;
	ib_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
	ib_ci.indexCount = m_IndexCount;
	ib_ci.stride = m_IndexSize;
	ib_ci.data = _indices.get();
	ib->Create(&ib_ci);

	Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.vb = vb;
	mesh_ci.ib = ib;
	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>(&mesh_ci);
	m_pActorManager->AddMesh(shape_uid, mesh);

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

	shape_uid = (avs::uid) -1;

    return avs::Result::OK;
}

//Returns a scr::Texture::Format from a avs::TextureFormat.
scr::Texture::Format textureFormatFromAVSTextureFormat(avs::TextureFormat format)
{
	switch(format)
	{
		case avs::TextureFormat::INVALID: return scr::Texture::Format::FORMAT_UNKNOWN;
		case avs::TextureFormat::G8: return scr::Texture::Format::R8;
		case avs::TextureFormat::BGRA8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::BGRE8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::RGBA16: return scr::Texture::Format::RGBA16;
		case avs::TextureFormat::RGBE8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::RGBA16F: return scr::Texture::Format::RGBA16F;
		case avs::TextureFormat::RGBA8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::MAX: return scr::Texture::Format::FORMAT_UNKNOWN;
	}
}

//Returns a SCR compression format from a basis universal transcoder format.
scr::Texture::CompressionFormat toSCRCompressionFormat(basist::transcoder_texture_format format)
{
	switch(format)
	{
		case basist::transcoder_texture_format::cTFBC1: return scr::Texture::CompressionFormat::BC1;
		case basist::transcoder_texture_format::cTFBC3: return scr::Texture::CompressionFormat::BC3;
		case basist::transcoder_texture_format::cTFBC4: return scr::Texture::CompressionFormat::BC4;
		case basist::transcoder_texture_format::cTFBC5: return scr::Texture::CompressionFormat::BC5;
		case basist::transcoder_texture_format::cTFETC1: return scr::Texture::CompressionFormat::ETC1;
		case basist::transcoder_texture_format::cTFETC2: return scr::Texture::CompressionFormat::ETC2;
		case basist::transcoder_texture_format::cTFPVRTC1_4_OPAQUE_ONLY: return scr::Texture::CompressionFormat::PVRTC1_4_OPAQUE_ONLY;
		case basist::transcoder_texture_format::cTFBC7_M6_OPAQUE_ONLY: return scr::Texture::CompressionFormat::BC7_M6_OPAQUE_ONLY;
		case basist::transcoder_texture_format::cTFTotalTextureFormats: return scr::Texture::CompressionFormat::UNCOMPRESSED;
	}
}

void ResourceCreator::passTexture(avs::uid texture_uid, const avs::Texture& texture)
{
	scr::Texture::TextureCreateInfo texInfo =
	{
		texture.width,
		texture.height,
		texture.depth,
		texture.bytesPerPixel,
		texture.arrayCount,
		texture.mipCount,
		scr::Texture::Slot::UNKNOWN,
		scr::Texture::Type::TEXTURE_2D, //Assumed
		textureFormatFromAVSTextureFormat(texture.format),
		scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT, //Assumed
		0,
		nullptr,
		scr::Texture::CompressionFormat::UNCOMPRESSED
	};

	if(basis_transcoder.start_transcoding(texture.data, texture.dataSize))
	{
		uint32_t basisWidth, basisHeight, basisBlocks;

		basis_transcoder.get_image_level_desc(texture.data, texture.dataSize, 0, 0, basisWidth, basisHeight, basisBlocks);
		uint32_t outDataSize = basist::basis_get_bytes_per_block(basis_textureFormat) * basisBlocks;

		unsigned char* outData = new unsigned char[outDataSize];
		if(basis_transcoder.transcode_image_level(texture.data, texture.dataSize, 0, 0, outData, basisBlocks, basis_textureFormat))
		{
			delete[] texture.data;

			texInfo.size = outDataSize;
			texInfo.data = outData;
			texInfo.compression = toSCRCompressionFormat(basis_textureFormat);
		}
		else
		{
			delete[] outData;
		}
	}

	//The data is uncompressed if we failed to transcode it.
	if(!texInfo.data)
	{
		texInfo.size = texture.dataSize;
		texInfo.data = texture.data;
	}

	std::shared_ptr<scr::Texture> scrTexture = m_pRenderPlatform->InstantiateTexture();
	scrTexture->Create(&texInfo);
	
	m_TextureManager->Add(texture_uid, std::move(scrTexture));
}

///Most of these sets need actual values, rather than default initalisers.
void ResourceCreator::passMaterial(avs::uid material_uid, const avs::Material & material)
{
	///Claims textures without ever unclaiming the textures.
	scr::Material::MaterialCreateInfo materialInfo;
	materialInfo.diffuse.texture = nullptr;
	materialInfo.normal.texture = nullptr;
	materialInfo.combined.texture = nullptr;

	if(material.pbrMetallicRoughness.baseColorTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> *diffuseTexture = m_TextureManager->Claim(material.pbrMetallicRoughness.baseColorTexture.index);

		if(diffuseTexture)
		{
			materialInfo.diffuse.texture = &**diffuseTexture;

			materialInfo.diffuse.texCoordsScalar[0] = {1, 1};
			materialInfo.diffuse.texCoordsScalar[1] = {1, 1};
			materialInfo.diffuse.texCoordsScalar[2] = {1, 1};
			materialInfo.diffuse.texCoordsScalar[3] = {1, 1};

			materialInfo.diffuse.textureOutputScalar =
			{
				material.pbrMetallicRoughness.baseColorFactor.x,
				material.pbrMetallicRoughness.baseColorFactor.y,
				material.pbrMetallicRoughness.baseColorFactor.z,
				material.pbrMetallicRoughness.baseColorFactor.w,
			};
		}
		else
		{
			///REQUEST RESEND OF TEXTURE
		}
	}

	if(material.normalTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> *normalTexture = m_TextureManager->Claim(material.normalTexture.index);

		if(normalTexture)
		{
			materialInfo.normal.texture = &**normalTexture;

			materialInfo.normal.texCoordsScalar[0] = {1, 1};
			materialInfo.normal.texCoordsScalar[1] = {1, 1};
			materialInfo.normal.texCoordsScalar[2] = {1, 1};
			materialInfo.normal.texCoordsScalar[3] = {1, 1};

			materialInfo.normal.textureOutputScalar = {1, 1, 1, 1};
		}
		else
		{
			///REQUEST RESEND OF TEXTURE
		}
	}

	if(material.occlusionTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> *occlusionTexture = m_TextureManager->Claim(material.normalTexture.index);

		if(occlusionTexture)
		{
			materialInfo.combined.texture = &**occlusionTexture;

			materialInfo.combined.texCoordsScalar[0] = {1, 1};
			materialInfo.combined.texCoordsScalar[1] = {1, 1};
			materialInfo.combined.texCoordsScalar[2] = {1, 1};
			materialInfo.combined.texCoordsScalar[3] = {1, 1};

			materialInfo.combined.textureOutputScalar = {1, 1, 1, 1};
		}
		else
		{
			///REQUEST RESEND OF TEXTURE
		}
	}

	///This needs an actual value.
	materialInfo.effect = nullptr;
	std::shared_ptr<scr::Material> scr_material = std::make_shared<scr::Material>(m_pRenderPlatform,&materialInfo);
	m_pActorManager->AddMaterial(material_uid, scr_material);
	m_materialManager->Add(material_uid, std::move(scr_material));
}

void ResourceCreator::passNode(avs::uid node_uid, avs::DataNode& node)
{
	// It may just be an update of a node's transform and/or children 
	if (nodes.find(node_uid) == nodes.end())
	{
		nodes[node_uid] = std::make_shared<avs::DataNode>(node);
	}
	else
	{
		nodes[node_uid]->childrenUids = node.childrenUids;
		nodes[node_uid]->transform = node.transform;
	}

	scr::vec3 translation = scr::vec3(node.transform.position.x, node.transform.position.y, node.transform.position.z);
	scr::quat rotation = scr::quat(node.transform.rotation.w, node.transform.rotation.x, node.transform.rotation.y, node.transform.rotation.z);
	scr::vec3 scale = scr::vec3(node.transform.scale.x, node.transform.scale.y, node.transform.scale.z);

	if (m_pActorManager->GetTransform(node_uid) != nullptr) //Check the transform has already been added, if so update transform.
	{
		m_pActorManager->GetTransform(node_uid)->UpdateModelMatrix(translation, rotation, scale);
	}
	else
	{
		std::shared_ptr<scr::Transform> transform = std::make_shared<scr::Transform>();
		transform->UpdateModelMatrix(translation, rotation, scale);
		m_pActorManager->AddTransform(node_uid, transform);

	    switch (node.data_type)
	    {
	    case NodeDataType::Mesh:
	    	{
	    		size_t i = 0;
	    		for (auto& meshMaterialPair : m_MeshMaterialUIDPairs)
	    		{
	    			if (meshMaterialPair.first == node.data_uid) //data_uid == shape_uid
	    				break;
	    			else
	    				i++;
	    		}
				CreateActor(m_MeshMaterialUIDPairs[i].first, {m_MeshMaterialUIDPairs[i].second}, node_uid);
	    	}
	    case NodeDataType::Camera:
	    	return;
	    case NodeDataType::Scene:
	    	return;
	    }
	}
}

void ResourceCreator::CreateActor(avs::uid mesh_uid, const std::vector<avs::uid>& material_uids, avs::uid transform_uid)
{
	scr::Actor::ActorCreateInfo actor_ci = {};
	actor_ci.staticMesh = true;
	actor_ci.animatedMesh = false;
	actor_ci.mesh = m_pActorManager->GetMesh(mesh_uid).get();
	actor_ci.material = m_pActorManager->GetMaterial(material_uids[0]).get();
	actor_ci.transform = m_pActorManager->GetTransform(transform_uid).get();
	m_pActorManager->CreateActor(GenerateUid(), &actor_ci);
}