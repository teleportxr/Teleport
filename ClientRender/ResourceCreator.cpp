// (C) Copyright 2018-2022 Simul Software Ltd
#include "ResourceCreator.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4018) //warning C4018: '<': signed/unsigned mismatch
#endif

#include "Animation.h"
#include "Material.h"
#include <Platform/External/magic_enum/include/magic_enum.hpp>
#include "ThisPlatform/Threads.h"
#include "draco/compression/decode.h"

//#define STB_IMAGE_IMPLEMENTATION
namespace teleport
{
#include "stb_image.h"
}
using namespace clientrender;

#define RESOURCECREATOR_DEBUG_COUT(txt, ...)

ResourceCreator::ResourceCreator()
	:basisThread(&ResourceCreator::BasisThread_TranscodeTextures, this)
{
	basist::basisu_transcoder_init();
}

ResourceCreator::~ResourceCreator()
{
	//Safely close the basis transcoding thread.
	shouldBeTranscoding = false;
	basisThread.join();
}

void ResourceCreator::Initialize(platform::crossplatform::RenderPlatform* r, clientrender::VertexBufferLayout::PackingStyle packingStyle)
{
	renderPlatform = r;

	assert(packingStyle == clientrender::VertexBufferLayout::PackingStyle::GROUPED || packingStyle == clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED);
	m_PackingStyle = packingStyle;

	//Setup Dummy textures.
	m_DummyWhite = std::make_shared<clientrender::Texture>(renderPlatform);
	m_DummyNormal = std::make_shared<clientrender::Texture>(renderPlatform);
	m_DummyCombined = std::make_shared<clientrender::Texture>(renderPlatform);
	m_DummyBlack = std::make_shared<clientrender::Texture>(renderPlatform);
	m_DummyGreen = std::make_shared<clientrender::Texture>(renderPlatform);
	clientrender::Texture::TextureCreateInfo tci =
	{
		"Dummy Texture",
		0,
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.x),
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.y),
		static_cast<uint32_t>(clientrender::Texture::DUMMY_DIMENSIONS.z),
		4, 1, 1,
		//clientrender::Texture::Slot::UNKNOWN,
		clientrender::Texture::Type::TEXTURE_2D,
		clientrender::Texture::Format::RGBA8,
		clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
		{{0,0,0,0}},
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
	mutex_texturesToTranscode.lock();
	texturesToTranscode.clear();
	mutex_texturesToTranscode.unlock();

	geometryCache->ClearResourceRequests();
	geometryCache->ClearReceivedResources();
	geometryCache->m_CompletedNodes.clear();
}

void ResourceCreator::Update(float deltaTime)
{
}

