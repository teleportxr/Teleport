// (C) Copyright 2018-2022 Simul Software Ltd
#include "ResourceCreator.h"

#include "Animation.h"
#include "Material.h"

using namespace clientrender;

ResourceCreator::ResourceCreator()
	:basis_codeBook(basist::g_global_selector_cb_size, basist::g_global_selector_cb)
	, basisThread(&ResourceCreator::BasisThread_TranscodeTextures, this)
{
	basist::basisu_transcoder_init();
}

ResourceCreator::~ResourceCreator()
{
	//Safely close the basis transcoding thread.
	shouldBeTranscoding = false;
	basisThread.join();
}

void ResourceCreator::Initialize(clientrender::RenderPlatform* r, clientrender::VertexBufferLayout::PackingStyle packingStyle)
{
	m_pRenderPlatform = r;

	assert(packingStyle == clientrender::VertexBufferLayout::PackingStyle::GROUPED || packingStyle == clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_PackingStyle = packingStyle;

	//Setup Dummy textures.
	m_DummyWhite = m_pRenderPlatform->InstantiateTexture();
	m_DummyNormal = m_pRenderPlatform->InstantiateTexture();
	m_DummyCombined = m_pRenderPlatform->InstantiateTexture();
	m_DummyBlack = m_pRenderPlatform->InstantiateTexture();
	m_DummyGreen = m_pRenderPlatform->InstantiateTexture();
	clientrender::Texture::TextureCreateInfo tci =
	{
		"Dummy Texture",
		0,
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.x),
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.y),
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.z),
		4, 1, 1,
		clientrender::Texture::Slot::UNKNOWN,
		clientrender::Texture::Type::TEXTURE_2D,
		clientrender::Texture::Format::RGBA8,
		clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
		{4},
		{{0x00000000}},
		clientrender::Texture::CompressionFormat::UNCOMPRESSED,
		false
	};

	tci.images[0] = std::vector<unsigned char>(sizeof(whiteBGRA));
	memcpy(tci.images[0].data(), &whiteBGRA, sizeof(whiteBGRA));
	m_DummyWhite->Create(tci);

	tci.images[0] = std::vector<unsigned char>(sizeof(normalRGBA));
	memcpy(tci.images[0].data(), &normalRGBA, sizeof(normalRGBA));
	m_DummyNormal->Create(tci);

	tci.images[0] = std::vector<unsigned char>(sizeof(combinedBGRA));
	memcpy(tci.images[0].data(), &combinedBGRA, sizeof(combinedBGRA));
	m_DummyCombined->Create(tci);

	tci.images[0] = std::vector<unsigned char>(sizeof(blackBGRA));
	memcpy(tci.images[0].data(), &blackBGRA, sizeof(blackBGRA));
	m_DummyBlack->Create(tci);

	const size_t GRID=128;
	tci.images[0] = std::vector<unsigned char>(sizeof(uint32_t)*GRID*GRID);
	tci.width=tci.height=GRID;
	size_t sz=GRID*GRID*sizeof(uint32_t);
	tci.imageSizes[0]=sz;
	uint32_t green_grid[GRID*GRID];
	memset(green_grid,0,sz);
	for(size_t i=0;i<GRID;i+=16)
	{
		for(size_t j=0;j<GRID;j++)
		{
			green_grid[GRID*i+j]=greenBGRA;
			green_grid[GRID*j+i]=greenBGRA;
		}
	}
	tci.images[0] = std::vector<unsigned char>(sz);
	memcpy(tci.images[0].data(), &green_grid, sizeof(green_grid));
	m_DummyGreen->Create(tci);
}


void ResourceCreator::Clear()
{
	mutex_texturesToCreate.lock();
	texturesToCreate.clear();
	mutex_texturesToCreate.unlock();

	mutex_texturesToTranscode.lock();
	texturesToTranscode.clear();
	mutex_texturesToTranscode.unlock();

	geometryCache->ClearResourceRequests();
	//geometryCache->m_ResourceRequests.clear();
	geometryCache->ClearReceivedResources();
	geometryCache->m_CompletedNodes.clear();
	geometryCache->m_MissingResources.clear();
}

void ResourceCreator::Update(float deltaTime)
{
	std::lock_guard<std::mutex> lock_texturesToCreate(mutex_texturesToCreate);

	//Complete any textures that have finished to transcode, and are waiting.
	//This has to happen on the main thread, so we can use the main GL context.
	for (auto texturePair = texturesToCreate.begin(); texturePair != texturesToCreate.end();)
	{
		// The texture object has a copy of the create info and
		// will delete the allocated mip data in its destructor.
		CompleteTexture(texturePair->first, texturePair->second);
		texturePair = texturesToCreate.erase(texturePair);
	}
}

