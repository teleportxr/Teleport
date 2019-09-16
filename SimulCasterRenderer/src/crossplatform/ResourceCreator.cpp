// (C) Copyright 2018-2019 Simul Software Ltd
#include "ResourceCreator.h"

#include "Material.h"

using namespace avs;

ResourceCreator::ResourceCreator(basist::transcoder_texture_format transcoderTextureFormat)
	:basis_codeBook(basist::g_global_selector_cb_size, basist::g_global_selector_cb), basis_textureFormat(transcoderTextureFormat)
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
}


avs::Result ResourceCreator::Assemble(avs::MeshCreate* meshCreate)
{
	using namespace scr;

	if(m_VertexBufferManager->Has(meshCreate->mesh_uid) ||	m_IndexBufferManager->Has(meshCreate->mesh_uid))
		return avs::Result::OK;

	if (!m_pRenderPlatform)
	{
		SCR_CERR("No valid render platform was found.");
        return avs::Result::GeometryDecoder_ClientRendererError;
	}
	Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.vb.resize(meshCreate->m_NumElements);
	mesh_ci.ib.resize(meshCreate->m_NumElements);

	for (size_t i = 0; i < meshCreate->m_NumElements; i++)
	{
		MeshElementCreate* meshElementCreate = &(meshCreate->m_MeshElementCreate[i]);
		std::shared_ptr<VertexBufferLayout> layout(new VertexBufferLayout);
		if (meshElementCreate->m_Vertices) { layout->AddAttribute((uint32_t)AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT); }

		if (meshElementCreate->m_Normals || meshElementCreate->m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::NORMAL, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_Tangents || meshElementCreate->m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TANGENT, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_UV0s)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_UV1s)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_Colors)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_Joints)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate->m_Weights)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		layout->CalculateStride();

		size_t interleavedVBSize = layout->m_Stride * meshElementCreate->m_VertexCount;
		size_t indicesSize = meshElementCreate->m_IndexCount * meshElementCreate->m_IndexSize;

		std::unique_ptr<float[]> interleavedVB = std::make_unique<float[]>(interleavedVBSize);
		std::unique_ptr<uint8_t[]> _indices = std::make_unique<uint8_t[]>(indicesSize);
		memcpy(_indices.get(), meshElementCreate->m_Indices, indicesSize);
		
		for (size_t i = 0; i < meshElementCreate->m_VertexCount; i++)
		{
			size_t intraStrideOffset = 0;
			if (meshElementCreate->m_Vertices)
			{
				memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Vertices + i, sizeof(avs::vec3)); intraStrideOffset += 3;
			}
			if (meshElementCreate->m_TangentNormals)
			{
				avs::vec3 normal;
				avs::vec4 tangent;
				char* nt = (char*)(meshElementCreate->m_TangentNormals + (meshElementCreate->m_TangentNormalSize * i));
				// tangentx tangentz
				if (meshElementCreate->m_TangentNormalSize == 8)
				{
					Vec4<char>& x8 = *((avs::Vec4<char>*)(nt));
					tangent.x = x8.x / 127.0f;
					tangent.y = x8.y / 127.0f;
					tangent.z = x8.z / 127.0f;
					tangent.w = x8.w / 127.0f;
					Vec4<char>& n8 = *((avs::Vec4<char>*)(nt + 4));
					normal.x = n8.x / 127.0f;
					normal.y = n8.y / 127.0f;
					normal.z = n8.z / 127.0f;
				}
				else // 16
				{
					Vec4<short>& x8 = *((avs::Vec4<short>*)(nt));
					tangent.x = x8.x / 32767.0f;
					tangent.y = x8.y / 32767.0f;
					tangent.z = x8.z / 32767.0f;
					tangent.w = x8.w / 32767.0f;
					Vec4<short>& n8 = *((avs::Vec4<short>*)(nt + 8));
					normal.x = n8.x / 32767.0f;
					normal.y = n8.y / 32767.0f;
					normal.z = n8.z / 32767.0f;
				}
				memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, &normal, sizeof(avs::vec3));
				intraStrideOffset += 3;
				memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, &tangent, sizeof(avs::vec4));
				intraStrideOffset += 4;
			}
			else
			{
				if (meshElementCreate->m_Normals)
				{
					memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Normals + i, sizeof(avs::vec3));
					intraStrideOffset += 3;
				}
				if (meshElementCreate->m_Tangents)
				{
					memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Tangents + i, sizeof(avs::vec4));
					intraStrideOffset += 4;
				}
			}
			if (meshElementCreate->m_UV0s) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_UV0s + i, sizeof(avs::vec2));			intraStrideOffset += 2; }
			if (meshElementCreate->m_UV1s) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_UV1s + i, sizeof(avs::vec2));			intraStrideOffset += 2; }
			if (meshElementCreate->m_Colors) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Colors + i, sizeof(avs::vec4));			intraStrideOffset += 4; }
			if (meshElementCreate->m_Joints) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Joints + i, sizeof(avs::vec4));			intraStrideOffset += 4; }
			if (meshElementCreate->m_Weights) { memcpy(interleavedVB.get() + (layout->m_Stride / 4 * i) + intraStrideOffset, meshElementCreate->m_Weights + i, sizeof(avs::vec4));		intraStrideOffset += 4; }
		}

		if (interleavedVBSize == 0 || interleavedVB == nullptr || meshElementCreate->m_IndexCount == 0 || meshElementCreate->m_Indices == nullptr)
		{
			SCR_CERR("Unable to construct vertex and index buffers.");
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		std::shared_ptr<VertexBuffer> vb = m_pRenderPlatform->InstantiateVertexBuffer();
		VertexBuffer::VertexBufferCreateInfo vb_ci;
		vb_ci.layout = layout;
		vb_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
		vb_ci.vertexCount = meshElementCreate->m_VertexCount;
		vb_ci.size = interleavedVBSize;
		vb_ci.data = (const void*)interleavedVB.get();
		vb->Create(&vb_ci);

		std::shared_ptr<IndexBuffer> ib = m_pRenderPlatform->InstantiateIndexBuffer();
		IndexBuffer::IndexBufferCreateInfo ib_ci;
		ib_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
		ib_ci.indexCount = meshElementCreate->m_IndexCount;
		ib_ci.stride = meshElementCreate->m_IndexSize;
		ib_ci.data = _indices.get();
		ib->Create(&ib_ci);

		m_VertexBufferManager->Add(meshElementCreate->vb_uid, vb, m_PostUseLifetime);
		m_IndexBufferManager->Add(meshElementCreate->ib_uid, ib, m_PostUseLifetime);

		mesh_ci.vb[i] = vb;
		mesh_ci.ib[i] = ib;
	}
	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>(&mesh_ci);
	m_pActorManager->AddMesh(meshCreate->mesh_uid, mesh);
    return avs::Result::OK;
}