avs::Result ResourceCreator::CreateMesh(avs::MeshCreate& meshCreate)
{
	geometryCache->ReceivedResource(meshCreate.mesh_uid);
	//RESOURCECREATOR_DEBUG_COUT( "Assemble(Mesh" << meshCreate.mesh_uid << ": " << meshCreate.name << ")\n";

	using namespace clientrender;
	// TODO: this should NEVER happen, it represents a resource id conflict:
	//if (geometryCache->mVertexBufferManager.Has(meshCreate.mesh_uid) || geometryCache->mIndexBufferManager.Has(meshCreate.mesh_uid))
	//	return avs::Result::Failed;

	if (!renderPlatform)
	{
		TELEPORT_CERR << "No valid render platform was found." << std::endl;
		return avs::Result::GeometryDecoder_ClientRendererError;
	}
	clientrender::Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.name = meshCreate.name;
	mesh_ci.id = meshCreate.mesh_uid;
	size_t num=meshCreate.m_MeshElementCreate.size();
	mesh_ci.vb.resize(num);
	mesh_ci.ib.resize(num);

	for (size_t i = 0; i < num; i++)
	{
		avs::MeshElementCreate& meshElementCreate = meshCreate.m_MeshElementCreate[i];

		//We have to pad the UV1s, if we are missing UV1s but have joints and weights; we use a vector so it will clean itself up.
		std::vector<vec2> paddedUV1s(meshElementCreate.m_VertexCount);
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

		size_t constructedVBByteSize = layout->m_Stride * meshElementCreate.m_VertexCount;
		size_t indicesByteSize = meshElementCreate.m_IndexCount * meshElementCreate.m_IndexSize;

		std::shared_ptr<std::vector<uint8_t>> constructedVB = std::make_shared<std::vector<uint8_t>>(constructedVBByteSize);
		std::shared_ptr<std::vector<uint8_t>> _indices = std::make_unique<std::vector<uint8_t>>(indicesByteSize);

		memcpy(_indices->data(), meshElementCreate.m_Indices, indicesByteSize);

		if (layout->m_PackingStyle == clientrender::VertexBufferLayout::PackingStyle::INTERLEAVED)
		{
			for (size_t j = 0; j < meshElementCreate.m_VertexCount; j++)
			{
				size_t intraStrideOffset = 0;
				if (meshElementCreate.m_Vertices)
				{
					memcpy(constructedVB->data() + (layout->m_Stride * j) + intraStrideOffset, meshElementCreate.m_Vertices + j, sizeof(vec3));
					intraStrideOffset += sizeof(vec3);
				}
				if (meshElementCreate.m_TangentNormals)
				{
					vec3 normal;
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
					memcpy(constructedVB->data() + (layout->m_Stride * j) + intraStrideOffset, &normal, sizeof(vec3));
					intraStrideOffset += sizeof(vec3);
					memcpy(constructedVB->data() + (layout->m_Stride * j) + intraStrideOffset, &tangent, sizeof(vec4));
					intraStrideOffset += sizeof(vec4);
				}
				else
				{
					if (meshElementCreate.m_Normals)
					{
						memcpy(constructedVB->data() + (layout->m_Stride * j) + intraStrideOffset, meshElementCreate.m_Normals + j, sizeof(vec3));
						intraStrideOffset += sizeof(vec3);
					}
					if (meshElementCreate.m_Tangents)
					{
						memcpy(constructedVB->data() + (layout->m_Stride * j) + intraStrideOffset, meshElementCreate.m_Tangents + j, sizeof(vec4));
						intraStrideOffset += sizeof(vec4);
					}
				}
				if (meshElementCreate.m_UV0s)
				{
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, meshElementCreate.m_UV0s + j, sizeof(vec2));
					intraStrideOffset += sizeof(vec2);
				}
				if (meshElementCreate.m_UV1s)
				{
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, meshElementCreate.m_UV1s + j, sizeof(vec2));
					intraStrideOffset += sizeof(vec2);
				}
				if (meshElementCreate.m_Colors)
				{
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, meshElementCreate.m_Colors + j, sizeof(vec4));
					intraStrideOffset += sizeof(vec4);
				}
				if (meshElementCreate.m_Joints)
				{
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, meshElementCreate.m_Joints + j, sizeof(vec4));
					intraStrideOffset += sizeof(vec4);
				}
				if (meshElementCreate.m_Weights)
				{
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, meshElementCreate.m_Weights + j, sizeof(vec4));
					intraStrideOffset += sizeof(vec4);
				}
			}
		}
		else if (layout->m_PackingStyle == clientrender::VertexBufferLayout::PackingStyle::GROUPED)
		{
			size_t vertexBufferOffset = 0;
			if (meshElementCreate.m_Vertices)
			{
				size_t size = sizeof(vec3) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Vertices, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_TangentNormals)
			{
				for (size_t j = 0; j < meshElementCreate.m_VertexCount; j++)
				{
					vec3 normal;
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

					size_t size = sizeof(vec3);
					assert(constructedVBByteSize >= vertexBufferOffset + size);
					memcpy(constructedVB->data() + vertexBufferOffset, &normal, size);
					vertexBufferOffset += size;

					size = sizeof(vec4);
					assert(constructedVBByteSize >= vertexBufferOffset + size);
					memcpy(constructedVB->data() + vertexBufferOffset, &tangent, size);
					vertexBufferOffset += size;
				}
			}
			else
			{
				if (meshElementCreate.m_Normals)
				{
					size_t size = sizeof(vec3) * meshElementCreate.m_VertexCount;
					assert(constructedVBByteSize >= vertexBufferOffset + size);
					memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Normals, size);
					vertexBufferOffset += size;
				}
				if (meshElementCreate.m_Tangents)
				{
					size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
					assert(constructedVBByteSize >= vertexBufferOffset + size);
					memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Tangents, size);
					vertexBufferOffset += size;
				}
			}
			if (meshElementCreate.m_UV0s)
			{
				size_t size = sizeof(vec2) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_UV0s, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_UV1s)
			{
				size_t size = sizeof(vec2) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_UV1s, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Colors)
			{
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Colors, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Joints)
			{
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Joints, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Weights)
			{
				size_t size = sizeof(vec4) * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Weights, size);
				vertexBufferOffset += size;
			}
		}
		else
		{
			TELEPORT_CERR << "Unknown vertex buffer layout." << std::endl;
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		if (constructedVBByteSize == 0 || constructedVB == nullptr || meshElementCreate.m_IndexCount == 0 || meshElementCreate.m_Indices == nullptr)
		{
			TELEPORT_CERR << "Unable to construct vertex and index buffers." << std::endl;
			return avs::Result::GeometryDecoder_ClientRendererError;
		}

		std::shared_ptr<VertexBuffer> vb = std::make_shared<clientrender::VertexBuffer>(renderPlatform);
		VertexBuffer::VertexBufferCreateInfo vb_ci;
		vb_ci.layout = layout;
		vb_ci.usage = BufferUsageBit::STATIC_BIT | BufferUsageBit::DRAW_BIT;
		vb_ci.vertexCount = meshElementCreate.m_VertexCount;
		vb_ci.data = constructedVB;
		vb->Create(&vb_ci);

		std::shared_ptr<IndexBuffer> ib = std::make_shared<clientrender::IndexBuffer>(renderPlatform);
		IndexBuffer::IndexBufferCreateInfo ib_ci;
		ib_ci.usage = BufferUsageBit::STATIC_BIT | BufferUsageBit::DRAW_BIT;
		ib_ci.indexCount = meshElementCreate.m_IndexCount;
		ib_ci.stride = meshElementCreate.m_IndexSize;
		ib_ci.data = _indices;
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
	std::shared_ptr<clientrender::Texture::TextureCreateInfo> texInfo =std::make_shared<clientrender::Texture::TextureCreateInfo>();
	texInfo->name			=texture.name;
	texInfo->uid			=id;
	texInfo->width			=texture.width;
	texInfo->height			=texture.height;
	texInfo->depth			=texture.depth;
	texInfo->bytesPerPixel	=texture.bytesPerPixel;
	texInfo->arrayCount		=texture.arrayCount;
	texInfo->mipCount		=texture.mipCount;
	texInfo->type			=texture.cubemap?clientrender::Texture::Type::TEXTURE_CUBE_MAP:clientrender::Texture::Type::TEXTURE_2D; //Assumed
	texInfo->format			=textureFormatFromAVSTextureFormat(texture.format);
	texInfo->sampleCount	=clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT; //Assumed
	texInfo->compression	=(texture.compression == avs::TextureCompression::BASIS_COMPRESSED) ? toSCRCompressionFormat(basis_transcoder_textureFormat) : clientrender::Texture::CompressionFormat::UNCOMPRESSED;

	if (texture.compression != avs::TextureCompression::UNCOMPRESSED)
	{
		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		texturesToTranscode.emplace_back(id, texture.data, texture.dataSize, texInfo, texture.name, texture.compression, texture.valueScale);
	}
	else
	{
		texInfo->images.emplace_back(texture.dataSize);
		memcpy(texInfo->images.back().data(), texture.data, texture.dataSize);

		CompleteTexture(id, *texInfo);
	}
}

