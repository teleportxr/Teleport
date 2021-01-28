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
	m_DummyEmissive = m_pRenderPlatform->InstantiateTexture();

	scr::Texture::TextureCreateInfo tci =
	{
		"Dummy Texture",
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
	tci.mips[0] = reinterpret_cast<uint8_t*>(diffuse);
	m_DummyDiffuse->Create(tci);

	uint32_t* normal = new uint32_t[1];
	*normal = normalRGBA;
	tci.mips[0] = reinterpret_cast<uint8_t*>(normal);
	m_DummyNormal->Create(tci);

	uint32_t* combine = new uint32_t[1];
	*combine = combinedBGRA;
	tci.mips[0] = reinterpret_cast<uint8_t*>(combine);
	m_DummyCombined->Create(tci);

	uint32_t* emissive = new uint32_t[1];
	*emissive = emissiveBGRA;
	tci.mips[0] = reinterpret_cast<uint8_t*>(emissive);
	m_DummyEmissive->Create(tci);
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

void ResourceCreator::Clear()
{
	mutex_texturesToCreate.lock();
	texturesToCreate;
	mutex_texturesToCreate.unlock();

	mutex_texturesToTranscode.lock();
	texturesToTranscode;
	mutex_texturesToTranscode.unlock();

	m_ResourceRequests.clear();
	m_ReceivedResources.clear();
	m_CompletedActors.clear();
	m_MissingResources.clear();
}

void ResourceCreator::Update(float deltaTime)
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

avs::Result ResourceCreator::Assemble(avs::MeshCreate& meshCreate)
{
	m_ReceivedResources.push_back(meshCreate.mesh_uid);
	SCR_COUT << "Assemble(Mesh" << meshCreate.mesh_uid << ": " << meshCreate.name << ")\n";

	using namespace scr;

	if(m_VertexBufferManager->Has(meshCreate.mesh_uid) ||	m_IndexBufferManager->Has(meshCreate.mesh_uid))
		return avs::Result::OK;

	if (!m_pRenderPlatform)
	{
		SCR_CERR<<"No valid render platform was found."<<std::endl;
        return avs::Result::GeometryDecoder_ClientRendererError;
	}
	scr::Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.name = meshCreate.name;
	mesh_ci.vb.resize(meshCreate.m_NumElements);
	mesh_ci.ib.resize(meshCreate.m_NumElements);

	for (size_t i = 0; i < meshCreate.m_NumElements; i++)
	{
		MeshElementCreate &meshElementCreate = meshCreate.m_MeshElementCreate[i];

		//We have to pad the UV1s, if we are missing UV1s but have joints and weights; we use a vector so it will clean itself up.
		std::vector<avs::vec2> paddedUV1s(meshElementCreate.m_VertexCount);
		if(!meshElementCreate.m_UV1s && (meshElementCreate.m_Joints || meshElementCreate.m_Weights))
		{
			meshElementCreate.m_UV1s = paddedUV1s.data();
		}

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
			SCR_CERR<<"Unknown vertex buffer layout."<<std::endl;
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		if (constructedVBSize == 0 || constructedVB == nullptr || meshElementCreate.m_IndexCount == 0 || meshElementCreate.m_Indices == nullptr)
		{
			SCR_CERR<<"Unable to construct vertex and index buffers."<<std::endl;
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
		case basist::transcoder_texture_format::cTFPVRTC1_4_RGBA: return scr::Texture::CompressionFormat::PVRTC1_4_OPAQUE_ONLY;
		case basist::transcoder_texture_format::cTFBC7_M6_OPAQUE_ONLY: return scr::Texture::CompressionFormat::BC7_M6_OPAQUE_ONLY;
		case basist::transcoder_texture_format::cTFTotalTextureFormats: return scr::Texture::CompressionFormat::UNCOMPRESSED;
		default:
			exit(1);
	}
}

void ResourceCreator::CreateTexture(avs::uid texture_uid, const avs::Texture& texture)
{
	SCR_COUT << "CreateTexture(" << texture_uid << ", " << texture.name << ")\n";
	m_ReceivedResources.push_back(texture_uid);

	scr::Texture::TextureCreateInfo texInfo =
	{
		texture.name,
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

	if(texture.compression == avs::TextureCompression::BASIS_COMPRESSED)
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
}

void ResourceCreator::CreateMaterial(avs::uid material_uid, const avs::Material& material)
{
	SCR_COUT << "CreateMaterial(" << material_uid << ", " << material.name << ")\n";
	m_ReceivedResources.push_back(material_uid);

	std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::make_shared<IncompleteMaterial>(material_uid, avs::GeometryPayloadType::Material);
	//A list of unique resources that the material is missing, and needs to be completed.
	std::set<avs::uid> missingResources;

	incompleteMaterial->materialInfo.name = material.name;
	incompleteMaterial->materialInfo.renderPlatform = m_pRenderPlatform;

	//Colour/Albedo/Diffuse
	AddTextureToMaterial(material.pbrMetallicRoughness.baseColorTexture,
						 material.pbrMetallicRoughness.baseColorFactor,
						 m_DummyDiffuse,
						 incompleteMaterial,
						 incompleteMaterial->materialInfo.diffuse);

	//Normal
	AddTextureToMaterial(material.normalTexture,
						 avs::vec4{material.normalTexture.scale, 1, 1, 1},
						 m_DummyNormal,
						 incompleteMaterial,
						 incompleteMaterial->materialInfo.normal);

	//Combined
	float rough = material.pbrMetallicRoughness.roughnessFactor;
	float rough_or_smoothness = material.pbrMetallicRoughness.roughnessMode == RoughnessMode::MULTIPLY_REVERSE ? 1.0f : 0.0f;
	AddTextureToMaterial(material.pbrMetallicRoughness.metallicRoughnessTexture,
						 avs::vec4{rough, material.pbrMetallicRoughness.metallicFactor, material.occlusionTexture.strength, rough_or_smoothness},
						 m_DummyCombined,
						 incompleteMaterial,
						 incompleteMaterial->materialInfo.combined);

	//Emissive
	AddTextureToMaterial(material.emissiveTexture,
						 avs::vec4(material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, 1.0f),
						 m_DummyEmissive,
						 incompleteMaterial,
						 incompleteMaterial->materialInfo.emissive);

	if(incompleteMaterial->textureSlots.size() == 0)
	{
		CompleteMaterial(material_uid, incompleteMaterial->materialInfo);
	}
}

void ResourceCreator::CreateNode(avs::uid node_uid, avs::DataNode& node)
{
	m_ReceivedResources.push_back(node_uid);

	switch(node.data_type)
	{
		case NodeDataType::Mesh:
			// TODO: WHY do we only update the transform if the type is a mesh..?
			if(!m_pActorManager->UpdateActorTransform(node_uid, node.transform.position, node.transform.rotation, node.transform.scale))
			{
				CreateActor(node_uid, node);
			}
			break;
		case NodeDataType::Camera:
			break;
		case NodeDataType::Scene:
			break;
		case NodeDataType::ShadowMap:
		case NodeDataType::Light:
			CreateLight(node_uid, node);
			break;
		case NodeDataType::Bone:
			CreateBone(node_uid, node);
			break;
		default:
			SCR_LOG("Unknown NodeDataType: %c", static_cast<int>(node.data_type));
			break;
	}
}

void ResourceCreator::CreateSkin(avs::uid skinID, avs::Skin& skin)
{
	SCR_COUT << "CreateSkin(" << skinID << ", " << skin.name << ")\n";
	m_ReceivedResources.push_back(skinID);

	std::shared_ptr<IncompleteSkin> incompleteSkin = std::make_shared<IncompleteSkin>(skinID, avs::GeometryPayloadType::Skin);

	scr::Transform::TransformCreateInfo transformCreateInfo;
	transformCreateInfo.renderPlatform = m_pRenderPlatform;

	//Convert avs::Mat4x4 to scr::Transform.
	std::vector<scr::Transform> inverseBindMatrices;
	inverseBindMatrices.reserve(skin.inverseBindMatrices.size());
	for(const Mat4x4& matrix : skin.inverseBindMatrices)
	{
		inverseBindMatrices.push_back(scr::Transform(transformCreateInfo, static_cast<scr::mat4>(matrix)));
	}

	//Create skin.
	incompleteSkin->skin = m_pRenderPlatform->InstantiateSkin(skin.name, inverseBindMatrices, skin.jointIDs.size(), skin.skinTransform);

	//Add bones we have.
	for(size_t i = 0; i < skin.jointIDs.size(); i++)
	{
		avs::uid jointID = skin.jointIDs[i];
		std::shared_ptr<scr::Bone> bone = m_BoneManager->Get(jointID);

		if(bone) incompleteSkin->skin->SetBone(i, bone);
		else
		{
			SCR_COUT << "Skin_" << skinID << "(" << incompleteSkin->skin->name << ") missing Bone_" << jointID << std::endl;
			incompleteSkin->missingBones[jointID] = i;
			m_ResourceRequests.push_back(jointID);
			GetMissingResource(jointID, "Bone").waitingResources.push_back(incompleteSkin);
		}
	}

	if(incompleteSkin->missingBones.size() == 0)
	{
		CompleteSkin(skinID, incompleteSkin);
	}
}

void ResourceCreator::CreateAnimation(avs::uid id, avs::Animation& animation)
{
	SCR_COUT << "CreateAnimation(" << id << ", " << animation.name << ")\n";
	m_ReceivedResources.push_back(id);

	std::shared_ptr<IncompleteAnimation> incompleteAnimation = std::make_shared<IncompleteAnimation>(id, avs::GeometryPayloadType::Animation);
	incompleteAnimation->animation = std::make_shared<scr::Animation>(animation.name);

	for(size_t i = 0; i < animation.boneKeyframes.size(); i++)
	{
		const avs::TransformKeyframe& avsKeyframes = animation.boneKeyframes[i];

		scr::BoneKeyframe boneKeyframes;
		boneKeyframes.positionKeyframes = avsKeyframes.positionKeyframes;
		boneKeyframes.rotationKeyframes = avsKeyframes.rotationKeyframes;

		std::shared_ptr<scr::Bone> bone = m_BoneManager->Get(avsKeyframes.nodeID);
		if(bone) boneKeyframes.bonePtr = bone;
		else
		{
			SCR_COUT << "Animation_" << id << "(" << animation.name << ") missing Bone_" << avsKeyframes.nodeID << std::endl;
			incompleteAnimation->missingBones[avsKeyframes.nodeID] = i;
			m_ResourceRequests.push_back(avsKeyframes.nodeID);
			GetMissingResource(avsKeyframes.nodeID, "Bone").waitingResources.push_back(incompleteAnimation);
		}

		incompleteAnimation->animation->boneKeyframes.push_back(boneKeyframes);
	}

	if(incompleteAnimation->missingBones.size() == 0)
	{
		CompleteAnimation(id, incompleteAnimation);
	}
}

//TODO: Rename Actor to MeshNode.
void ResourceCreator::CreateActor(avs::uid node_uid, avs::DataNode& node)
{
	if(m_pActorManager->HasActor(node_uid))
	{
		SCR_CERR << "CreateActor(" << node_uid << ", " << node.name << "). Already created!\n";
		return;
	}
	SCR_COUT << "CreateActor(" << node_uid << ", " << node.name << ")\n";

	bool isHand = node.data_subtype == NodeDataSubtype::LeftHand || node.data_subtype == NodeDataSubtype::RightHand;

	std::shared_ptr<IncompleteActor> newActor = std::make_shared<IncompleteActor>(node_uid, avs::GeometryPayloadType::Node, isHand);
	//Whether the actor is missing any resource before, and must wait for them before it can be completed.
	bool isMissingResources = false;

	newActor->actor = m_pActorManager->CreateActor(node_uid, node.name);
	newActor->actor->SetLocalTransform(static_cast<scr::Transform>(node.transform));

	newActor->actor->SetMesh(m_MeshManager->Get(node.data_uid));
	if(!newActor->actor->GetMesh())
	{
		SCR_COUT << "MeshNode_" << node_uid << "(" << node.name << ") missing Mesh_" << node.data_uid << std::endl;

		isMissingResources = true;
		m_ResourceRequests.push_back(node.data_uid);
		GetMissingResource(node.data_uid, "Mesh").waitingResources.push_back(newActor);
	}
	else
	{
		SCR_COUT << "\thas mesh "<<node.data_uid<<"(" << newActor->actor->GetMesh()->GetMeshCreateInfo().name.c_str() << ")\n";
	}

	if(node.skinID != 0)
	{
		newActor->actor->SetSkin(m_SkinManager->Get(node.skinID));
		if(!newActor->actor->GetSkin())
		{
			SCR_COUT << "MeshNode_" << node_uid << "(" << node.name << ") missing Skin_" << node.skinID << std::endl;

			isMissingResources = true;
			m_ResourceRequests.push_back(node.skinID);
			GetMissingResource(node.skinID, "Skin").waitingResources.push_back(newActor);
		}
	}

	for(size_t i = 0; i < node.animations.size(); i++)
	{
		avs::uid animationID = node.animations[i];
		std::shared_ptr<scr::Animation> animation = m_AnimationManager->Get(animationID);

		if(animation)
		{
			newActor->actor->animationComponent.AddAnimation(animationID, animation);
		}
		else
		{
			SCR_COUT << "MeshNode_" << node_uid << "(" << node.name << ") missing Animation_" << animationID << std::endl;

			isMissingResources = true;
			m_ResourceRequests.push_back(animationID);
			GetMissingResource(animationID, "Animation").waitingResources.push_back(newActor);
		}
	}

	if(m_pRenderPlatform->placeholderMaterial== nullptr)
	{
		scr::Material::MaterialCreateInfo materialCreateInfo;
		materialCreateInfo.renderPlatform = m_pRenderPlatform;
		materialCreateInfo.diffuse.texture=m_DummyDiffuse;
		materialCreateInfo.combined.texture=m_DummyCombined;
		materialCreateInfo.normal.texture=m_DummyNormal;
		materialCreateInfo.emissive.texture=m_DummyEmissive;
		m_pRenderPlatform->placeholderMaterial = std::make_shared<scr::Material>(materialCreateInfo);
	}

	newActor->actor->SetMaterialListSize(node.materials.size());
	for(size_t i = 0; i < node.materials.size(); i++)
	{
		std::shared_ptr<scr::Material> material = m_MaterialManager->Get(node.materials[i]);

		if(material)
		{
			newActor->actor->SetMaterial(i, material);
		}
		else
		{
			newActor->actor->SetMaterial(i, m_pRenderPlatform->placeholderMaterial);

			SCR_COUT << "MeshNode_" << node_uid << "(" << node.name << ") missing Material_" << node.materials[i] << std::endl;

			isMissingResources = true;
			m_ResourceRequests.push_back(node.materials[i]);
			GetMissingResource(node.materials[i], "Material").waitingResources.push_back(newActor);
			newActor->materialSlots[node.materials[i]].push_back(i);
		}
	}

	newActor->actor->SetChildrenIDs(node.childrenIDs);

	//Create MeshNode even if it is missing resources, but create a hand if it is a hand.
	m_pActorManager->AddActor(newActor->actor, node);

	//Complete actor now, if we aren't missing any resources.
	if(!isMissingResources)
	{
		CompleteActor(node_uid, newActor->actor, isHand);
	}
}

void ResourceCreator::CreateLight(avs::uid node_uid, avs::DataNode& node)
{
	SCR_COUT << "CreateLight(" << node_uid << ", " << node.name << ")\n";
	m_ReceivedResources.push_back(node_uid);

	scr::Light::LightCreateInfo lci;
	lci.renderPlatform = m_pRenderPlatform;
	lci.type = (scr::Light::Type)node.lightType;
	lci.position = avs::vec3(node.transform.position);
	lci.direction=node.lightDirection;
	lci.orientation = scr::quat(node.transform.rotation);
	lci.shadowMapTexture = m_TextureManager->Get(node.data_uid);
	lci.lightColour=node.lightColour;
	lci.lightRadius=node.lightRadius;
	lci.uid=node_uid;
	std::shared_ptr<scr::Light> light = std::make_shared<scr::Light>(&lci);
	m_LightManager->Add(node_uid, light);
}

void ResourceCreator::CreateBone(avs::uid boneID, avs::DataNode& node)
{
	SCR_COUT << "CreateBone(" << boneID << ", " << node.name << ")\n";
	m_ReceivedResources.push_back(boneID);

	std::shared_ptr<scr::Bone> bone = std::make_shared<scr::Bone>(boneID, node.name);
	bone->SetLocalTransform(node.transform);

	//Link to parent and child bones.
	//We don't know what order the bones will arrive in, so we have to do it for both orders(parent -> child, child -> parent).
	std::shared_ptr<scr::Bone> parent = m_BoneManager->Get(node.parentID);
	if(parent)
	{
		bone->SetParent(parent);
		parent->AddChild(bone);
	}

	for(avs::uid childID : node.childrenIDs)
	{
		std::shared_ptr<scr::Bone> child = m_BoneManager->Get(childID);
		if(child)
		{
			child->SetParent(bone);
			bone->AddChild(child);
		}
	}

	CompleteBone(boneID, bone);
}

void ResourceCreator::CompleteMesh(avs::uid mesh_uid, const scr::Mesh::MeshCreateInfo& meshInfo)
{
	SCR_COUT << "CompleteMesh(" << mesh_uid << ", " << meshInfo.name << ")\n";

	std::shared_ptr<scr::Mesh> mesh = std::make_shared<scr::Mesh>(meshInfo);
	m_MeshManager->Add(mesh_uid, mesh);

	//Add mesh to actors waiting for mesh.
	MissingResource& missingMesh = GetMissingResource(mesh_uid, "Mesh");
	for(auto it = missingMesh.waitingResources.begin(); it != missingMesh.waitingResources.end(); it++)
	{
		std::shared_ptr<IncompleteActor> incompleteActor = std::static_pointer_cast<IncompleteActor>(*it);

		incompleteActor->actor->SetMesh(mesh);

		//If only this mesh and this function are pointing to the actor, then it is complete.
		if(it->use_count() == 2) CompleteActor(incompleteActor->id, incompleteActor->actor, incompleteActor->isHand);
		else SCR_COUT << "Waiting MeshNode_" << incompleteActor->id << "(" << incompleteActor->actor->name << ") got Mesh_" << mesh_uid << "(" << meshInfo.name << ")\n";
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(mesh_uid);
}

void ResourceCreator::CompleteSkin(avs::uid skinID, std::shared_ptr<IncompleteSkin> completeSkin)
{
	SCR_COUT << "CompleteSkin(" << skinID << ", " << completeSkin->skin->name << ")\n";

	m_SkinManager->Add(skinID, completeSkin->skin);

	//Add skin to actors waiting for skin.
	MissingResource& missingSkin = GetMissingResource(skinID, "Skin");
	for(auto it = missingSkin.waitingResources.begin(); it != missingSkin.waitingResources.end(); it++)
	{
		std::shared_ptr<IncompleteActor> incompleteActor = std::static_pointer_cast<IncompleteActor>(*it);
		incompleteActor->actor->SetSkin(completeSkin->skin);

		//If only this resource and this skin are pointing to the actor, then it is complete.
		if(it->use_count() == 2) CompleteActor(incompleteActor->id, incompleteActor->actor, incompleteActor->isHand);
		else SCR_COUT << "Waiting MeshNode_" << incompleteActor->id << "(" << incompleteActor->actor->name << ") got Skin_" << skinID << "(" << completeSkin->skin->name << ")\n";
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(skinID);
}

void ResourceCreator::CompleteTexture(avs::uid texture_uid, const scr::Texture::TextureCreateInfo& textureInfo)
{
	SCR_COUT << "CompleteTexture(" << texture_uid << ", " << textureInfo.name << ")\n";

	std::shared_ptr<scr::Texture> scrTexture = m_pRenderPlatform->InstantiateTexture();
	scrTexture->Create(textureInfo);

	m_TextureManager->Add(texture_uid, scrTexture);

	//Add texture to materials waiting for texture.
	MissingResource& missingTexture = GetMissingResource(texture_uid, "Texture");
	for(auto it = missingTexture.waitingResources.begin(); it != missingTexture.waitingResources.end(); it++)
	{
		std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::static_pointer_cast<IncompleteMaterial>(*it);

		incompleteMaterial->textureSlots.at(texture_uid) = scrTexture;

		//If only this texture and this function are pointing to the material, then it is complete.
		if(it->use_count() == 2)
		{
			CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
		}
		else
		{
			SCR_COUT << "Waiting Material_" << incompleteMaterial->id << "(" << incompleteMaterial->materialInfo.name << ") got Texture_" << texture_uid << "(" << textureInfo.name << ")\n";
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(texture_uid);
}

void ResourceCreator::CompleteMaterial(avs::uid material_uid, const scr::Material::MaterialCreateInfo& materialInfo)
{
	SCR_COUT << "CompleteMaterial(" << material_uid << ", " << materialInfo.name << ")\n";

	std::shared_ptr<scr::Material> material = std::make_shared<scr::Material>(materialInfo);
	m_MaterialManager->Add(material_uid, material);

	//Add material to actors waiting for material.
	MissingResource& missingMaterial = GetMissingResource(material_uid, "Material");
	for(auto it = missingMaterial.waitingResources.begin(); it != missingMaterial.waitingResources.end(); it++)
	{
		std::shared_ptr<IncompleteActor> incompleteActor = std::static_pointer_cast<IncompleteActor>(*it);

		auto indexesPair = incompleteActor->materialSlots.find(material_uid);
		for(size_t materialIndex : indexesPair->second)
		{
			incompleteActor->actor->SetMaterial(materialIndex, material);
		}

		//If only this material and function are pointing to the MeshNode, then it is complete.
		if(incompleteActor.use_count() == 2)
		{
			CompleteActor(incompleteActor->id, incompleteActor->actor, incompleteActor->isHand);
		}
		else
		{
			SCR_COUT << "Waiting MeshNode_" << incompleteActor->id << "(" << incompleteActor->actor->name << ") got Material_" << material_uid << "(" << materialInfo.name << ")\n";
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(material_uid);
}

void ResourceCreator::CompleteActor(avs::uid actor_uid, std::shared_ptr<scr::Node> actor, bool isHand)
{
	SCR_COUT << "CompleteActor(ID: " << actor_uid << ", actor: " << actor->name << ", isHand: " << isHand << ")\n";

	///We're using the node ID as the actor ID as we are currently generating an actor per node/transform anyway; this way the server can tell the client to remove an actor.
	m_CompletedActors.push_back(actor_uid);
}

void ResourceCreator::CompleteBone(avs::uid boneID, std::shared_ptr<scr::Bone> bone)
{
	SCR_COUT << "CompleteBone(" << boneID << ", " << bone->name << ")\n";

	m_BoneManager->Add(boneID, bone);

	//Add bone to skin waiting for bone.
	MissingResource& missingBone = GetMissingResource(boneID, "Bone");
	for(auto it = missingBone.waitingResources.begin(); it != missingBone.waitingResources.end(); it++)
	{
		if((*it)->type == avs::GeometryPayloadType::Skin)
		{
			std::shared_ptr<IncompleteSkin> incompleteSkin = std::static_pointer_cast<IncompleteSkin>(*it);
			incompleteSkin->skin->SetBone(incompleteSkin->missingBones[boneID], bone);

			//If only this bone, and the loop, are pointing at the skin, then it is complete.
			if(it->use_count() == 2) CompleteSkin(incompleteSkin->id, incompleteSkin);
			else SCR_COUT << "Waiting Skin_" << incompleteSkin->id << "(" << incompleteSkin->skin->name << ") got Bone_" << boneID << "(" << bone->name << ")\n";
		}
		else if((*it)->type == avs::GeometryPayloadType::Animation)
		{
			std::shared_ptr<IncompleteAnimation> incompleteAnimation = std::static_pointer_cast<IncompleteAnimation>(*it);
			incompleteAnimation->animation->boneKeyframes[incompleteAnimation->missingBones[boneID]].bonePtr = bone;

			//If only this bone, and the loop, are pointing at the animation, then it is complete.
			if(it->use_count() == 2) CompleteAnimation(incompleteAnimation->id, incompleteAnimation);
			else SCR_COUT << "Waiting Animation_" << incompleteAnimation->id << "(" << incompleteAnimation->animation->name << ") got Bone_" << boneID << "(" << bone->name << ")\n";
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(boneID);
}

void ResourceCreator::CompleteAnimation(avs::uid animationID, std::shared_ptr<IncompleteAnimation> completeAnimation)
{
	SCR_COUT << "CompleteAnimation(" << animationID << ", " << completeAnimation->animation->name << ")\n";

	//Update animation length now that all data for the animation has been received.
	completeAnimation->animation->updateAnimationLength();
	m_AnimationManager->Add(animationID, completeAnimation->animation);

	//Add animation to waiting actors.
	MissingResource& missingAnimation = GetMissingResource(animationID, "Animation");
	for(auto it = missingAnimation.waitingResources.begin(); it != missingAnimation.waitingResources.end(); it++)
	{
		std::shared_ptr<IncompleteActor> incompleteActor = std::static_pointer_cast<IncompleteActor>(*it);
		incompleteActor->actor->animationComponent.AddAnimation(animationID, completeAnimation->animation);

		//If only this bone, and the loop, are pointing at the skin, then it is complete.
		if(incompleteActor.use_count() == 2) CompleteActor(incompleteActor->id, incompleteActor->actor, incompleteActor->isHand);
		else SCR_COUT << "Waiting MeshNode_" << incompleteActor->id << "(" << incompleteActor->actor->name << ") got Animation_" << animationID << "(" << completeAnimation->animation->name << ")\n";
	}

	//Resource has arrived, so we are no longer waiting for it.
	m_MissingResources.erase(animationID);
}

void ResourceCreator::AddTextureToMaterial(const avs::TextureAccessor& accessor, const avs::vec4& colourFactor, const std::shared_ptr<scr::Texture>& dummyTexture, std::shared_ptr<IncompleteMaterial> incompleteMaterial, scr::Material::MaterialParameter& materialParameter)
{
	if(accessor.index != 0)
	{
		const std::shared_ptr<scr::Texture> texture = m_TextureManager->Get(accessor.index);

		if(texture)
		{
			materialParameter.texture = texture;
		}
		else
		{
			SCR_COUT << "Material_" << incompleteMaterial->id << "(" << incompleteMaterial->id << ") missing Texture_" << accessor.index << std::endl;

			m_ResourceRequests.push_back(accessor.index);
			GetMissingResource(accessor.index, "Texture").waitingResources.push_back(incompleteMaterial);
			incompleteMaterial->textureSlots.emplace(accessor.index, materialParameter.texture);
		}

		avs::vec2 tiling = {accessor.tiling.x, accessor.tiling.y};

		materialParameter.texCoordsScalar[0] = tiling;
		materialParameter.texCoordsScalar[1] = tiling;
		materialParameter.texCoordsScalar[2] = tiling;
		materialParameter.texCoordsScalar[3] = tiling;
		materialParameter.texCoordIndex = static_cast<float>(accessor.texCoord);
	}
	else
	{
		materialParameter.texture = dummyTexture;
		materialParameter.texCoordsScalar[0] = avs::vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[1] = avs::vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[2] = avs::vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[3] = avs::vec2(1.0f, 1.0f);
		materialParameter.texCoordIndex = 0.0f;
	}

	materialParameter.textureOutputScalar = colourFactor;
}

ResourceCreator::MissingResource& ResourceCreator::GetMissingResource(avs::uid id, const char* resourceType)
{
	auto missingPair = m_MissingResources.find(id);
	if(missingPair == m_MissingResources.end())
	{
		missingPair = m_MissingResources.emplace(id, MissingResource(id, resourceType)).first;
	}

	return missingPair->second;
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
					uint32_t outDataSize = basist::basis_get_bytes_per_block_or_pixel(basis_textureFormat) * basisBlocks;

					unsigned char* outData = new unsigned char[outDataSize];
					if(basis_transcoder.transcode_image_level(transcoding.data, transcoding.dataSize, 0, mipIndex, outData, basisBlocks, basis_textureFormat))
					{
						transcoding.scrTexture.mipSizes.push_back(outDataSize);
						transcoding.scrTexture.mips.push_back(outData);
					}
					else
					{
						SCR_CERR<<"Texture \"" << transcoding.name << "\" failed to transcode mipmap level " <<mipIndex<< "."<<std::endl;
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
					SCR_CERR<<"Texture \"" << transcoding.name << "\" failed to transcode, but was a valid basis file."<<std::endl;
				}

				delete[] transcoding.data;
			}
			else
			{
				SCR_CERR<<"Texture \"" << transcoding.name << "\" failed to start transcoding."<<std::endl;
			}

			texturesToTranscode.erase(texturesToTranscode.begin());
		}
	}
}