//Returns a scr::Texture::Format from a avs::TextureFormat.
scr::Texture::Format textureFormatFromAVSTextureFormat(avs::TextureFormat format)
{
	switch(format)
	{
		case avs::TextureFormat::INVALID: return scr::Texture::Format::FORMAT_UNKNOWN;
		case avs::TextureFormat::G8: return scr::Texture::Format::R8;
		case avs::TextureFormat::BGRA8: return scr::Texture::Format::BGRA8;
		case avs::TextureFormat::BGRE8: return scr::Texture::Format::BGRA8;
		case avs::TextureFormat::RGBA16: return scr::Texture::Format::RGBA16;
		case avs::TextureFormat::RGBE8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::RGBA16F: return scr::Texture::Format::RGBA16F;
		case avs::TextureFormat::RGBA8: return scr::Texture::Format::RGBA8;
		case avs::TextureFormat::MAX: return scr::Texture::Format::FORMAT_UNKNOWN;
		default:
			exit(1);
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
		default:
			exit(1);
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
   
	//We need a new transcoder for every .basis file.
	basist::basisu_transcoder basis_transcoder(&basis_codeBook);

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
	
	m_TextureManager->Add(texture_uid, scrTexture);
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
		const std::shared_ptr<scr::Texture> diffuseTexture = m_TextureManager->Get(material.pbrMetallicRoughness.baseColorTexture.index);

		if(diffuseTexture)
		{
			materialInfo.diffuse.texture = diffuseTexture;

			scr::vec2 tiling = {material.pbrMetallicRoughness.baseColorTexture.tiling.x, material.pbrMetallicRoughness.baseColorTexture.tiling.y};

			materialInfo.diffuse.texCoordsScalar[0] = tiling;
			materialInfo.diffuse.texCoordsScalar[1] = tiling;
			materialInfo.diffuse.texCoordsScalar[2] = tiling;
			materialInfo.diffuse.texCoordsScalar[3] = tiling;

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
		const std::shared_ptr<scr::Texture> normalTexture = m_TextureManager->Get(material.normalTexture.index);

		if(normalTexture)
		{
			materialInfo.normal.texture = normalTexture;

			scr::vec2 tiling = {material.normalTexture.tiling.x, material.normalTexture.tiling.y};

			materialInfo.normal.texCoordsScalar[0] = tiling;
			materialInfo.normal.texCoordsScalar[1] = tiling;
			materialInfo.normal.texCoordsScalar[2] = tiling;
			materialInfo.normal.texCoordsScalar[3] = tiling;

			materialInfo.normal.textureOutputScalar = {1, 1, 1, 1};
		}
		else
		{
			///REQUEST RESEND OF TEXTURE
		}
	}

	if(material.pbrMetallicRoughness.metallicRoughnessTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> metallicRoughnessTexture = m_TextureManager->Get(material.pbrMetallicRoughness.metallicRoughnessTexture.index);

		if(metallicRoughnessTexture)
		{
			materialInfo.combined.texture = metallicRoughnessTexture;

			scr::vec2 tiling = {material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x, material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y};

			materialInfo.combined.texCoordsScalar[0] = tiling;
			materialInfo.combined.texCoordsScalar[1] = tiling;
			materialInfo.combined.texCoordsScalar[2] = tiling;
			materialInfo.combined.texCoordsScalar[3] = tiling;

			materialInfo.combined.textureOutputScalar = {1, 1, 1, 1};
		}
		else
		{
			///REQUEST RESEND OF TEXTURE
		}
	}

	///This needs an actual value.
	materialInfo.effect = nullptr;
	materialInfo.renderPlatform = m_pRenderPlatform;
	std::shared_ptr<scr::Material> scr_material = std::make_shared<scr::Material>(&materialInfo);
	m_MaterialManager->Add(material_uid, scr_material);
	m_pActorManager->AddMaterial(material_uid, scr_material);
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
		scr::Transform::TransformCreateInfo tci = { m_pRenderPlatform };
		std::shared_ptr<scr::Transform> transform = std::make_shared<scr::Transform>(&tci);
		transform->UpdateModelMatrix(translation, rotation, scale);
		m_pActorManager->AddTransform(node_uid, transform);

	    switch (node.data_type)
	    {
	    case NodeDataType::Mesh:
	    	{
	    		CreateActor(node.data_uid,node.materials, node_uid);
	    	}
	    case NodeDataType::Camera:
	    	return;
	    case NodeDataType::Scene:
	    	return;
	    }
	}
}

void ResourceCreator::CreateActor(avs::uid mesh_uid, const std::vector<avs::uid> &material_uids, avs::uid transform_uid)
{
	scr::Actor::ActorCreateInfo actor_ci = {};
	actor_ci.staticMesh = true;
	actor_ci.animatedMesh = false;
	actor_ci.mesh = m_pActorManager->GetMesh(mesh_uid);
	actor_ci.transform = m_pActorManager->GetTransform(transform_uid);

	actor_ci.materials.clear();
	actor_ci.materials.reserve(material_uids.size());
	for (avs::uid m_uid : material_uids)
	{
		actor_ci.materials.push_back(m_pActorManager->GetMaterial(m_uid));
	}

	avs::uid actor_uid = GenerateUid();
	m_pActorManager->CreateActor(actor_uid, &actor_ci);

	if(!m_pActorManager->m_Actors[actor_uid]->IsComplete())
	    SCR_COUT("Incomplete Actor: " << actor_uid << "created.");
}