avs::Result ResourceCreator::CreateMesh(avs::MeshCreate& meshCreate)
{
	geometryCache->ReceivedResource(meshCreate.mesh_uid);
	//TELEPORT_COUT << "Assemble(Mesh" << meshCreate.mesh_uid << ": " << meshCreate.name << ")\n";

	using namespace clientrender;
	// TODO: this should NEVER happen, it represents a resource id conflict:
	//if (geometryCache->mVertexBufferManager.Has(meshCreate.mesh_uid) || geometryCache->mIndexBufferManager.Has(meshCreate.mesh_uid))
	//	return avs::Result::Failed;

	if (!m_pRenderPlatform)
	{
		TELEPORT_CERR << "No valid render platform was found." << std::endl;
		return avs::Result::GeometryDecoder_ClientRendererError;
	}
	clientrender::Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.name = meshCreate.name;
	mesh_ci.id = meshCreate.mesh_uid;
	mesh_ci.vb.resize(meshCreate.m_NumElements);
	mesh_ci.ib.resize(meshCreate.m_NumElements);

	for (size_t i = 0; i < meshCreate.m_NumElements; i++)
	{
		avs::MeshElementCreate& meshElementCreate = meshCreate.m_MeshElementCreate[i];

		//We have to pad the UV1s, if we are missing UV1s but have joints and weights; we use a vector so it will clean itself up.
		std::vector<avs::vec2> paddedUV1s(meshElementCreate.m_VertexCount);
		if (!meshElementCreate.m_UV1s && (meshElementCreate.m_Joints || meshElementCreate.m_Weights))
		{
			meshElementCreate.m_UV1s = paddedUV1s.data();
		}

		std::shared_ptr<VertexBufferLayout> layout(new VertexBufferLayout);
		if (meshElementCreate.m_Vertices)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::POSITION, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Normals || meshElementCreate.m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::NORMAL, VertexBufferLayout::ComponentCount::VEC3, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Tangents || meshElementCreate.m_TangentNormals)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::TANGENT, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_UV0s)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::TEXCOORD_0, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_UV1s)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::TEXCOORD_1, VertexBufferLayout::ComponentCount::VEC2, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Colors)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::COLOR_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Joints)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::JOINTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		if (meshElementCreate.m_Weights)
		{
			layout->AddAttribute((uint32_t)avs::AttributeSemantic::WEIGHTS_0, VertexBufferLayout::ComponentCount::VEC4, VertexBufferLayout::Type::FLOAT);
		}
		layout->CalculateStride();
		layout->m_PackingStyle = this->m_PackingStyle;

		size_t constructedVBSize = layout->m_Stride * meshElementCreate.m_VertexCount;
		size_t indicesSize = meshElementCreate.m_IndexCount * meshElementCreate.m_IndexSize;

		std::unique_ptr<float[]> constructedVB = std::make_unique<float[]>(constructedVBSize);
		std::unique_ptr<uint8_t[]> _indices = std::make_unique<uint8_t[]>(indicesSize);

		memcpy(_indices.get(), meshElementCreate.m_Indices, indicesSize);

		if (layout->m_PackingStyle == clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED)
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
					::vec4 tangent;
					char* nt = (char*)(meshElementCreate.m_TangentNormals + (meshElementCreate.m_TangentNormalSize * j));
					// tangentx tangentz
					if (meshElementCreate.m_TangentNormalSize == 8)
					{
						avs::Vec4<signed char>& x8 = *((avs::Vec4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						avs::Vec4<signed char>& n8 = *((avs::Vec4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						avs::Vec4<short>& x8 = *((avs::Vec4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						avs::Vec4<short>& n8 = *((avs::Vec4<short>*)(nt + 8));
						normal.x = float(n8.x) / 32767.0f;
						normal.y = float(n8.y) / 32767.0f;
						normal.z = float(n8.z) / 32767.0f;
					}
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, &normal, sizeof(avs::vec3));
					intraStrideOffset += 3;
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, &tangent, sizeof(vec4));
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
						memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Tangents + j, sizeof(vec4));
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
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Colors + j, sizeof(vec4));
					intraStrideOffset += 4;
				}
				if (meshElementCreate.m_Joints)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Joints + j, sizeof(vec4));
					intraStrideOffset += 4;
				}
				if (meshElementCreate.m_Weights)
				{
					memcpy(constructedVB.get() + (layout->m_Stride / 4 * j) + intraStrideOffset, meshElementCreate.m_Weights + j, sizeof(vec4));
					intraStrideOffset += 4;
				}
			}
		}
		else if (layout->m_PackingStyle == clientrender::VertexBufferLayout::PackingStyle::GROUPED)
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
					vec4 tangent;
					char* nt = (char*)(meshElementCreate.m_TangentNormals + (meshElementCreate.m_TangentNormalSize * j));
					// tangentx tangentz
					if (meshElementCreate.m_TangentNormalSize == 8)
					{
						avs::Vec4<signed char>& x8 = *((avs::Vec4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						avs::Vec4<signed char>& n8 = *((avs::Vec4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						avs::Vec4<short>& x8 = *((avs::Vec4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						avs::Vec4<short>& n8 = *((avs::Vec4<short>*)(nt + 8));
						normal.x = float(n8.x) / 32767.0f;
						normal.y = float(n8.y) / 32767.0f;
						normal.z = float(n8.z) / 32767.0f;
					}

					size_t size = sizeof(avs::vec3);
					assert(constructedVBSize >= vertexBufferOffset + size);
					memcpy(constructedVB.get() + vertexBufferOffset, &normal, size);
					vertexBufferOffset += size;

					size = sizeof(vec4);
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
					size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
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
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Colors, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Joints)
			{
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Joints, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Weights)
			{
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBSize >= vertexBufferOffset + size);
				memcpy(constructedVB.get() + vertexBufferOffset, meshElementCreate.m_Weights, size);
				vertexBufferOffset += size;
			}
		}
		else
		{
			TELEPORT_CERR << "Unknown vertex buffer layout." << std::endl;
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		if (constructedVBSize == 0 || constructedVB == nullptr || meshElementCreate.m_IndexCount == 0 || meshElementCreate.m_Indices == nullptr)
		{
			TELEPORT_CERR << "Unable to construct vertex and index buffers." << std::endl;
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		std::shared_ptr<VertexBuffer> vb = m_pRenderPlatform->InstantiateVertexBuffer();
		VertexBuffer::VertexBufferCreateInfo vb_ci;
		vb_ci.layout = layout;
		vb_ci.usage = BufferUsageBit::STATIC_BIT | BufferUsageBit::DRAW_BIT;
		vb_ci.vertexCount = meshElementCreate.m_VertexCount;
		vb_ci.size = constructedVBSize;
		vb_ci.data = (const void*)constructedVB.get();
		vb->Create(&vb_ci);

		std::shared_ptr<IndexBuffer> ib = m_pRenderPlatform->InstantiateIndexBuffer();
		IndexBuffer::IndexBufferCreateInfo ib_ci;
		ib_ci.usage = BufferUsageBit::STATIC_BIT | BufferUsageBit::DRAW_BIT;
		ib_ci.indexCount = meshElementCreate.m_IndexCount;
		ib_ci.stride = meshElementCreate.m_IndexSize;
		ib_ci.data = _indices.get();
		ib->Create(&ib_ci);

		geometryCache->mVertexBufferManager.Add(geometryCache->GenerateUid(meshElementCreate.vb_id), vb);
		geometryCache->mIndexBufferManager.Add(geometryCache->GenerateUid(meshElementCreate.ib_id), ib);

		mesh_ci.vb[i] = vb;
		mesh_ci.ib[i] = ib;
	}
	if (!geometryCache->mMeshManager.Has(meshCreate.mesh_uid))
	{
		CompleteMesh(meshCreate.mesh_uid, mesh_ci);
	}

	return avs::Result::OK;
}

//Returns a clientrender::Texture::Format from a avs::TextureFormat.
clientrender::Texture::Format textureFormatFromAVSTextureFormat(avs::TextureFormat format)
{
	switch (format)
	{
	case avs::TextureFormat::INVALID: return clientrender::Texture::Format::FORMAT_UNKNOWN;
	case avs::TextureFormat::G8: return clientrender::Texture::Format::R8;
	case avs::TextureFormat::BGRA8: return clientrender::Texture::Format::BGRA8;
	case avs::TextureFormat::BGRE8: return clientrender::Texture::Format::BGRA8;
	case avs::TextureFormat::RGBA16: return clientrender::Texture::Format::RGBA16;
	case avs::TextureFormat::RGBE8: return clientrender::Texture::Format::RGBA8;
	case avs::TextureFormat::RGBA16F: return clientrender::Texture::Format::RGBA16F;
	case avs::TextureFormat::RGBA32F: return clientrender::Texture::Format::RGBA32F;
	case avs::TextureFormat::RGBA8: return clientrender::Texture::Format::RGBA8;
	case avs::TextureFormat::D16F: return clientrender::Texture::Format::DEPTH_COMPONENT16;
	case avs::TextureFormat::D24F: return clientrender::Texture::Format::DEPTH_COMPONENT24;
	case avs::TextureFormat::D32F: return clientrender::Texture::Format::DEPTH_COMPONENT32F;
	case avs::TextureFormat::MAX: return clientrender::Texture::Format::FORMAT_UNKNOWN;
	default:
		exit(1);
	}
}

basist::transcoder_texture_format transcoderFormatFromBasisTextureFormat(basist::basis_tex_format format)
{
	switch (format)
	{
	case basist::basis_tex_format::cETC1S: return basist::transcoder_texture_format::cTFBC3;
	case basist::basis_tex_format::cUASTC4x4: return basist::transcoder_texture_format::cTFASTC_4x4;
	default:
		exit(1);
	}
}
//Returns a SCR compression format from a basis universal transcoder format.
clientrender::Texture::CompressionFormat toSCRCompressionFormat(basist::transcoder_texture_format format)
{
	switch (format)
	{
	case basist::transcoder_texture_format::cTFBC1: return clientrender::Texture::CompressionFormat::BC1;
	case basist::transcoder_texture_format::cTFBC3: return clientrender::Texture::CompressionFormat::BC3;
	case basist::transcoder_texture_format::cTFBC4: return clientrender::Texture::CompressionFormat::BC4;
	case basist::transcoder_texture_format::cTFBC5: return clientrender::Texture::CompressionFormat::BC5;
	case basist::transcoder_texture_format::cTFETC1: return clientrender::Texture::CompressionFormat::ETC1;
	case basist::transcoder_texture_format::cTFETC2: return clientrender::Texture::CompressionFormat::ETC2;
	case basist::transcoder_texture_format::cTFPVRTC1_4_RGBA: return clientrender::Texture::CompressionFormat::PVRTC1_4_OPAQUE_ONLY;
	case basist::transcoder_texture_format::cTFBC7_M6_OPAQUE_ONLY: return clientrender::Texture::CompressionFormat::BC7_M6_OPAQUE_ONLY;
	case basist::transcoder_texture_format::cTFASTC_4x4_RGBA:
		return clientrender::Texture::CompressionFormat::BC6H;
	case basist::transcoder_texture_format::cTFTotalTextureFormats: return clientrender::Texture::CompressionFormat::UNCOMPRESSED;
	default:
		exit(1);
	}
}

void ResourceCreator::CreateTexture(avs::uid id, const avs::Texture& texture)
{
	geometryCache->ReceivedResource(id);
	clientrender::Texture::CompressionFormat scrTextureCompressionFormat= clientrender::Texture::CompressionFormat::UNCOMPRESSED;
	if(texture.compression!=avs::TextureCompression::UNCOMPRESSED)
	{
		switch(texture.format)
		{
		case avs::TextureFormat::RGBA32F:
			scrTextureCompressionFormat = clientrender::Texture::CompressionFormat::BC6H;
			break;
		default:
			scrTextureCompressionFormat = clientrender::Texture::CompressionFormat::BC3;
			break;
		};
	}
	clientrender::Texture::TextureCreateInfo texInfo =
	{
		texture.name,
		id,
		texture.width,
		texture.height,
		texture.depth,
		texture.bytesPerPixel,
		texture.arrayCount,
		texture.mipCount,
		clientrender::Texture::Slot::UNKNOWN,
		texture.cubemap?clientrender::Texture::Type::TEXTURE_CUBE_MAP:clientrender::Texture::Type::TEXTURE_2D, //Assumed
		textureFormatFromAVSTextureFormat(texture.format),
		clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT, //Assumed
		{},
		{},
		(texture.compression == avs::TextureCompression::BASIS_COMPRESSED) ? toSCRCompressionFormat(basis_transcoder_textureFormat) : clientrender::Texture::CompressionFormat::UNCOMPRESSED

	};

	//Copy the data out of the buffer, so it can be transcoded or used as-is (uncompressed).
	std::vector<unsigned char> data = std::vector<unsigned char>(texture.dataSize);
	memcpy(data.data(), texture.data, texture.dataSize);

	//TELEPORT_COUT << "CreateTexture(" << id << ", " << texture.name << ") ";
	if (texture.compression != avs::TextureCompression::UNCOMPRESSED)
	{
		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		texturesToTranscode.emplace_back(UntranscodedTexture{ id, std::move(data), std::move(texInfo), texture.name,texture.compression,texture.valueScale });
		//std::cout << "will transcode with "<<(texture.compression==avs::TextureCompression::BASIS_COMPRESSED?"Basis":"Png")<<"\n";
	}
	else
	{
		texInfo.imageSizes.push_back(texture.dataSize);
		texInfo.images.emplace_back(std::move(data));

		//std::cout << "Uncompressed, completing.\n";
		CompleteTexture(id, texInfo);
	}
}

void ResourceCreator::CreateMaterial(avs::uid id, const avs::Material& material)
{
	//TELEPORT_COUT << "CreateMaterial(" << id << ", " << material.name << ")\n";
	geometryCache->ReceivedResource(id);

	std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::make_shared<IncompleteMaterial>(id, avs::GeometryPayloadType::Material);
	//A list of unique resources that the material is missing, and needs to be completed.
 	std::set<avs::uid> missingResources;

	incompleteMaterial->materialInfo.name = material.name;

	//Colour/Albedo/Diffuse
	AddTextureToMaterial(material.pbrMetallicRoughness.baseColorTexture,
		*((vec4*)&material.pbrMetallicRoughness.baseColorFactor),
		m_DummyWhite,
		incompleteMaterial,
		incompleteMaterial->materialInfo.diffuse);

	//Normal
	AddTextureToMaterial(material.normalTexture,
		vec4{ material.normalTexture.scale, material.normalTexture.scale, 1.0f, 1.0f },
		m_DummyNormal,
		incompleteMaterial,
		incompleteMaterial->materialInfo.normal);

	//Combined
	AddTextureToMaterial(material.pbrMetallicRoughness.metallicRoughnessTexture,
		vec4{ material.pbrMetallicRoughness.roughnessMultiplier, material.pbrMetallicRoughness.metallicFactor, material.occlusionTexture.strength, material.pbrMetallicRoughness.roughnessOffset },
		m_DummyCombined,
		incompleteMaterial,
		incompleteMaterial->materialInfo.combined);

	//Emissive
	AddTextureToMaterial(material.emissiveTexture,
		vec4(material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, 1.0f),
		m_DummyWhite,
		incompleteMaterial,
		incompleteMaterial->materialInfo.emissive);
// Add it to the manager, even if incomplete.
	std::shared_ptr<clientrender::Material> scrMaterial = std::make_shared<clientrender::Material>(m_pRenderPlatform,incompleteMaterial->materialInfo);
	geometryCache->mMaterialManager.Add(id, scrMaterial);
	scrMaterial->id = id;

	if (incompleteMaterial->textureSlots.size() == 0)
	{
		CompleteMaterial(id, incompleteMaterial->materialInfo);
	}
}

void ResourceCreator::CreateNode(avs::uid id, avs::Node& node)
{
	geometryCache->ReceivedResource(id);

	switch (node.data_type)
	{
	case avs::NodeDataType::Invalid:
		TELEPORT_CERR << "CreateNode failure! Received a node with a data type of Invalid(0)!\n";
		break;
	case avs::NodeDataType::None:
		CreateMeshNode(id, node);
		break;
	case avs::NodeDataType::Mesh:
		CreateMeshNode(id, node);
		break;
	case avs::NodeDataType::Light:
		CreateLight(id, node);
		break;
	case avs::NodeDataType::Bone:
		CreateBone(id, node);
		break;
	default:
		SCR_LOG("Unknown NodeDataType: %c", static_cast<int>(node.data_type));
		break;
	}
}

void ResourceCreator::CreateSkin(avs::uid id, avs::Skin& skin)
{
	TELEPORT_COUT << "CreateSkin(" << id << ", " << skin.name << ")\n";
	geometryCache->ReceivedResource(id);

	std::shared_ptr<IncompleteSkin> incompleteSkin = std::make_shared<IncompleteSkin>(id, avs::GeometryPayloadType::Skin);

	//Convert avs::Mat4x4 to clientrender::Transform.
	std::vector<mat4> inverseBindMatrices;
	inverseBindMatrices.reserve(skin.inverseBindMatrices.size());
	for (const avs::Mat4x4& matrix : skin.inverseBindMatrices)
	{
		inverseBindMatrices.push_back(*((mat4*)&matrix));
	}

	//Create skin.
	incompleteSkin->skin = m_pRenderPlatform->InstantiateSkin(skin.name, inverseBindMatrices, skin.boneIDs.size(), skin.skinTransform);

	std::vector<avs::uid> bone_ids;
	bone_ids.resize(skin.boneTransforms.size());
	incompleteSkin->skin->SetNumBones(bone_ids.size());
	//Add bones. This is the full list of transforms for this skin.
	for (size_t i = 0; i < bone_ids.size(); i++)
	{
		static uint64_t next_bone_id=0;
		next_bone_id++;
		bone_ids[i]=next_bone_id;
		std::shared_ptr<clientrender::Bone> bone = std::make_shared<clientrender::Bone>(next_bone_id,skin.boneNames[i]);
		geometryCache->mBoneManager.Add(next_bone_id, bone);
		std::shared_ptr<clientrender::Bone> parent=geometryCache->mBoneManager.Get(bone_ids[skin.parentIndices[i]]);
		if(parent)
		{
			bone->SetParent(parent);
			parent->AddChild(bone);
		}
		else if(skin.parentIndices[i]!=0)
		{
			TELEPORT_CERR<<"Error creating skin "<<std::endl;
		}
		bone->SetLocalTransform(skin.boneTransforms[i]);

		incompleteSkin->skin->SetBone(i, bone);
	}
	// Add joints. This is only those bones that are part of the skeleton, a subset of the full hierarchy.
	std::vector<std::shared_ptr<clientrender::Bone>> joints;
	joints.reserve(skin.jointIndices.size());
	for (size_t i = 0; i < skin.jointIndices.size(); i++)
	{
		avs::uid jointID = bone_ids[skin.jointIndices[i]];
		std::shared_ptr<clientrender::Bone> bone = geometryCache->mBoneManager.Get(jointID);

		if(bone)
			joints.push_back(bone);
		#if TELEPORT_INTERNAL_CHECKS
		else
		{
			TELEPORT_CERR << "Joints must be a subset of bones!" << std::endl;
		}
#endif
	}
	incompleteSkin->skin->SetJoints(joints);
	if (incompleteSkin->missingBones.size() == 0)
	{
		CompleteSkin(id, incompleteSkin);
	}
}

void ResourceCreator::CreateAnimation(avs::uid id, avs::Animation& animation)
{
	TELEPORT_COUT << "CreateAnimation(" << id << ", " << animation.name << ")\n";
	geometryCache->ReceivedResource(id);

	std::vector<clientrender::BoneKeyframeList> boneKeyframeLists;
	boneKeyframeLists.reserve(animation.boneKeyframes.size());

	for(size_t i = 0; i < animation.boneKeyframes.size(); i++)
	{
		const avs::TransformKeyframeList& avsKeyframes = animation.boneKeyframes[i];

		clientrender::BoneKeyframeList boneKeyframeList;
		boneKeyframeList.boneIndex = avsKeyframes.boneIndex;
		boneKeyframeList.positionKeyframes = avsKeyframes.positionKeyframes;
		boneKeyframeList.rotationKeyframes = avsKeyframes.rotationKeyframes;

		boneKeyframeLists.push_back(boneKeyframeList);
	}

	std::shared_ptr<clientrender::Animation> completeAnimation = std::make_shared<clientrender::Animation>(animation.name, boneKeyframeLists);
	CompleteAnimation(id, completeAnimation);
}

void ResourceCreator::CreateMeshNode(avs::uid id, avs::Node& node)
{
	if (geometryCache->mNodeManager->HasNode(id))
	{
		TELEPORT_CERR << "CreateMeshNode(" << id << ", " << node.name << "). Already created! "<<(node.stationary?"static":"mobile")<<"\n";
		//leaves nodes with no children. why?
		auto n=geometryCache->mNodeManager->GetNode(id);
		if (n->GetChildrenIDs().size() != node.childrenIDs.size())
		{
			TELEPORT_CERR << "recreating node " << n->id << " with " << node.childrenIDs.size() << " children." << std::endl;
		}
		return;
	}
	//TELEPORT_COUT << "CreateMeshNode(" << id << ", " << node.name;
	if (node.childrenIDs.size())
		std::cout << ", " << node.childrenIDs.size() << " children";
	std::cout<< ") " << (node.stationary ? "static" : "mobile") << "\n";

	std::shared_ptr<IncompleteNode> newNode = std::make_shared<IncompleteNode>(id, avs::GeometryPayloadType::Node);
	//Whether the node is missing any resource before, and must wait for them before it can be completed.
	bool isMissingResources = false;

	newNode->node = geometryCache->mNodeManager->CreateNode(id, node);

	if (node.data_uid != 0)
	{
		newNode->node->SetMesh(geometryCache->mMeshManager.Get(node.data_uid));
		if (!newNode->node->GetMesh())
		{
			//TELEPORT_COUT << "MeshNode_" << id << "(" << node.name << ") missing Mesh_" << node.data_uid << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(node.data_uid, avs::GeometryPayloadType::Mesh).waitingResources.push_back(newNode);
		}
	}

	if (node.skinID != 0)
	{
		auto skin=geometryCache->mSkinManager.Get(node.skinID);
		newNode->node->SetSkin(skin);
		if (!skin)
		{
			//TELEPORT_COUT << "MeshNode_" << id << "(" << node.name << ") missing Skin_" << node.skinID << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(node.skinID, avs::GeometryPayloadType::Skin).waitingResources.push_back(newNode);
		}
	}

	for (size_t i = 0; i < node.animations.size(); i++)
	{
		avs::uid animationID = node.animations[i];
		std::shared_ptr<clientrender::Animation> animation = geometryCache->mAnimationManager.Get(animationID);

		if (animation)
		{
			newNode->node->animationComponent.addAnimation(animationID, animation);
		}
		else
		{
			TELEPORT_COUT << "MeshNode_" << id << "(" << node.name << ") missing Animation_" << animationID << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(animationID, avs::GeometryPayloadType::Animation).waitingResources.push_back(newNode);
		}
	}

	if (m_pRenderPlatform->placeholderMaterial == nullptr)
	{
		clientrender::Material::MaterialCreateInfo materialCreateInfo;
		materialCreateInfo.diffuse.texture = m_DummyWhite;
		materialCreateInfo.combined.texture = m_DummyCombined;
		materialCreateInfo.normal.texture = m_DummyNormal;
		//materialCreateInfo.emissive.texture = m_DummyBlack;
		materialCreateInfo.emissive.texture = m_DummyGreen;
		m_pRenderPlatform->placeholderMaterial = std::make_shared<clientrender::Material>(m_pRenderPlatform,materialCreateInfo);
	}
	if(node.renderState.globalIlluminationUid>0)
	{
		std::shared_ptr<clientrender::Texture> gi_texture = geometryCache->mTextureManager.Get(node.renderState.globalIlluminationUid);
		if(!gi_texture)
		{
			isMissingResources = true;
			geometryCache->GetMissingResource(node.renderState.globalIlluminationUid, avs::GeometryPayloadType::Texture).waitingResources.push_back(newNode);
		}
	}
	for (size_t i = 0; i < node.materials.size(); i++)
	{
		auto mat_uid=node.materials[i];
		std::shared_ptr<clientrender::Material> material = geometryCache->mMaterialManager.Get(mat_uid);

		if (material)
		{
			newNode->node->SetMaterial(i, material);
		}
		else if(mat_uid!=0)
		{
			// If we don't know have the information on the material yet, we use placeholder OVR surfaces.
			newNode->node->SetMaterial(i, m_pRenderPlatform->placeholderMaterial);

			TELEPORT_CERR << "MeshNode_" << id << "(" << node.name << ") missing Material_" << node.materials[i] << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(node.materials[i], avs::GeometryPayloadType::Material).waitingResources.push_back(newNode);
			newNode->materialSlots[node.materials[i]].push_back(i);
		}
		else
		{
			// but if the material is uid 0, this means *no* material.
		}
	}

	//Complete node now, if we aren't missing any resources.
	if (!isMissingResources)
	{
		CompleteMeshNode(id, newNode->node);
	}
}

void ResourceCreator::CreateLight(avs::uid id, avs::Node& node)
{
	TELEPORT_COUT << "CreateLight(" << id << ", " << node.name << ")\n";
	geometryCache->ReceivedResource(id);

	clientrender::Light::LightCreateInfo lci;
	lci.renderPlatform = m_pRenderPlatform;
	lci.type = (clientrender::Light::Type)node.lightType;
	lci.position = avs::vec3(node.globalTransform.position);
	lci.direction = node.lightDirection;
	lci.orientation = clientrender::quat(node.globalTransform.rotation);
	lci.shadowMapTexture = geometryCache->mTextureManager.Get(node.data_uid);
	lci.lightColour = node.lightColour;
	lci.lightRadius = node.lightRadius;
	lci.lightRange = node.lightRange;
	lci.uid = id;
	lci.name = node.name;
	std::shared_ptr<clientrender::Light> light = std::make_shared<clientrender::Light>(&lci);
	geometryCache->mLightManager.Add(id, light);
}

void ResourceCreator::CreateBone(avs::uid id, avs::Node& node)
{
	TELEPORT_COUT << "CreateBone(" << id << ", " << node.name << ")\n";
	geometryCache->ReceivedResource(id);

	std::shared_ptr<clientrender::Bone> bone = std::make_shared<clientrender::Bone>(id, node.name);
	bone->SetLocalTransform(node.localTransform);

	//Link to parent and child bones.
	//Bones are sent in order from the top to bottom of the hierarchy.
	// So the parent will be present when the child is processed here.
	std::shared_ptr<clientrender::Bone> parent = geometryCache->mBoneManager.Get(node.parentID);
	if (!parent)
	{
		TELEPORT_CERR << "Parent not found for bone " << node.name.c_str() << std::endl;
	}
	else
	{
		bone->SetParent(parent);
		parent->AddChild(bone);
	}

	for(avs::uid childID : node.childrenIDs)
	{
		std::shared_ptr<clientrender::Bone> child = geometryCache->mBoneManager.Get(childID);
		if(child)
		{
			child->SetParent(bone);
			bone->AddChild(child);
		}
	}

	CompleteBone(id, bone);
}

void ResourceCreator::CompleteMesh(avs::uid id, const clientrender::Mesh::MeshCreateInfo& meshInfo)
{
	//TELEPORT_COUT << "CompleteMesh(" << id << ", " << meshInfo.name << ")\n";

	std::shared_ptr<clientrender::Mesh> mesh = std::make_shared<clientrender::Mesh>(meshInfo);
	geometryCache->mMeshManager.Add(id, mesh);

	//Add mesh to nodes waiting for mesh.
	MissingResource* missingMesh = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Mesh);
	if(missingMesh)
	for(auto it = missingMesh->waitingResources.begin(); it != missingMesh->waitingResources.end(); it++)
	{
		if(it->get()->type!=avs::GeometryPayloadType::Node)
		{
			TELEPORT_CERR<<"Waiting resource is not a node, it's "<<int(it->get()->type)<<std::endl;
			continue;
		}
		std::shared_ptr<IncompleteNode> incompleteNode = std::static_pointer_cast<IncompleteNode>(*it);
		incompleteNode->node->SetMesh(mesh);
		TELEPORT_COUT << "Waiting MeshNode_" << incompleteNode->id << "(" << incompleteNode->node->name << ") got Mesh_" << id << "(" << meshInfo.name << ")\n";

		//If only this mesh and this function are pointing to the node, then it is complete.
		if(it->use_count() == 2)
		{
			CompleteMeshNode(incompleteNode->id, incompleteNode->node);
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->m_MissingResources.erase(id);
}

void ResourceCreator::CompleteSkin(avs::uid id, std::shared_ptr<IncompleteSkin> completeSkin)
{
	TELEPORT_COUT << "CompleteSkin(" << id << ", " << completeSkin->skin->name << ")\n";

	geometryCache->mSkinManager.Add(id, completeSkin->skin);

	//Add skin to nodes waiting for skin.
	MissingResource *missingSkin = geometryCache->GetMissingResourceIfMissing(id,avs::GeometryPayloadType::Skin);
	if(missingSkin)
	{
		for(auto it = missingSkin->waitingResources.begin(); it != missingSkin->waitingResources.end(); it++)
		{
			std::shared_ptr<IncompleteNode> incompleteNode = std::static_pointer_cast<IncompleteNode>(*it);
			incompleteNode->node->SetSkin(completeSkin->skin);
			TELEPORT_COUT << "Waiting MeshNode_" << incompleteNode->id << "(" << incompleteNode->node->name << ") got Skin_" << id << "(" << completeSkin->skin->name << ")\n";

			//If only this resource and this skin are pointing to the node, then it is complete.
			if(it->use_count() == 2)
			{
				CompleteMeshNode(incompleteNode->id, incompleteNode->node);
			}
		}

		//Resource has arrived, so we are no longer waiting for it.
		geometryCache->m_MissingResources.erase(id);
	}
}

void ResourceCreator::CompleteTexture(avs::uid id, const clientrender::Texture::TextureCreateInfo& textureInfo)
{
	TELEPORT_COUT << "CompleteTexture(" << id << ", " << textureInfo.name << ")\n";

	std::shared_ptr<clientrender::Texture> scrTexture = m_pRenderPlatform->InstantiateTexture();
	scrTexture->Create(textureInfo);

	geometryCache->mTextureManager.Add(id, scrTexture);

	//Add texture to materials waiting for texture.
	MissingResource * missingTexture = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Texture);
	if(missingTexture)
	{
		for(auto it = missingTexture->waitingResources.begin(); it != missingTexture->waitingResources.end(); it++)
		{
			switch((*it)->type)
			{
				case avs::GeometryPayloadType::Material:
					{
						std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::static_pointer_cast<IncompleteMaterial>(*it);
						incompleteMaterial->textureSlots.at(id) = scrTexture;
						TELEPORT_COUT << "Waiting Material_" << incompleteMaterial->id << "(" << incompleteMaterial->materialInfo.name << ") got Texture_" << id << "(" << textureInfo.name << ")\n";

						//If only this texture and this function are pointing to the material, then it is complete.
						if (it->use_count() == 2)
						{
							CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
						}
					}
					break;
				case avs::GeometryPayloadType::Node:
					{
						std::shared_ptr<IncompleteNode> incompleteNode = std::static_pointer_cast<IncompleteNode>(*it);
						TELEPORT_COUT << "Waiting Node " << incompleteNode->id << "(" << incompleteNode->node->name.c_str() << ") got Texture_" << id << "(" << textureInfo.name << ")\n";

						//If only this material and function are pointing to the MeshNode, then it is complete.
						if(incompleteNode.use_count() == 2)
						{
							CompleteMeshNode(incompleteNode->id, incompleteNode->node);
						}
					}
					break;
				default:
					break;
			}
		}
		//Resource has arrived, so we are no longer waiting for it.
		geometryCache->m_MissingResources.erase(id);
	}
}

void ResourceCreator::CompleteMaterial(avs::uid id, const clientrender::Material::MaterialCreateInfo& materialInfo)
{
	TELEPORT_COUT << "CompleteMaterial(" << id << ", " << materialInfo.name << ")\n";
	std::shared_ptr<clientrender::Material> material = geometryCache->mMaterialManager.Get(id);
	// Update its properties:
	material->SetMaterialCreateInfo(m_pRenderPlatform,materialInfo);
	//Add material to nodes waiting for material.
	MissingResource *missingMaterial = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Material);
	if(missingMaterial)
	{
		for(auto it = missingMaterial->waitingResources.begin(); it != missingMaterial->waitingResources.end(); it++)
		{
			std::shared_ptr<IncompleteNode> incompleteNode = std::static_pointer_cast<IncompleteNode>(*it);

			const auto &indexesPair = incompleteNode->materialSlots.find(id);
			if (indexesPair == incompleteNode->materialSlots.end())
			{
				TELEPORT_CERR << "Material " << id << " not found in incomplete node " << incompleteNode->id << std::endl;
				continue;
			}
			for(size_t materialIndex : indexesPair->second)
			{
				incompleteNode->node->SetMaterial(materialIndex, material);
			}
			TELEPORT_COUT << "Waiting MeshNode_" << incompleteNode->id << "(" << incompleteNode->node->name << ") got Material_" << id << "(" << materialInfo.name << ")\n";

			//If only this material and function are pointing to the MeshNode, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteMeshNode(incompleteNode->id, incompleteNode->node);
			}
		}

		//Resource has arrived, so we are no longer waiting for it.
		geometryCache->m_MissingResources.erase(id);
	}
}

void ResourceCreator::CompleteMeshNode(avs::uid id, std::shared_ptr<clientrender::Node> node)
{
//	TELEPORT_COUT << "CompleteMeshNode(ID: " << id << ", node: " << node->name << ")\n";

	///We're using the node ID as the node ID as we are currently generating an node per node/transform anyway; this way the server can tell the client to remove an node.
	geometryCache->m_CompletedNodes.push_back(id);
}

void ResourceCreator::CompleteBone(avs::uid id, std::shared_ptr<clientrender::Bone> bone)
{
	//TELEPORT_COUT << "CompleteBone(" << id << ", " << bone->name << ")\n";

	geometryCache->mBoneManager.Add(id, bone);

	//Add bone to skin waiting for bone.
	MissingResource *missingBone = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Bone);
	if(missingBone)
	{
		for(auto it = missingBone->waitingResources.begin(); it != missingBone->waitingResources.end(); it++)
		{
			if((*it)->type == avs::GeometryPayloadType::Skin)
			{
				std::shared_ptr<IncompleteSkin> incompleteSkin = std::static_pointer_cast<IncompleteSkin>(*it);
				incompleteSkin->skin->SetBone(incompleteSkin->missingBones[id], bone);
				TELEPORT_COUT << "Waiting Skin_" << incompleteSkin->id << "(" << incompleteSkin->skin->name << ") got Bone_" << id << "(" << bone->name << ")\n";

				//If only this bone, and the loop, are pointing at the skin, then it is complete.
				if(it->use_count() == 2)
				{
					CompleteSkin(incompleteSkin->id, incompleteSkin);
				}
			}
		}

		//Resource has arrived, so we are no longer waiting for it.
		geometryCache->m_MissingResources.erase(id);
	}
}