void ResourceCreator::CreateMaterial(avs::uid id, const avs::Material& material)
{
	//RESOURCECREATOR_DEBUG_COUT( "CreateMaterial(" << id << ", " << material.name << ")\n";
	geometryCache->ReceivedResource(id);

	std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::make_shared<IncompleteMaterial>(id, avs::GeometryPayloadType::Material);
	//A list of unique resources that the material is missing, and needs to be completed.
 	std::set<avs::uid> missingResources;

	incompleteMaterial->materialInfo.name = material.name;
	incompleteMaterial->materialInfo.materialMode=material.materialMode;
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
	std::shared_ptr<clientrender::Material> scrMaterial = std::make_shared<clientrender::Material>(renderPlatform,incompleteMaterial->materialInfo);
	geometryCache->mMaterialManager.Add(id, scrMaterial);
	scrMaterial->id = id;

	if (incompleteMaterial->missingTextureUids.size() == 0)
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
	case avs::NodeDataType::TextCanvas:
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

void ResourceCreator::CreateFontAtlas(avs::uid id,teleport::core::FontAtlas &fontAtlas)
{
	std::shared_ptr<clientrender::FontAtlas> f = std::make_shared<clientrender::FontAtlas>(id);
	clientrender::FontAtlas &F=*f;
	*(static_cast<teleport::core::FontAtlas*>(&F))=fontAtlas;
	geometryCache->mFontAtlasManager.Add(id, f);
	geometryCache->ReceivedResource(id);

	const std::shared_ptr<clientrender::Texture> texture = geometryCache->mTextureManager.Get(fontAtlas.font_texture_uid);
	// The texture hasn't arrived yet. Mark it as missing.
	if (!texture)
	{
		RESOURCECREATOR_DEBUG_COUT( "FontAtlas {0} missing Texture {1}",id,f->font_texture_uid);
		clientrender::MissingResource& missing=geometryCache->GetMissingResource(f->font_texture_uid, avs::GeometryPayloadType::Texture);
		std::shared_ptr<IncompleteFontAtlas> i = f;
		missing.waitingResources.insert(i);
		f->missingTextureUid=fontAtlas.font_texture_uid;
	}
	// Was this resource being awaited?
	MissingResource* missingResource = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::FontAtlas);
	if(missingResource)
	for(auto it = missingResource->waitingResources.begin(); it != missingResource->waitingResources.end(); it++)
	{
		if(it->get()->type!=avs::GeometryPayloadType::TextCanvas)
		{
			TELEPORT_CERR<<"Waiting resource is not a TextCanvas, it's "<<int(it->get()->type)<<std::endl;
			continue;
		}
		std::shared_ptr<IncompleteTextCanvas> incompleteTextCanvas = std::static_pointer_cast<IncompleteTextCanvas>(*it);
		incompleteTextCanvas->missingFontAtlasUid=0;
		std::shared_ptr<TextCanvas> textCanvas = std::static_pointer_cast<TextCanvas>(*it);
		textCanvas->SetFontAtlas(f);
		RESOURCECREATOR_DEBUG_COUT( "Waiting TextCanvas {0}({1}) got FontAtlas {2}({3})" , incompleteNode->id,"",id,"");
		// The TextCanvas is complete
	}
}

