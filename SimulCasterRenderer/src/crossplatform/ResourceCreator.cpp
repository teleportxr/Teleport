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

std::vector<avs::uid> ResourceCreator::TakeResourceRequests()
{
	std::vector<avs::uid> resourceRequests = std::move(m_ResourceRequests);
	m_ResourceRequests.clear();

	//Remove duplicates.
	std::sort(resourceRequests.begin(), resourceRequests.end());
	resourceRequests.erase(std::unique(resourceRequests.begin(), resourceRequests.end()), resourceRequests.end());

	return resourceRequests;
}

avs::Result ResourceCreator::Assemble(avs::MeshCreate * meshCreate)
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
		if (meshElementCreate->m_Vertices)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
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

		m_VertexBufferManager->Add(meshElementCreate->vb_uid, vb);
		m_IndexBufferManager->Add(meshElementCreate->ib_uid, ib);

		mesh_ci.vb[i] = vb;
		mesh_ci.ib[i] = ib;
	}
	if(!m_MeshManager->Has(meshCreate->mesh_uid))
	{
		CompleteMesh(meshCreate->mesh_uid, mesh_ci);
	}

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
		{},
		{},
		toSCRCompressionFormat(basis_textureFormat)
     };
   
	//We need a new transcoder for every .basis file.
	basist::basisu_transcoder basis_transcoder(&basis_codeBook);

	if(basis_transcoder.start_transcoding(texture.data, texture.dataSize))
	{
		texInfo.mipCount = basis_transcoder.get_total_image_levels(texture.data, texture.dataSize, 0);

		for(uint32_t mipIndex = 0; mipIndex < texInfo.mipCount; mipIndex++)
		{
			uint32_t basisWidth, basisHeight, basisBlocks;

			basis_transcoder.get_image_level_desc(texture.data, texture.dataSize, 0, mipIndex, basisWidth, basisHeight, basisBlocks);
			uint32_t outDataSize = basist::basis_get_bytes_per_block(basis_textureFormat) * basisBlocks;

			unsigned char* outData = new unsigned char[outDataSize];
			if(basis_transcoder.transcode_image_level(texture.data, texture.dataSize, 0, mipIndex, outData, basisBlocks, basis_textureFormat))
			{
				texInfo.mipSizes.push_back(outDataSize);
				texInfo.mips.push_back(outData);
			}
			else
			{
				delete[] outData;
			}
		}

		delete[] texture.data;
	}

	//The data is uncompressed if we failed to transcode it.
	if(texInfo.mips.size() == 0)
	{
		texInfo.mipSizes.push_back(texture.dataSize);
		texInfo.mips.push_back(texture.data);
	}

	CompleteTexture(texture_uid, texInfo);
}