void ResourceCreator::CompleteAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation)
{
	TELEPORT_COUT << "CompleteAnimation(" << id << ", " << animation->name << ")\n";

	//Update animation length before adding to the animation manager.
	animation->updateAnimationLength();
	geometryCache->mAnimationManager.Add(id, animation);

	//Add animation to waiting nodes.
	MissingResource *missingAnimation = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Animation);
	if(missingAnimation)
	{
		for(auto it = missingAnimation->waitingResources.begin(); it != missingAnimation->waitingResources.end(); it++)
		{
			std::shared_ptr<IncompleteNode> incompleteNode = std::static_pointer_cast<IncompleteNode>(*it);
			incompleteNode->node->animationComponent.addAnimation(id, animation);
			TELEPORT_COUT << "Waiting MeshNode_" << incompleteNode->id << "(" << incompleteNode->node->name << ") got Animation_" << id << "(" << animation->name << ")\n";

			//If only this bone, and the loop, are pointing at the skin, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteMeshNode(incompleteNode->id, incompleteNode->node);
			}
		}

		//Resource has arrived, so we are no longer waiting for it.
		geometryCache->m_MissingResources.erase(id);
	}
}

void ResourceCreator::AddTextureToMaterial(const avs::TextureAccessor& accessor, const vec4& colourFactor, const std::shared_ptr<clientrender::Texture>& dummyTexture,
										   std::shared_ptr<IncompleteMaterial> incompleteMaterial, clientrender::Material::MaterialParameter& materialParameter)
{
	if (accessor.index != 0)
	{
		const std::shared_ptr<clientrender::Texture> texture = geometryCache->mTextureManager.Get(accessor.index);

		if (texture)
		{
			materialParameter.texture = texture;
		}
		else
		{
			TELEPORT_COUT << "Material_" << incompleteMaterial->id << "(" << incompleteMaterial->id << ") missing Texture_" << accessor.index << std::endl;

			geometryCache->GetMissingResource(accessor.index, avs::GeometryPayloadType::Texture).waitingResources.push_back(incompleteMaterial);
			incompleteMaterial->textureSlots.emplace(accessor.index, materialParameter.texture);
		}

		avs::vec2 tiling = { accessor.tiling.x, accessor.tiling.y };

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

	materialParameter.textureOutputScalar = *((avs::vec4*)&colourFactor);
}

//#define STB_IMAGE_IMPLEMENTATION
namespace teleport
{
#include "stb_image.h"
}

bool BasisValidate(basist::basisu_transcoder &dec, basist::basisu_file_info &fileinfo,const std::vector<unsigned char>& data)
{
#ifndef __ANDROID__

	assert(fileinfo.m_total_images == fileinfo.m_image_mipmap_levels.size());
	assert(fileinfo.m_total_images == dec.get_total_images(data.data(), (uint32_t)data.size()));

	printf("File info:\n");
	printf("  Version: %X\n", fileinfo.m_version);
	printf("  Total header size: %u\n", fileinfo.m_total_header_size);
	printf("  Total selectors: %u\n", fileinfo.m_total_selectors);
	printf("  Selector codebook size: %u\n", fileinfo.m_selector_codebook_size);
	printf("  Total endpoints: %u\n", fileinfo.m_total_endpoints);
	printf("  Endpoint codebook size: %u\n", fileinfo.m_endpoint_codebook_size);
	printf("  Tables size: %u\n", fileinfo.m_tables_size);
	printf("  Slices size: %u\n", fileinfo.m_slices_size);
	printf("  Texture format: %s\n", (fileinfo.m_tex_format == basist::basis_tex_format::cUASTC4x4) ? "UASTC" : "ETC1S");
	printf("  Texture type: %s\n", basist::basis_get_texture_type_name(fileinfo.m_tex_type));
	printf("  us per frame: %u (%f fps)\n", fileinfo.m_us_per_frame, fileinfo.m_us_per_frame ? (1.0f / ((float)fileinfo.m_us_per_frame / 1000000.0f)) : 0.0f);
	printf("  Total slices: %u\n", (uint32_t)fileinfo.m_slice_info.size());
	printf("  Total images: %i\n", fileinfo.m_total_images);
	printf("  Y Flipped: %u, Has alpha slices: %u\n", fileinfo.m_y_flipped, fileinfo.m_has_alpha_slices);
	printf("  userdata0: 0x%X userdata1: 0x%X\n", fileinfo.m_userdata0, fileinfo.m_userdata1);
	printf("  Per-image mipmap levels: ");
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
		printf("%u ", fileinfo.m_image_mipmap_levels[i]);
	printf("\n");

	uint32_t total_texels = 0;

	printf("\nImage info:\n");
	for (uint32_t i = 0; i < fileinfo.m_total_images; i++)
	{
		basist::basisu_image_info ii;
		if (!dec.get_image_info(data.data(),(uint32_t)data.size(), ii, i))
		{
			printf("get_image_info() failed!\n");
			return false;
		}

		printf("Image %u: MipLevels: %u OrigDim: %ux%u, BlockDim: %ux%u, FirstSlice: %u, HasAlpha: %u\n", i, ii.m_total_levels, ii.m_orig_width, ii.m_orig_height,
			ii.m_num_blocks_x, ii.m_num_blocks_y, ii.m_first_slice_index, (uint32_t)ii.m_alpha_flag);

		total_texels += ii.m_width * ii.m_height;
	}

	printf("\nSlice info:\n");

	for (uint32_t i = 0; i < fileinfo.m_slice_info.size(); i++)
	{
		const basist::basisu_slice_info& sliceinfo = fileinfo.m_slice_info[i];
		printf("%u: OrigWidthHeight: %ux%u, BlockDim: %ux%u, TotalBlocks: %u, Compressed size: %u, Image: %u, Level: %u, UnpackedCRC16: 0x%X, alpha: %u, iframe: %i\n",
			i,
			sliceinfo.m_orig_width, sliceinfo.m_orig_height,
			sliceinfo.m_num_blocks_x, sliceinfo.m_num_blocks_y,
			sliceinfo.m_total_blocks,
			sliceinfo.m_compressed_size,
			sliceinfo.m_image_index, sliceinfo.m_level_index,
			sliceinfo.m_unpacked_slice_crc16,
			(uint32_t)sliceinfo.m_alpha_flag,
			(uint32_t)sliceinfo.m_iframe_flag);
	}
	printf("\n");

	const float basis_bits_per_texel = data.size() * 8.0f / total_texels;
	//const float comp_bits_per_texel = comp_size * 8.0f / total_texels;

	//printf("Original size: %u, bits per texel: %3.3f\nCompressed size (Deflate): %u, bits per texel: %3.3f\n", (uint32_t)basis_file_data.size(), basis_bits_per_texel, (uint32_t)comp_size, comp_bits_per_texel);
#endif
	return true;
}

void ResourceCreator::BasisThread_TranscodeTextures()
{
	while (shouldBeTranscoding)
	{
		std::this_thread::yield(); //Yield at the start, as we don't want to yield before we unlock (when lock goes out of scope).

		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		if (texturesToTranscode.size() != 0)
		{
			UntranscodedTexture& transcoding = texturesToTranscode[0];
			if (transcoding.fromCompressionFormat == avs::TextureCompression::PNG)
			{
				int mipWidth=0, mipHeight=0;
				uint8_t *srcPtr=transcoding.data.data();
				uint8_t *basePtr=srcPtr;
				// let's have a uint16 here, N with the number of images, then a list of N uint32 offsets. Each is a subresource image. Then image 0 starts.
				uint16_t num_images=*((uint16_t*)srcPtr);
				std::vector<uint32_t> imageOffsets(num_images);
				srcPtr+=sizeof(uint16_t);
				size_t dataSize=transcoding.data.size()-sizeof(uint16_t);
				for(int i=0;i<num_images;i++)
				{
					imageOffsets[i]=*((uint32_t*)srcPtr);
					srcPtr+=sizeof(uint32_t);
					dataSize-=sizeof(uint32_t);
				}
				imageOffsets.push_back((uint32_t)transcoding.data.size());
				std::vector<uint32_t> imageSizes(num_images);
				for(int i=0;i<num_images;i++)
				{
					imageSizes[i]=imageOffsets[i+1]-imageOffsets[i];
				}
				for(int i=0;i<num_images;i++)
				{
					// Convert from Png to raw data:
					int num_channels=0;
					unsigned char *target = teleport::stbi_load_from_memory(basePtr+imageOffsets[i],(int) imageSizes[i], &mipWidth, &mipHeight, &num_channels,(int)4);
					if(num_channels ==4 && mipWidth > 0 && mipHeight > 0&&target&&transcoding.data.size()>2)
					{
						// this is for 8-bits-per-channel textures:
						size_t outDataSize = (size_t)(mipWidth * mipHeight * 4);
						std::vector<unsigned char> outData = std::vector<unsigned char>(outDataSize);

						memcpy(outData.data(), target, outDataSize);
						transcoding.scrTexture.imageSizes.push_back(outDataSize);
						transcoding.scrTexture.images.emplace_back(std::move(outData));
						transcoding.scrTexture.valueScale=transcoding.valueScale;

					}
					else
					{
						TELEPORT_CERR << "Failed to transcode PNG-format texture \"" << transcoding.name << "\"." << std::endl;
					}
					teleport::stbi_image_free(target);
				}
				if (transcoding.scrTexture.images.size() != 0)
				{
					std::lock_guard<std::mutex> lock_texturesToCreate(mutex_texturesToCreate);
					texturesToCreate.emplace(std::pair{ transcoding.texture_uid, std::move(transcoding.scrTexture) });
				}
				else
				{
					TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to transcode, but was a valid basis file." << std::endl;
				}
			}
			else if(transcoding.fromCompressionFormat==avs::TextureCompression::BASIS_COMPRESSED)
			{
				//We need a new transcoder for every .basis file.
				basist::basisu_transcoder basis_transcoder(&basis_codeBook);
				basist::basisu_file_info fileinfo;
				if (!basis_transcoder.get_file_info(transcoding.data.data(), (uint32_t)transcoding.data.size(), fileinfo))
				{
					TELEPORT_CERR << "Failed to transcode texture \"" << transcoding.name << "\"." << std::endl;
					continue;
				}
				BasisValidate(basis_transcoder, fileinfo,transcoding.data);
				if (basis_transcoder.start_transcoding(transcoding.data.data(), (uint32_t)transcoding.data.size()))
				{
					transcoding.scrTexture.mipCount = basis_transcoder.get_total_image_levels(transcoding.data.data(), (uint32_t)transcoding.data.size(), 0);
					//transcoding.scrTexture.arrayCount = basis_transcoder.get_total_images(transcoding.data.data(), (uint32_t)transcoding.data.size(), 0);
					transcoding.scrTexture.imageSizes.reserve(transcoding.scrTexture.mipCount*transcoding.scrTexture.arrayCount);
					transcoding.scrTexture.images.reserve(transcoding.scrTexture.mipCount*transcoding.scrTexture.arrayCount);
					//basist::basis_tex_format format=basis_transcoder.get_tex_format(transcoding.data, transcoding.dataSize);
					// choose a transcoder format based on this:
					//basist::transcoder_texture_format basis_transcoder_textureFormat= transcoderFormatFromBasisTextureFormat(format);

					if (!basis_is_format_supported(basis_transcoder_textureFormat, fileinfo.m_tex_format))
					{
						TELEPORT_CERR << "Failed to transcode texture \"" << transcoding.name << "\"." << std::endl;
						continue;
					}
					//transcoding.scrTexture.mipCount=std::min(transcoding.scrTexture.mipCount,(uint)2);
					//transcoding.scrTexture.compression= toSCRCompressionFormat(basis_transcoder_textureFormat);
						for (uint32_t arrayIndex = 0; arrayIndex < transcoding.scrTexture.arrayCount; arrayIndex++)
						{
					for (uint32_t mipIndex = 0; mipIndex < transcoding.scrTexture.mipCount; mipIndex++)
					{
							uint32_t basisWidth, basisHeight, basisBlocks;

							basis_transcoder.get_image_level_desc(transcoding.data.data(), (uint32_t)transcoding.data.size(), arrayIndex, mipIndex, basisWidth, basisHeight, basisBlocks);
							uint32_t outDataSize = basist::basis_get_bytes_per_block_or_pixel(basis_transcoder_textureFormat) * basisBlocks;

							std::vector<unsigned char> outData = std::vector<unsigned char>(outDataSize);
							if (basis_transcoder.transcode_image_level(transcoding.data.data(), (uint32_t)transcoding.data.size(),arrayIndex, mipIndex, outData.data(), basisBlocks, basis_transcoder_textureFormat))
							{
								transcoding.scrTexture.imageSizes.push_back(outDataSize);
								transcoding.scrTexture.images.emplace_back(std::move(outData));
							}
							else
							{
								TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to transcode mipmap level " << mipIndex << "." << std::endl;
							}
						}
					}

					if (transcoding.scrTexture.images.size() != 0)
					{
						std::lock_guard<std::mutex> lock_texturesToCreate(mutex_texturesToCreate);
						texturesToCreate.emplace(std::pair{ transcoding.texture_uid, std::move(transcoding.scrTexture) });
					}
					else
					{
						TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to transcode, but was a valid basis file." << std::endl;
					}
				}
				else
				{
					TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to start transcoding." << std::endl;
				}
			}
			texturesToTranscode.erase(texturesToTranscode.begin());
		}
	}
}