void ResourceCreator::CreateTextCanvas(clientrender::TextCanvasCreateInfo &textCanvasCreateInfo)
{
	std::shared_ptr<clientrender::TextCanvas> textCanvas=geometryCache->mTextCanvasManager.Get(textCanvasCreateInfo.uid);
	if(!textCanvas)
	{
		textCanvas = std::make_shared<clientrender::TextCanvas>(textCanvasCreateInfo);
		geometryCache->mTextCanvasManager.Add(textCanvas->textCanvasCreateInfo.uid, textCanvas);
	}
	textCanvas->textCanvasCreateInfo=textCanvasCreateInfo;

	geometryCache->ReceivedResource(textCanvas->textCanvasCreateInfo.uid);
	
	const std::shared_ptr<clientrender::FontAtlas> fontAtlas = geometryCache->mFontAtlasManager.Get(textCanvas->textCanvasCreateInfo.font);
	// The fontAtlas hasn't arrived yet. Mark it as missing.
	if (!fontAtlas)
	{
		RESOURCECREATOR_DEBUG_COUT( "TextCanvas {0} missing fontAtlas {1}",id,textCanvas->textCanvasCreateInfo.font);
		clientrender::MissingResource& missing=geometryCache->GetMissingResource(textCanvas->textCanvasCreateInfo.font, avs::GeometryPayloadType::FontAtlas);
		std::shared_ptr<IncompleteTextCanvas> i = textCanvas;
		missing.waitingResources.insert(i);
		textCanvas->missingFontAtlasUid=textCanvas->textCanvasCreateInfo.font;
	}
	else
		textCanvas->SetFontAtlas(fontAtlas);

	// Was this resource being awaited?
	MissingResource* missingResource = geometryCache->GetMissingResourceIfMissing(textCanvas->textCanvasCreateInfo.uid, avs::GeometryPayloadType::TextCanvas);
	if(missingResource)
	for(auto it = missingResource->waitingResources.begin(); it != missingResource->waitingResources.end(); it++)
	{
		if(it->get()->type!=avs::GeometryPayloadType::Node)
		{
			TELEPORT_CERR<<"Waiting resource is not a node, it's "<<int(it->get()->type)<<std::endl;
			continue;
		}
		std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
		incompleteNode->SetTextCanvas(textCanvas);
		// MOdified "material" - add to transparent list.
		geometryCache->mNodeManager->NotifyModifiedMaterials(incompleteNode);
		RESOURCECREATOR_DEBUG_COUT( "Waiting Node {0}({1}) got Canvas {2}({3})" , incompleteNode->id,incompleteNode->name,textCanvas->textCanvasCreateInfo.uid,"");

		//If only this mesh and this function are pointing to the node, then it is complete.
		if(it->use_count() == 2)
		{
			CompleteNode(incompleteNode->id, incompleteNode);
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(textCanvas->textCanvasCreateInfo.uid);
}


void ResourceCreator::CreateSkin(avs::uid id, avs::Skin& skin)
{
	TELEPORT_INTERNAL_COUT ( "CreateSkin({0}, {1})",id,skin.name);
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
	incompleteSkin->skin = std::make_shared<clientrender::Skin>(skin.name, inverseBindMatrices, skin.boneIDs.size(), skin.skinTransform);

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

void ResourceCreator::CreateAnimation(avs::uid id, teleport::core::Animation& animation)
{
	RESOURCECREATOR_DEBUG_COUT("CreateAnimation({0}, {1})", id ,animation.name);
	geometryCache->ReceivedResource(id);

	std::vector<clientrender::BoneKeyframeList> boneKeyframeLists;
	boneKeyframeLists.reserve(animation.boneKeyframes.size());

	for(size_t i = 0; i < animation.boneKeyframes.size(); i++)
	{
		const teleport::core::TransformKeyframeList& avsKeyframes = animation.boneKeyframes[i];

		clientrender::BoneKeyframeList boneKeyframeList;
		boneKeyframeList.boneIndex = avsKeyframes.boneIndex;
		boneKeyframeList.positionKeyframes = avsKeyframes.positionKeyframes;
		boneKeyframeList.rotationKeyframes = avsKeyframes.rotationKeyframes;

		boneKeyframeLists.push_back(boneKeyframeList);
	}

	std::shared_ptr<clientrender::Animation> completeAnimation = std::make_shared<clientrender::Animation>(animation.name, boneKeyframeLists);
	CompleteAnimation(id, completeAnimation);
}

void ResourceCreator::CreateMeshNode(avs::uid id, avs::Node& avsNode)
{
	std::shared_ptr<Node> node;
	if (geometryCache->mNodeManager->HasNode(id))
	{
		TELEPORT_CERR << "CreateMeshNode(" << id << ", " << avsNode.name << "). Already created! "<<(avsNode.stationary?"static":"mobile")<<"\n";
		//leaves nodes with no children. why?
		auto n=geometryCache->mNodeManager->GetNode(id);
		if (n->GetChildrenIDs().size() != avsNode.childrenIDs.size())
		{
			TELEPORT_CERR << "recreating avsNode " << n->id << " with " << avsNode.childrenIDs.size() << " children." << std::endl;
		}
		node=geometryCache->mNodeManager->GetNode(id);
	}
	else
	{
		node = geometryCache->mNodeManager->CreateNode(id, avsNode);
	}
	//Whether the node is missing any resource before, and must wait for them before it can be completed.
	bool isMissingResources = false;


	if (avsNode.data_uid != 0)
	{
		if(avsNode.data_type==avs::NodeDataType::Mesh)
		{
			node->SetMesh(geometryCache->mMeshManager.Get(avsNode.data_uid));
			if (!node->GetMesh())
			{
			//RESOURCECREATOR_DEBUG_COUT( "MeshNode_" << id << "(" << avsNode.name << ") missing Mesh_" << avsNode.data_uid << std::endl;

				isMissingResources = true;
				geometryCache->GetMissingResource(avsNode.data_uid, avs::GeometryPayloadType::Mesh).waitingResources.insert(node);
			}
		}
		if(avsNode.data_type==avs::NodeDataType::TextCanvas)
		{
			node->SetTextCanvas(geometryCache->mTextCanvasManager.Get(avsNode.data_uid));
			if (!node->GetTextCanvas())
			{
			//RESOURCECREATOR_DEBUG_COUT( "MeshNode_" << id << "(" << avsNode.name << ") missing Mesh_" << avsNode.data_uid << std::endl;

				isMissingResources = true;
				geometryCache->GetMissingResource(avsNode.data_uid, avs::GeometryPayloadType::TextCanvas).waitingResources.insert(node);
			}
			else
				geometryCache->mNodeManager->NotifyModifiedMaterials(node);
		}
	}

	if (avsNode.skinID != 0)
	{
		auto skin=geometryCache->mSkinManager.Get(avsNode.skinID);
		node->SetSkin(skin);
		if (!skin)
		{
			//RESOURCECREATOR_DEBUG_COUT( "MeshNode_" << id << "(" << avsNode.name << ") missing Skin_" << avsNode.skinID << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(avsNode.skinID, avs::GeometryPayloadType::Skin).waitingResources.insert(node);
		}
	}

	for (size_t i = 0; i < avsNode.animations.size(); i++)
	{
		avs::uid animationID = avsNode.animations[i];
		std::shared_ptr<clientrender::Animation> animation = geometryCache->mAnimationManager.Get(animationID);

		if (animation)
		{
			node->animationComponent.addAnimation(animationID, animation);
		}
		else
		{
			RESOURCECREATOR_DEBUG_COUT( "MeshNode {0}({1} missing Animation {2})",id,avsNode.name,animationID);

			isMissingResources = true;
			geometryCache->GetMissingResource(animationID, avs::GeometryPayloadType::Animation).waitingResources.insert(node);
		}
	}

	if (placeholderMaterial == nullptr)
	{
		clientrender::Material::MaterialCreateInfo materialCreateInfo;
		materialCreateInfo.diffuse.texture = m_DummyWhite;
		materialCreateInfo.combined.texture = m_DummyCombined;
		materialCreateInfo.normal.texture = m_DummyNormal;
		//materialCreateInfo.emissive.texture = m_DummyBlack;
		materialCreateInfo.emissive.texture = m_DummyGreen;
		placeholderMaterial = std::make_shared<clientrender::Material>(renderPlatform,materialCreateInfo);
	}
	if(avsNode.renderState.globalIlluminationUid>0)
	{
		std::shared_ptr<clientrender::Texture> gi_texture = geometryCache->mTextureManager.Get(avsNode.renderState.globalIlluminationUid);
		if(!gi_texture)
		{
			isMissingResources = true;
			geometryCache->GetMissingResource(avsNode.renderState.globalIlluminationUid, avs::GeometryPayloadType::Texture).waitingResources.insert(node);
		}
	}
	for (size_t i = 0; i < avsNode.materials.size(); i++)
	{
		auto mat_uid=avsNode.materials[i];
		std::shared_ptr<clientrender::Material> material = geometryCache->mMaterialManager.Get(mat_uid);

		if (material)
		{
			node->SetMaterial(i, material);
			geometryCache->mNodeManager->NotifyModifiedMaterials(node);
		}
		else if(mat_uid!=0)
		{
			// If we don't know have the information on the material yet, we use placeholder OVR surfaces.
			node->SetMaterial(i, placeholderMaterial);

			TELEPORT_CERR << "MeshNode_" << id << "(" << avsNode.name << ") missing Material_" << avsNode.materials[i] << std::endl;

			isMissingResources = true;
			geometryCache->GetMissingResource(avsNode.materials[i], avs::GeometryPayloadType::Material).waitingResources.insert(node);
			node->materialSlots[avsNode.materials[i]].push_back(i);
		}
		else
		{
			// but if the material is uid 0, this means *no* material.
		}
	}
			

	//Complete node now, if we aren't missing any resources.
	if (!isMissingResources)
	{
		CompleteNode(id, node);
	}
}

void ResourceCreator::CreateLight(avs::uid id, avs::Node& node)
{
	RESOURCECREATOR_DEBUG_COUT( "CreateLight {0}({1})" , id , node.name);
	geometryCache->ReceivedResource(id);

	clientrender::Light::LightCreateInfo lci;
	lci.renderPlatform = renderPlatform;
	lci.type = (clientrender::Light::Type)node.lightType;
	lci.position = vec3(node.globalTransform.position);
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
	RESOURCECREATOR_DEBUG_COUT( "CreateBonet {0}({1})" , id ,node.name );
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
	//RESOURCECREATOR_DEBUG_COUT( "CompleteMesh(" << id << ", " << meshInfo.name << ")\n";

	std::shared_ptr<clientrender::Mesh> mesh = std::make_shared<clientrender::Mesh>(meshInfo);
	geometryCache->mMeshManager.Add(id, mesh);

	//Add mesh to nodes waiting for mesh.
	MissingResource* missingMesh = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Mesh);
	if (missingMesh)
	{
		for (auto it = missingMesh->waitingResources.begin(); it != missingMesh->waitingResources.end(); it++)
		{
			if (it->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_CERR << "Waiting resource is not a node, it's " << int(it->get()->type) << std::endl;
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetMesh(mesh);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got Mesh {2}({3})", incompleteNode->id, incompleteNode->name, id, meshInfo.name);

			//If only this mesh and this function are pointing to the node, then it is complete.
			if (it->use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::CompleteSkin(avs::uid id, std::shared_ptr<IncompleteSkin> completeSkin)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteSkin {0}({1})",id,completeSkin->skin->name);

	geometryCache->mSkinManager.Add(id, completeSkin->skin);

	//Add skin to nodes waiting for skin.
	MissingResource *missingSkin = geometryCache->GetMissingResourceIfMissing(id,avs::GeometryPayloadType::Skin);
	if(missingSkin)
	{
		for(auto it = missingSkin->waitingResources.begin(); it != missingSkin->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetSkin(completeSkin->skin);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Skin {0}({1})",incompleteNode->id,incompleteNode->name,id,completeSkin->skin->name);

			//If only this resource and this skin are pointing to the node, then it is complete.
			if(it->use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::CompleteTexture(avs::uid id, const clientrender::Texture::TextureCreateInfo& textureInfo)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteTexture {0}()",id,textureInfo.name,magic_enum::enum_name<clientrender::Texture::CompressionFormat>(textureInfo.compression));
	std::shared_ptr<clientrender::Texture> scrTexture = std::make_shared<clientrender::Texture>(renderPlatform);
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
				case avs::GeometryPayloadType::FontAtlas:
					{
						std::shared_ptr<IncompleteFontAtlas> incompleteFontAtlas = std::static_pointer_cast<IncompleteFontAtlas>(*it);
						RESOURCECREATOR_DEBUG_COUT("Waiting FontAtlas {0} got Texture {1}({2})",incompleteNode->id,id,textureInfo.name);

						geometryCache->CompleteResource(incompleteFontAtlas->id);
					}
					break;
				case avs::GeometryPayloadType::Material:
					{
						std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::static_pointer_cast<IncompleteMaterial>(*it);
						// Replacing this nonsense:
						//incompleteMaterial->textureSlots.at(id) = scrTexture;
						// with the correct:
						if(incompleteMaterial->materialInfo.diffuse.texture_uid==id)
							incompleteMaterial->materialInfo.diffuse.texture=scrTexture;
						if(incompleteMaterial->materialInfo.normal.texture_uid==id)
							incompleteMaterial->materialInfo.normal.texture=scrTexture;
						if(incompleteMaterial->materialInfo.combined.texture_uid==id)
							incompleteMaterial->materialInfo.combined.texture=scrTexture;
						if(incompleteMaterial->materialInfo.emissive.texture_uid==id)
							incompleteMaterial->materialInfo.emissive.texture=scrTexture;
						RESOURCECREATOR_DEBUG_COUT( "Waiting Material ",") got Texture ",incompleteMaterial->id,incompleteMaterial->materialInfo.name,id,textureInfo.name);

						//If only this texture and this function are pointing to the material, then it is complete.
						if (it->use_count() == 2)
						{
							CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
						}
						else
						{
							RESOURCECREATOR_DEBUG_COUT(" Still awaiting {0} resources.",(it->use_count()-2));
						}
					}
					break;
				case avs::GeometryPayloadType::Node:
					{
						std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
						RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Texture {2}({3})",incompleteNode->id,incompleteNode->name.c_str(),id,textureInfo.name);

						//If only this material and function are pointing to the MeshNode, then it is complete.
						if(incompleteNode.use_count() == 2)
						{
							CompleteNode(incompleteNode->id, incompleteNode);
						}
					}
					break;
				default:
					break;
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::CompleteMaterial(avs::uid id, const clientrender::Material::MaterialCreateInfo& materialInfo)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteMaterial {0}({1})",id,materialInfo.name);
	std::shared_ptr<clientrender::Material> material = geometryCache->mMaterialManager.Get(id);
	// Update its properties:
	material->SetMaterialCreateInfo(renderPlatform,materialInfo);
	//Add material to nodes waiting for material.
	MissingResource *missingMaterial = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Material);
	if(missingMaterial)
	{
		for(auto it = missingMaterial->waitingResources.begin(); it != missingMaterial->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);

			const auto &indexesPair = incompleteNode->materialSlots.find(id);
			if (indexesPair == incompleteNode->materialSlots.end())
			{
				TELEPORT_CERR<<"Material "<<id<<" not found in incomplete node "<<incompleteNode->id<<std::endl;
				continue;
			}
			for(size_t materialIndex : indexesPair->second)
			{
				incompleteNode->SetMaterial(materialIndex, material);
			}
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Material {2}({3})",incompleteNode->id,incompleteNode->name,id,materialInfo.name);

			//If only this material and function are pointing to the MeshNode, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::CompleteNode(avs::uid id, std::shared_ptr<clientrender::Node> node)
{
//	RESOURCECREATOR_DEBUG_COUT( "CompleteMeshNode(ID: ",id,", node: ",node->name,")\n";

	///We're using the node ID as the node ID as we are currently generating an node per node/transform anyway; this way the server can tell the client to remove an node.
	geometryCache->m_CompletedNodes.push_back(id);
}

void ResourceCreator::CompleteBone(avs::uid id, std::shared_ptr<clientrender::Bone> bone)
{
	//RESOURCECREATOR_DEBUG_COUT( "CompleteBone(",id,", ",bone->name,")\n";

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
				RESOURCECREATOR_DEBUG_COUT( "Waiting Skin {0}({1}) got Bone {2}({3})",incompleteSkin->id,incompleteSkin->skin->name,id,bone->name);

				//If only this bone, and the loop, are pointing at the skin, then it is complete.
				if(it->use_count() == 2)
				{
					CompleteSkin(incompleteSkin->id, incompleteSkin);
				}
			}
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::CompleteAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteAnimation {0}({1})",id,animation->name);

	//Update animation length before adding to the animation manager.
	animation->updateAnimationLength();
	geometryCache->mAnimationManager.Add(id, animation);

	//Add animation to waiting nodes.
	MissingResource *missingAnimation = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Animation);
	if(missingAnimation)
	{
		for(auto it = missingAnimation->waitingResources.begin(); it != missingAnimation->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->animationComponent.addAnimation(id, animation);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Animation {2}({3})",incompleteNode->id,incompleteNode->name,id,animation->name);

			//If only this bone, and the loop, are pointing at the skin, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}

	}
	//Resource has arrived, so we are no longer waiting for it.
	geometryCache->CompleteResource(id);
}

void ResourceCreator::AddTextureToMaterial(const avs::TextureAccessor& accessor, const vec4& colourFactor, const std::shared_ptr<clientrender::Texture>& dummyTexture,
										   std::shared_ptr<IncompleteMaterial> incompleteMaterial, clientrender::Material::MaterialParameter& materialParameter)
{
	materialParameter.texture_uid=accessor.index;
	if (accessor.index != 0)
	{
		const std::shared_ptr<clientrender::Texture> texture = geometryCache->mTextureManager.Get(accessor.index);

		if (texture)
		{
			materialParameter.texture = texture;
		}
		else
		{
			RESOURCECREATOR_DEBUG_COUT( "Material {0}({1}) missing Texture ",incompleteMaterial->id,"(",incompleteMaterial->id,accessor.index);
			clientrender::MissingResource& missing=geometryCache->GetMissingResource(accessor.index, avs::GeometryPayloadType::Texture);
			missing.waitingResources.insert(incompleteMaterial);
			incompleteMaterial->missingTextureUids.insert(accessor.index);
		}

		vec2 tiling = { accessor.tiling.x, accessor.tiling.y };

		materialParameter.texCoordsScalar[0] = tiling;
		materialParameter.texCoordsScalar[1] = tiling;
		materialParameter.texCoordsScalar[2] = tiling;
		materialParameter.texCoordsScalar[3] = tiling;
		materialParameter.texCoordIndex = static_cast<float>(accessor.texCoord);
	}
	else
	{
		materialParameter.texture = dummyTexture;
		materialParameter.texCoordsScalar[0] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[1] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[2] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[3] = vec2(1.0f, 1.0f);
		materialParameter.texCoordIndex = 0.0f;
	}

	materialParameter.textureOutputScalar = *((vec4*)&colourFactor);
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
	SetThisThreadName("BasisThread_TranscodeTextures");
	while (shouldBeTranscoding)
	{
		//std::this_thread::yield(); //Yield at the start, as we don't want to yield before we unlock (when lock goes out of scope).

		//Copy the data out of the shared data structure and minimise thread stalls due to mutexes
		std::vector<UntranscodedTexture> texturesToTranscode_Internal;
		texturesToTranscode_Internal.reserve(texturesToTranscode.size());
		{
			std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
			texturesToTranscode_Internal.insert(texturesToTranscode_Internal.end(), texturesToTranscode.begin(), texturesToTranscode.end());
			texturesToTranscode.clear();
		}

		for (UntranscodedTexture& transcoding : texturesToTranscode_Internal)
		{
			if (transcoding.compressionFormat == avs::TextureCompression::PNG)
			{
				RESOURCECREATOR_DEBUG_COUT("Transcoding  {0}with PNG",transcoding.name.c_str());
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
				transcoding.textureCI->images.resize(num_images);
				for(int i=0;i<num_images;i++)
				{
					// Convert from Png to raw data:
					int num_channels=0;
					unsigned char *target = teleport::stbi_load_from_memory(basePtr+imageOffsets[i],(int) imageSizes[i], &mipWidth, &mipHeight, &num_channels,(int)0);
					if( mipWidth > 0 && mipHeight > 0&&target&&transcoding.data.size()>2)
					{
						// this is for 8-bits-per-channel textures:
						size_t outDataSize = (size_t)(mipWidth * mipHeight * num_channels);
						transcoding.textureCI->images[i].resize(outDataSize);
						memcpy(transcoding.textureCI->images[i].data(), target, outDataSize);
						transcoding.textureCI->valueScale=transcoding.valueScale;

					}
					else
					{
						TELEPORT_CERR << "Failed to transcode PNG-format texture \"" << transcoding.name << "\"." << std::endl;
					}
					teleport::stbi_image_free(target);
				}
				if (transcoding.textureCI->images.size() != 0)
				{
					CompleteTexture(transcoding.texture_uid, *(transcoding.textureCI));
				}
				else
				{
					TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to transcode, no images found." << std::endl;
				}
			}
			else if(transcoding.compressionFormat==avs::TextureCompression::BASIS_COMPRESSED)
			{
				RESOURCECREATOR_DEBUG_COUT("Transcoding {0} with BASIS",transcoding.name.c_str());
				//We need a new transcoder for every .basis file.
				basist::basisu_transcoder basis_transcoder;
				basist::basisu_file_info fileinfo;
				if (!basis_transcoder.get_file_info(transcoding.data.data(), (uint32_t)transcoding.data.size(), fileinfo))
				{
					TELEPORT_CERR << "Failed to transcode texture \"" << transcoding.name << "\"." << std::endl;
					continue;
				}
				BasisValidate(basis_transcoder, fileinfo,transcoding.data);
				if (basis_transcoder.start_transcoding(transcoding.data.data(), (uint32_t)transcoding.data.size()))
				{
					transcoding.textureCI->mipCount = basis_transcoder.get_total_image_levels(transcoding.data.data(), (uint32_t)transcoding.data.size(), 0);
					transcoding.textureCI->images.resize(transcoding.textureCI->mipCount*transcoding.textureCI->arrayCount);

					if (!basis_is_format_supported(basis_transcoder_textureFormat, fileinfo.m_tex_format))
					{
						TELEPORT_CERR << "Failed to transcode texture \"" << transcoding.name << "\"." << std::endl;
						continue;
					}
					int imageIndex=0;
					for (uint32_t arrayIndex = 0; arrayIndex < transcoding.textureCI->arrayCount; arrayIndex++)
					{
						for (uint32_t mipIndex = 0; mipIndex < transcoding.textureCI->mipCount; mipIndex++)
						{
							uint32_t basisWidth, basisHeight, basisBlocks;

							basis_transcoder.get_image_level_desc(transcoding.data.data(), (uint32_t)transcoding.data.size(), arrayIndex, mipIndex, basisWidth, basisHeight, basisBlocks);
							uint32_t outDataSize = basist::basis_get_bytes_per_block_or_pixel(basis_transcoder_textureFormat) * basisBlocks;
							auto &img=transcoding.textureCI->images[imageIndex];
							img.resize(outDataSize);
							if (!basis_transcoder.transcode_image_level(transcoding.data.data(), (uint32_t)transcoding.data.size(),arrayIndex, mipIndex, img.data(), basisBlocks, basis_transcoder_textureFormat))
							{
								TELEPORT_CERR << "Texture \"" << transcoding.name << "\" failed to transcode mipmap level " << mipIndex << "." << std::endl;
							}
							imageIndex++;
						}
					}

					if (transcoding.textureCI->images.size() != 0)
					{
						CompleteTexture(transcoding.texture_uid, *(transcoding.textureCI));
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
		}
	}
}