///Most of these sets need actual values, rather than default initalisers.
void ResourceCreator::passMaterial(avs::uid material_uid, const avs::Material & material)
{
	std::shared_ptr<IncompleteMaterial> newMaterial = std::make_shared<IncompleteMaterial>();
	std::vector<avs::uid> missingResources;

	newMaterial->materialInfo.renderPlatform = m_pRenderPlatform;
	newMaterial->materialInfo.diffuse.texture = nullptr;
	newMaterial->materialInfo.normal.texture = nullptr;
	newMaterial->materialInfo.combined.texture = nullptr;

	if(material.pbrMetallicRoughness.baseColorTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> diffuseTexture = m_TextureManager->Get(material.pbrMetallicRoughness.baseColorTexture.index);

		if(diffuseTexture)
		{
			newMaterial->materialInfo.diffuse.texture = diffuseTexture;
		}
		else
		{
			missingResources.push_back(material.pbrMetallicRoughness.baseColorTexture.index);
			newMaterial->textureSlots.emplace(material.pbrMetallicRoughness.baseColorTexture.index, newMaterial->materialInfo.diffuse.texture);
		}

			scr::vec2 tiling = {material.pbrMetallicRoughness.baseColorTexture.tiling.x, material.pbrMetallicRoughness.baseColorTexture.tiling.y};

		newMaterial->materialInfo.diffuse.texCoordsScalar[0] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[1] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[2] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[3] = tiling;

		newMaterial->materialInfo.diffuse.textureOutputScalar = material.pbrMetallicRoughness.baseColorFactor;

		newMaterial->materialInfo.diffuse.texCoordIndex = (float)material.pbrMetallicRoughness.baseColorTexture.texCoord;
		}

	if(material.normalTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> normalTexture = m_TextureManager->Get(material.normalTexture.index);

		if(normalTexture)
		{
			newMaterial->materialInfo.normal.texture = normalTexture;
		}
		else
		{
			missingResources.push_back(material.normalTexture.index);
			newMaterial->textureSlots.emplace(material.normalTexture.index, newMaterial->materialInfo.normal.texture);
		}

			scr::vec2 tiling = {material.normalTexture.tiling.x, material.normalTexture.tiling.y};

		newMaterial->materialInfo.normal.texCoordsScalar[0] = tiling;
		newMaterial->materialInfo.normal.texCoordsScalar[1] = tiling;
		newMaterial->materialInfo.normal.texCoordsScalar[2] = tiling;
		newMaterial->materialInfo.normal.texCoordsScalar[3] = tiling;

		newMaterial->materialInfo.normal.textureOutputScalar = scr::vec4{1, 1, 1, 1};
		newMaterial->materialInfo.normal.texCoordIndex = (float)material.normalTexture.texCoord;
		}

	if(material.pbrMetallicRoughness.metallicRoughnessTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> metallicRoughnessTexture = m_TextureManager->Get(material.pbrMetallicRoughness.metallicRoughnessTexture.index);

		if(metallicRoughnessTexture)
		{
			newMaterial->materialInfo.combined.texture = metallicRoughnessTexture;
		}
		else
		{
			missingResources.push_back(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
			newMaterial->textureSlots.emplace(material.pbrMetallicRoughness.metallicRoughnessTexture.index, newMaterial->materialInfo.combined.texture);
		}

			scr::vec2 tiling = {material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x, material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y};

		newMaterial->materialInfo.combined.texCoordsScalar[0] = tiling;
		newMaterial->materialInfo.combined.texCoordsScalar[1] = tiling;
		newMaterial->materialInfo.combined.texCoordsScalar[2] = tiling;
		newMaterial->materialInfo.combined.texCoordsScalar[3] = tiling;

		newMaterial->materialInfo.combined.textureOutputScalar = scr::vec4{1, 1, 1, 1};

		newMaterial->materialInfo.combined.texCoordIndex = (float)material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;
		}

	///This needs an actual value.
	newMaterial->materialInfo.effect = nullptr;

	if(missingResources.size() == 0)
	{
		CompleteMaterial(material_uid, newMaterial->materialInfo);
	}
	else
	{
		m_ResourceRequests.insert(std::end(m_ResourceRequests), std::begin(missingResources), std::end(missingResources));

		newMaterial->id = material_uid;
		for(avs::uid uid : missingResources)
		{
			m_WaitingForResources[uid].push_back(newMaterial);
	}
	}
}

void ResourceCreator::passNode(avs::uid node_uid, avs::DataNode& node)
	{
	if (m_pActorManager->HasActor(node_uid)) //Check the actor has already been added, if so update transform.
	{
		m_pActorManager->GetActor(node_uid)->GetTransform().UpdateModelMatrix(node.transform.position, node.transform.rotation, node.transform.scale);
	}
	else
	{
	    switch (node.data_type)
	    {
	    case NodeDataType::Mesh:
	    	{
	    		CreateActor(node_uid, node.data_uid,node.materials, avs::Transform{node.transform.position, node.transform.rotation, node.transform.scale});
	    		return;
	    	}
	    case NodeDataType::Camera:
	    	return;
	    case NodeDataType::Scene:
	    	return;
	    }
	}
}

void ResourceCreator::CreateActor(avs::uid node_uid, avs::uid mesh_uid, const std::vector<avs::uid> &material_uids, avs::Transform &&transform)
{
	std::shared_ptr<IncompleteActor> newActor = std::make_shared<IncompleteActor>();

	std::vector<avs::uid> missingResources;

	newActor->actorInfo.staticMesh = true;
	newActor->actorInfo.animatedMesh = false;
	newActor->actorInfo.transform = transform;

	newActor->actorInfo.mesh = m_MeshManager->Get(mesh_uid);
	if(!newActor->actorInfo.mesh)
	{
		missingResources.push_back(mesh_uid);
	}

	newActor->actorInfo.materials.reserve(material_uids.size());
	for (avs::uid m_uid : material_uids)
	{
		std::shared_ptr<scr::Material> material = m_MaterialManager->Get(m_uid);

		if(material)
		{
			newActor->actorInfo.materials.push_back(material);
		}
		else
		{
			missingResources.push_back(m_uid);
		}
	}

	//Complete actor now, if we aren't missing any resources.
	if(missingResources.size() == 0)
	{
		CompleteActor(node_uid, newActor->actorInfo);
	}
	else
	{
		m_ResourceRequests.insert(std::end(m_ResourceRequests), std::begin(missingResources), std::end(missingResources));

		newActor->id = node_uid;
		for(avs::uid uid : missingResources)
		{
			m_WaitingForResources[uid].push_back(newActor);
		}
	}
}

void ResourceCreator::CompleteMesh(avs::uid mesh_uid, const scr::Mesh::MeshCreateInfo& meshInfo)
{
	std::shared_ptr<scr::Mesh> mesh = std::make_shared<scr::Mesh>(meshInfo);
	m_MeshManager->Add(mesh_uid, mesh);

	//Add mesh to actors waiting for mesh.
	for(auto it = m_WaitingForResources[mesh_uid].begin(); it != m_WaitingForResources[mesh_uid].end(); it++)
	{
		std::weak_ptr<IncompleteActor> actorInfo = std::static_pointer_cast<IncompleteActor>(*it);

		actorInfo.lock()->actorInfo.mesh = mesh;

		//If only this mesh is pointing to the actor, then it is complete.
		if(it->use_count() == 1)
		{
			CompleteActor(actorInfo.lock()->id, actorInfo.lock()->actorInfo);
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_WaitingForResources.erase(mesh_uid);
}

void ResourceCreator::CompleteTexture(avs::uid texture_uid, const scr::Texture::TextureCreateInfo& textureInfo)
{
	std::shared_ptr<scr::Texture> scrTexture = m_pRenderPlatform->InstantiateTexture();
	scrTexture->Create(textureInfo);

	m_TextureManager->Add(texture_uid, scrTexture);

	//Add texture to materials waiting for texture.
	for(auto it = m_WaitingForResources[texture_uid].begin(); it != m_WaitingForResources[texture_uid].end(); it++)
	{
		std::weak_ptr<IncompleteMaterial> materialInfo = std::static_pointer_cast<IncompleteMaterial>(*it);

		materialInfo.lock()->textureSlots.at(texture_uid) = scrTexture;

		//If only this texture is pointing to the material, then it is complete.
		if(it->use_count() == 1)
		{
			CompleteMaterial(materialInfo.lock()->id, materialInfo.lock()->materialInfo);
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_WaitingForResources.erase(texture_uid);
}

void ResourceCreator::CompleteMaterial(avs::uid material_uid, const scr::Material::MaterialCreateInfo& materialInfo)
{
	std::shared_ptr<scr::Material> material = std::make_shared<scr::Material>(materialInfo);
	m_MaterialManager->Add(material_uid, material);

	//Add material to actors waiting for material.
	for(auto it = m_WaitingForResources[material_uid].begin(); it != m_WaitingForResources[material_uid].end(); it++)
	{
		const std::weak_ptr<IncompleteActor>& actorInfo = std::static_pointer_cast<IncompleteActor>(*it);

		actorInfo.lock()->actorInfo.materials.push_back(material);

		//If only this material is pointing to the actor, then it is complete.
		if(it->use_count() == 1)
		{
			CompleteActor(actorInfo.lock()->id, actorInfo.lock()->actorInfo);
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_WaitingForResources.erase(material_uid);
}

void ResourceCreator::CompleteActor(avs::uid actor_uid, const scr::Actor::ActorCreateInfo& actorInfo)
{
	///We're using the node_uid as the actor_uid as we are currently generating an actor per node/transform anyway; this way the server can tell the client to remove an actor.
	m_pActorManager->CreateActor(actor_uid, actorInfo);
}
