// (C) Copyright 2018-2019 Simul Software Ltd
#include "ResourceCreator.h"

#include "Material.h"

#include <set>

using namespace avs;

ResourceCreator::ResourceCreator(basist::transcoder_texture_format transcoderTextureFormat)
	:basis_codeBook(basist::g_global_selector_cb_size, basist::g_global_selector_cb), basis_textureFormat(transcoderTextureFormat), basisThread(&ResourceCreator::BasisThread_TranscodeTextures, this)
{
	basist::basisu_transcoder_init();
}

ResourceCreator::~ResourceCreator()
{
	//Safely close the basis transcoding thread.
	shouldBeTranscoding = false;
	basisThread.join();
}

void ResourceCreator::Initialise(scr::RenderPlatform* r, scr::VertexBufferLayout::PackingStyle packingStyle)
{
	m_API.SetAPI(r->GetAPI());
	m_pRenderPlatform = r;

	assert(packingStyle == scr::VertexBufferLayout::PackingStyle::GROUPED || packingStyle == scr::VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_PackingStyle = packingStyle;

	//Setup Dummy textures.
	m_DummyDiffuse = m_pRenderPlatform->InstantiateTexture();
	m_DummyNormal = m_pRenderPlatform->InstantiateTexture();
	m_DummyCombined = m_pRenderPlatform->InstantiateTexture();

	scr::Texture::TextureCreateInfo tci =
	{
		1, 1, 1, 4, 1, 1,
		scr::Texture::Slot::UNKNOWN,
		scr::Texture::Type::TEXTURE_2D,
		scr::Texture::Format::RGBA8,
		scr::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
		{4},
		{0x00000000},
		scr::Texture::CompressionFormat::UNCOMPRESSED,
		false
	};

	uint32_t* diffuse = new uint32_t[1];
	*diffuse = diffuseBGRA;
	tci.mips[0] = (uint8_t*)diffuse;
	m_DummyDiffuse->Create(tci);

	uint32_t* normal = new uint32_t[1];
	*normal = normalBGRA;
	tci.mips[0] = (uint8_t*)normal;
	m_DummyNormal->Create(tci);

	uint32_t* combine = new uint32_t[1];
	*combine = combinedBGRA;
	tci.mips[0] = (uint8_t*)combine;
	m_DummyCombined->Create(tci);
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

std::vector<avs::uid> ResourceCreator::TakeReceivedResources()
{
	std::vector<avs::uid> receivedResources = std::move(m_ReceivedResources);
	m_ReceivedResources.clear();

	return receivedResources;
}

std::vector<avs::uid> ResourceCreator::TakeCompletedActors()
{
	std::vector<avs::uid> completedActors = std::move(m_CompletedActors);
	m_CompletedActors.clear();

	return completedActors;
}

void ResourceCreator::Update(uint32_t deltaTime)
{
	std::lock_guard<std::mutex> lock_texturesToCreate(mutex_texturesToCreate);
	
	//Complete any textures that have finished to transcode, and are waiting.
	//This has to happen on the main thread, so we can use the main GL context.
	for(auto texturePair = texturesToCreate.begin(); texturePair != texturesToCreate.end();)
	{
		CompleteTexture(texturePair->first, texturePair->second);
		texturePair = texturesToCreate.erase(texturePair);
	}
}

avs::Result ResourceCreator::Assemble(const avs::MeshCreate& meshCreate)
{
	m_ReceivedResources.push_back(meshCreate.mesh_uid);

	using namespace scr;

	if(m_VertexBufferManager->Has(meshCreate.mesh_uid) ||	m_IndexBufferManager->Has(meshCreate.mesh_uid))
		return avs::Result::OK;

	if (!m_pRenderPlatform)
	{
		SCR_CERR("No valid render platform was found.");
        return avs::Result::GeometryDecoder_ClientRendererError;
	}
	scr::Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.vb.resize(meshCreate.m_NumElements);
	mesh_ci.ib.resize(meshCreate.m_NumElements);

	for (size_t i = 0; i < meshCreate.m_NumElements; i++)
	{
		const MeshElementCreate &meshElementCreate = meshCreate.m_MeshElementCreate[i];

		std::shared_ptr<VertexBufferLayout> layout(new VertexBufferLayout);
		if (meshElementCreate.m_Vertices)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Normals || meshElementCreate.m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::NORMAL, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Tangents || meshElementCreate.m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TANGENT, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_UV0s)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_UV1s)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Colors)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Joints)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Weights)
		{
			layout->AddAttribute((uint32_t)AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		layout->CalculateStride();
		layout->m_PackingStyle = this->m_PackingStyle;

		size_t constructedVBSize = layout->m_Stride * meshElementCreate.m_VertexCount;
		size_t indicesSize = meshElementCreate.m_IndexCount * meshElementCreate.m_IndexSize;

		std::unique_ptr<float[]> constructedVB = std::make_unique<float[]>(constructedVBSize);
		std::unique_ptr<uint8_t[]> _indices = std::make_unique<uint8_t[]>(indicesSize);
		
		memcpy(_indices.get(), meshElementCreate.m_Indices, indicesSize);

		if (layout->m_PackingStyle == scr::VertexBufferLayout::PackingStyle::INTERLEAVED)
		{
			for (size_t j = 0; j < meshElementCreate.m_VertexCount; j++)
			{
				size_t intraStrideOffset = 0;
				if (meshElementCreate.m_Vertices)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Vertices + j, sizeof(avs::vec3)); intraStrideOffset += 3;
				}
				if (meshElementCreate.m_TangentNormals)
				{
					avs::vec3 normal;
					avs::vec4 tangent;
					char* nt = (char*)(meshElementCreate.m_TangentNormals + (meshElementCreate.m_TangentNormalSize * j));
					// tangentx tangentz
					if (meshElementCreate.m_TangentNormalSize == 8)
					{
						Vec4<signed char>& x8 = *((avs::Vec4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						Vec4<signed char>& n8 = *((avs::Vec4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						Vec4<short>& x8 = *((avs::Vec4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						Vec4<short>& n8 = *((avs::Vec4<short>*)(nt + 8));
						normal.x = float(n8.x) / 32767.0f;
						normal.y = float(n8.y) / 32767.0f;
						normal.z = float(n8.z) / 32767.0f;
					}
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, &normal, sizeof(avs::vec3));
					intraStrideOffset += 3;
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, &tangent, sizeof(avs::vec4));
					intraStrideOffset += 4;
				}
				else
				{
					if (meshElementCreate.m_Normals)
					{
						memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Normals + j, sizeof(avs::vec3));
						intraStrideOffset += 3;
					}
					if (meshElementCreate.m_Tangents)
					{
						memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Tangents + j, sizeof(avs::vec4));
						intraStrideOffset += 4;
					}
				}
				if (meshElementCreate.m_UV0s)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_UV0s + j, sizeof(avs::vec2));
					intraStrideOffset += 2;
				}
				if (meshElementCreate.m_UV1s)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_UV1s + j, sizeof(avs::vec2));
					intraStrideOffset += 2;
				}
				if (meshElementCreate.m_Colors)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Colors + j, sizeof(avs::vec4));
					intraStrideOffset += 4;
				}
				if (meshElementCreate.m_Joints)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Joints + j, sizeof(avs::vec4));
					intraStrideOffset += 4;
				}
				if (meshElementCreate.m_Weights)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Weights + j, sizeof(avs::vec4));
					intraStrideOffset += 4;
				}
			}
		}
		else if (layout->m_PackingStyle == scr::VertexBufferLayout::PackingStyle::GROUPED)
		{
			size_t vertexBufferOffset = 0;
			if (meshElementCreate.m_Vertices)
			{
				size_t size = sizeof(avs::vec3) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Vertices, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_TangentNormals)
			{
				for (size_t j = 0; j < meshElementCreate.m_VertexCount; j++)
				{
					avs::vec3 normal;
					avs::vec4 tangent;
					char* nt = (char*)(meshElementCreate.m_TangentNormals + (meshElementCreate.m_TangentNormalSize * j));
					// tangentx tangentz
					if (meshElementCreate.m_TangentNormalSize == 8)
					{
						Vec4<signed char>& x8 = *((avs::Vec4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						Vec4<signed char>& n8 = *((avs::Vec4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						Vec4<short>& x8 = *((avs::Vec4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						Vec4<short>& n8 = *((avs::Vec4<short>*)(nt + 8));
						normal.x = float(n8.x) / 32767.0f;
						normal.y = float(n8.y) / 32767.0f;
						normal.z = float(n8.z) / 32767.0f;
					}

					size_t size = sizeof(avs::vec3);
					assert(constructedVBSize >= vertexBufferOffset + size);
					memcpy(constructedVB.get() + vertexBufferOffset, &normal, size);
					vertexBufferOffset += size;

					size = sizeof(avs::vec4);
					assert(constructedVBSize >= vertexBufferOffset + size);
					memcpy(constructedVB.get() + vertexBufferOffset, &tangent, size);
					vertexBufferOffset += size;
				}
			}
			else
			{
				if (meshElementCreate.m_Normals)
				{
					size_t size = sizeof(avs::vec3) * meshElementCreate.m_VertexCount;
					assert(constructedVBSize >= vertexBufferOffset + size);
					memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Normals, size);
					vertexBufferOffset += size;
				}
				if (meshElementCreate.m_Tangents)
				{
					size_t size = sizeof(avs::vec4) * meshElementCreate.m_VertexCount;
					assert(constructedVBSize >= vertexBufferOffset + size);
					memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Tangents, size);
					vertexBufferOffset += size;
				}
			}
			if (meshElementCreate.m_UV0s)
			{
				size_t size = sizeof(avs::vec2) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_UV0s, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_UV1s)
			{
				size_t size = sizeof(avs::vec2) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_UV1s, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Colors)
			{
				size_t size = sizeof(avs::vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Colors, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Joints)
			{
				size_t size = sizeof(avs::vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Joints, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Weights)
			{
				size_t size = sizeof(avs::vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Weights, size);
				vertexBufferOffset += size;
			}
		}
		else
		{
			SCR_CERR("Unknown vertex buffer layout.");
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		if (constructedVBSize == 0 || constructedVB == nullptr || meshElementCreate.m_IndexCount == 0 || meshElementCreate.m_Indices == nullptr)
		{
			SCR_CERR("Unable to construct vertex and index buffers.");
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		std::shared_ptr<VertexBuffer> vb = m_pRenderPlatform->InstantiateVertexBuffer();
		VertexBuffer::VertexBufferCreateInfo vb_ci;
		vb_ci.layout = layout;
		vb_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
		vb_ci.vertexCount = meshElementCreate.m_VertexCount;
		vb_ci.size = constructedVBSize;
		vb_ci.data = (const void*)constructedVB.get();
		vb->Create(&vb_ci);

		std::shared_ptr<IndexBuffer> ib = m_pRenderPlatform->InstantiateIndexBuffer();
		IndexBuffer::IndexBufferCreateInfo ib_ci;
		ib_ci.usage = (BufferUsageBit)(STATIC_BIT | DRAW_BIT);
		ib_ci.indexCount = meshElementCreate.m_IndexCount;
		ib_ci.stride = meshElementCreate.m_IndexSize;
		ib_ci.data = _indices.get();
		ib->Create(&ib_ci);

		m_VertexBufferManager->Add(meshElementCreate.vb_uid, vb);
		m_IndexBufferManager->Add(meshElementCreate.ib_uid, ib);

		mesh_ci.vb[i] = vb;
		mesh_ci.ib[i] = ib;
	}
	if(!m_MeshManager->Has(meshCreate.mesh_uid))
	{
		CompleteMesh(meshCreate.mesh_uid, mesh_ci);
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
		case avs::TextureFormat::D16F: return scr::Texture::Format::DEPTH_COMPONENT16;
		case avs::TextureFormat::D24F: return scr::Texture::Format::DEPTH_COMPONENT24;
		case avs::TextureFormat::D32F: return scr::Texture::Format::DEPTH_COMPONENT32F;
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
		(texture.compression == avs::TextureCompression::BASIS_COMPRESSED) ? toSCRCompressionFormat(basis_textureFormat) : scr::Texture::CompressionFormat::UNCOMPRESSED
     };

	//Copy the data out of the buffer, so it can be transcoded or used as-is (uncompressed).
	unsigned char* data = new unsigned char[texture.dataSize];
	memcpy(data, texture.data, texture.dataSize);
	
	if (texture.compression == avs::TextureCompression::BASIS_COMPRESSED)
	{
		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		texturesToTranscode.emplace_back(UntranscodedTexture{texture_uid, texture.dataSize, data, std::move(texInfo), texture.name});
	}
	else
	{
		texInfo.mipSizes.push_back(texture.dataSize);
		texInfo.mips.push_back(data);

		CompleteTexture(texture_uid, texInfo);
	}

	m_ReceivedResources.push_back(texture_uid);
}

void ResourceCreator::passMaterial(avs::uid material_uid, const avs::Material & material)
{
	std::shared_ptr<IncompleteMaterial> newMaterial = std::make_shared<IncompleteMaterial>();
	//A list of unique resources that the material is missing, and needs to be completed.
	std::set<avs::uid> missingResources;

	newMaterial->materialInfo.renderPlatform = m_pRenderPlatform;
	newMaterial->materialInfo.diffuse.texture = nullptr;
	newMaterial->materialInfo.normal.texture = nullptr;
	newMaterial->materialInfo.combined.texture = nullptr;

	if (material.pbrMetallicRoughness.baseColorTexture.index != 0)
	{
		const std::shared_ptr<scr::Texture> diffuseTexture = m_TextureManager->Get(material.pbrMetallicRoughness.baseColorTexture.index);

		if (diffuseTexture)
		{
			newMaterial->materialInfo.diffuse.texture = diffuseTexture;
		}
		else
		{
			missingResources.insert(material.pbrMetallicRoughness.baseColorTexture.index);
			newMaterial->textureSlots.emplace(material.pbrMetallicRoughness.baseColorTexture.index, newMaterial->materialInfo.diffuse.texture);
		}

		scr::vec2 tiling = { material.pbrMetallicRoughness.baseColorTexture.tiling.x, material.pbrMetallicRoughness.baseColorTexture.tiling.y };

		newMaterial->materialInfo.diffuse.texCoordsScalar[0] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[1] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[2] = tiling;
		newMaterial->materialInfo.diffuse.texCoordsScalar[3] = tiling;

		newMaterial->materialInfo.diffuse.textureOutputScalar = material.pbrMetallicRoughness.baseColorFactor;

		newMaterial->materialInfo.diffuse.texCoordIndex = (float)material.pbrMetallicRoughness.baseColorTexture.texCoord;
	}
	else
	{
		newMaterial->materialInfo.diffuse.texture = m_DummyDiffuse; 
		newMaterial->materialInfo.diffuse.texCoordsScalar[0] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.diffuse.texCoordsScalar[1] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.diffuse.texCoordsScalar[2] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.diffuse.texCoordsScalar[3] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.diffuse.textureOutputScalar = scr::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		newMaterial->materialInfo.diffuse.texCoordIndex = 0.0f;
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
			missingResources.insert(material.normalTexture.index);
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
	else
	{
		newMaterial->materialInfo.normal.texture = m_DummyNormal;
		newMaterial->materialInfo.normal.texCoordsScalar[0] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.normal.texCoordsScalar[1] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.normal.texCoordsScalar[2] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.normal.texCoordsScalar[3] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.normal.textureOutputScalar = scr::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		newMaterial->materialInfo.normal.texCoordIndex = 0.0f;
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
			missingResources.insert(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
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
	else
	{
		newMaterial->materialInfo.combined.texture = m_DummyCombined;
		newMaterial->materialInfo.combined.texCoordsScalar[0] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.combined.texCoordsScalar[1] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.combined.texCoordsScalar[2] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.combined.texCoordsScalar[3] = scr::vec2(1.0f, 1.0f);
		newMaterial->materialInfo.combined.textureOutputScalar = scr::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		newMaterial->materialInfo.combined.texCoordIndex = 0.0f;
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

	m_ReceivedResources.push_back(material_uid);
}

void ResourceCreator::passNode(avs::uid node_uid, avs::DataNode& node)
{
	switch(node.data_type)
	{
		case NodeDataType::Hand:
			if(!m_pActorManager->HasActor(node_uid))
			{
				CreateActor(node_uid, node.data_uid, node.materials, node.transform, true);
			}
			break;
		case NodeDataType::Mesh:
			if(!m_pActorManager->UpdateActorTransform(node_uid, node.transform.position, node.transform.rotation, node.transform.scale))
			{
				CreateActor(node_uid, node.data_uid, node.materials, node.transform, false);
			}
			break;
		case NodeDataType::Camera:
			break;
		case NodeDataType::Scene:
			break;
		case NodeDataType::ShadowMap:
			CreateLight(node_uid, node);
			break;
		default:
			SCR_LOG("Unknown NodeDataType: %c", static_cast<int>(node.data_type));
			break;
	}

	m_ReceivedResources.push_back(node_uid);
}

void ResourceCreator::CreateActor(avs::uid node_uid, avs::uid mesh_uid, const std::vector<avs::uid> &material_uids, const avs::Transform &transform, bool isHand)
{
	std::shared_ptr<IncompleteActor> newActor = std::make_shared<IncompleteActor>();
	//A list of unique resources that the actor is missing, and needs to be completed.
	std::set<avs::uid> missingResources;

	newActor->actorInfo.staticMesh = true;
	newActor->actorInfo.animatedMesh = false;
	newActor->actorInfo.transform = transform;

	newActor->actorInfo.mesh = m_MeshManager->Get(mesh_uid);
	if(!newActor->actorInfo.mesh)
	{
		missingResources.insert(mesh_uid);
	}

	newActor->actorInfo.materials.resize(material_uids.size());
	for(size_t i = 0; i < material_uids.size(); i++)
	{
		std::shared_ptr<scr::Material> material = m_MaterialManager->Get(material_uids[i]);

		if(material)
		{
			newActor->actorInfo.materials[i] = material;
		}
		else
		{
			missingResources.insert(material_uids[i]);
			newActor->materialSlots[material_uids[i]].push_back(i);
		}
	}

	//Complete actor now, if we aren't missing any resources.
	if(missingResources.size() == 0)
	{
		CompleteActor(node_uid, newActor->actorInfo, isHand);
	}
	else
	{
		m_ResourceRequests.insert(std::end(m_ResourceRequests), std::begin(missingResources), std::end(missingResources));

		newActor->id = node_uid;

		for(avs::uid uid : missingResources)
		{
			m_WaitingForResources[uid].push_back(newActor);
		}

		newActor->isHand = isHand;
	}
}

void ResourceCreator::CreateLight(avs::uid node_uid, avs::DataNode& node)
{
	scr::Light::LightCreateInfo lci;
	lci.renderPlatform = m_pRenderPlatform;
	lci.type = scr::Light::Type::DIRECTIONAL;
	lci.position = scr::vec3(node.transform.position);
	lci.orientation = scr::quat(node.transform.rotation);
	lci.shadowMapTexture = m_TextureManager->Get(node.data_uid);

	std::shared_ptr<scr::Light> light = std::make_shared<scr::Light>(&lci);
	m_LightManager->Add(node_uid, light);
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
			CompleteActor(actorInfo.lock()->id, actorInfo.lock()->actorInfo, actorInfo.lock()->isHand);
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

		for(size_t materialIndex : actorInfo.lock()->materialSlots.at(material_uid))
		{
			actorInfo.lock()->actorInfo.materials[materialIndex] = material;
		}		

		//If only this material is pointing to the actor, then it is complete.
		if(it->use_count() == 1)
		{
			CompleteActor(actorInfo.lock()->id, actorInfo.lock()->actorInfo, actorInfo.lock()->isHand);
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_WaitingForResources.erase(material_uid);
}

void ResourceCreator::CompleteActor(avs::uid actor_uid, const scr::Actor::ActorCreateInfo& actorInfo, bool isHand)
{
	///We're using the node ID as the actor ID as we are currently generating an actor per node/transform anyway; this way the server can tell the client to remove an actor.
	if(isHand) m_pActorManager->CreateHand(actor_uid, actorInfo);
	else m_pActorManager->CreateActor(actor_uid, actorInfo);
	m_CompletedActors.push_back(actor_uid);
}

void ResourceCreator::BasisThread_TranscodeTextures()
{
	while(shouldBeTranscoding)
	{
		std::this_thread::yield(); //Yield at the start, as we don't want to yield before we unlock (when lock goes out of scope).

		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		if(texturesToTranscode.size() != 0)
		{
			UntranscodedTexture& transcoding = texturesToTranscode[0];

			//We need a new transcoder for every .basis file.
			basist::basisu_transcoder basis_transcoder(&basis_codeBook);

			if(basis_transcoder.start_transcoding(transcoding.data, transcoding.dataSize))
			{
				transcoding.scrTexture.mipCount = basis_transcoder.get_total_image_levels(transcoding.data, transcoding.dataSize, 0);
				transcoding.scrTexture.mipSizes.reserve(transcoding.scrTexture.mipCount);
				transcoding.scrTexture.mips.reserve(transcoding.scrTexture.mipCount);

				for(uint32_t mipIndex = 0; mipIndex < transcoding.scrTexture.mipCount; mipIndex++)
				{
					uint32_t basisWidth, basisHeight, basisBlocks;

					basis_transcoder.get_image_level_desc(transcoding.data, transcoding.dataSize, 0, mipIndex, basisWidth, basisHeight, basisBlocks);
					uint32_t outDataSize = basist::basis_get_bytes_per_block(basis_textureFormat) * basisBlocks;

					unsigned char* outData = new unsigned char[outDataSize];
					if(basis_transcoder.transcode_image_level(transcoding.data, transcoding.dataSize, 0, mipIndex, outData, basisBlocks, basis_textureFormat))
					{
						transcoding.scrTexture.mipSizes.push_back(outDataSize);
						transcoding.scrTexture.mips.push_back(outData);
					}
					else
					{
						SCR_COUT("Texture \"" + transcoding.name + "\" failed to transcode mipmap level " + std::to_string(mipIndex) + ".");
						delete[] outData;
					}
				}

				if(transcoding.scrTexture.mips.size() != 0)
				{
					std::lock_guard<std::mutex> lock_texturesToCreate(mutex_texturesToCreate);
					texturesToCreate.emplace(std::pair{transcoding.texture_uid, std::move(transcoding.scrTexture)});
				}
				else
				{
					SCR_COUT("Texture \"" + transcoding.name + "\" failed to transcode, but was a valid basis file.");
				}

				delete[] transcoding.data;
			}
			else
			{
				SCR_COUT("Texture \"" + transcoding.name + "\" failed to start transcoding.");
			}

			texturesToTranscode.erase(texturesToTranscode.begin());
		}
	}
}
