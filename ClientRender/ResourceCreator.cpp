// (C) Copyright 2018-2022 Simul Software Ltd
#include "ResourceCreator.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4018) //warning C4018: '<': signed/unsigned mismatch
#endif

#include "Animation.h"
#include "Material.h"
#include <magic_enum/magic_enum.hpp>
#include "ThisPlatform/Threads.h"
#include "TeleportCore/Logging.h"
#include "draco/compression/decode.h"
#include <fmt/core.h>
#include <fstream>
#include <ktx.h>
#include <ktxint.h>
#include <texture2.h>
#include <ktx/lib/gl_format.h>
#include "NodeComponents/SubSceneComponent.h"
#include "TeleportClient/Config.h"
#ifdef _MSC_VER
#define isinf !_finite
#else
#include <cmath> // for isinf()
#endif
using namespace std::chrono_literals;
#pragma optimize("", off)

// #define STB_IMAGE_IMPLEMENTATION
namespace teleport
{
#include "stb_image.h"
}
using namespace teleport;
using namespace clientrender;

#define RESOURCECREATOR_DEBUG_COUT(txt, ...) //TELEPORT_INTERNAL_COUT(txt,##__VA_ARGS__)
template<typename T> void read_from_buffer(T &result,const uint8_t * &mem)
{
	const T *src=(const T*)mem;
	result=*src;
	src++;
	mem=(const uint8_t*)src;
}


ResourceCreator::ResourceCreator()
	:textureTranscodeThread(&ResourceCreator::thread_TranscodeTextures, this)
{
}

ResourceCreator::~ResourceCreator()
{
	//Safely close the transcoding thread.
	shouldBeTranscoding = false;
	textureTranscodeThread.join();
}

ResourceCreator &ResourceCreator::GetInstance()
{
	static ResourceCreator resourceCreator;
	return resourceCreator;
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
		1, 1,
		//clientrender::Texture::Slot::UNKNOWN,
		clientrender::Texture::Type::TEXTURE_2D,
		clientrender::Texture::Format::RGBA8,
		clientrender::Texture::SampleCountBit::SAMPLE_COUNT_1_BIT,
		nullptr,
		clientrender::Texture::CompressionFormat::UNCOMPRESSED,
		false
	};

	tci.images = std::make_shared<std::vector<std::vector<uint8_t>>>(1) ;
	tci.name = "whiteBGRA";
	(*tci.images)[0].resize(sizeof(whiteBGRA));
	memcpy((*tci.images)[0].data(), &whiteBGRA, sizeof(whiteBGRA));
	m_DummyWhite->Create(tci);

	tci.images = std::make_shared<std::vector<std::vector<uint8_t>>>(1);
	tci.name = "normalRGBA";
	(*tci.images)[0].resize(sizeof(normalRGBA));
	memcpy((*tci.images)[0].data(), &normalRGBA, sizeof(normalRGBA));
	m_DummyNormal->Create(tci);

	tci.images = std::make_shared<std::vector<std::vector<uint8_t>>>(1);
	tci.name = "combinedBGRA";
	(*tci.images)[0].resize(sizeof(combinedBGRA));
	memcpy((*tci.images)[0].data(), &combinedBGRA, sizeof(combinedBGRA));
	m_DummyCombined->Create(tci);

	tci.images = std::make_shared<std::vector<std::vector<uint8_t>>>(1);
	tci.name = "blackBGRA";
	(*tci.images)[0].resize(sizeof(blackBGRA));
	memcpy((*tci.images)[0].data(), &blackBGRA, sizeof(blackBGRA));
	m_DummyBlack->Create(tci);

	const size_t GRID=128;
	tci.images = std::make_shared<std::vector<std::vector<uint8_t>>>(1);
	(*tci.images)[0].resize(sizeof(uint32_t)* GRID* GRID);
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
	memcpy((*tci.images)[0].data(), &green_grid, sizeof(green_grid));
	m_DummyGreen->Create(tci);
}

void ResourceCreator::Clear()
{
	mutex_texturesToTranscode.lock();
	texturesToTranscode.clear();
	mutex_texturesToTranscode.unlock();
}

template<typename indexType>
bool GenerateTangents(avs::MeshElementCreate& meshElementCreate,std::vector<vec4> &tangent)
{
//void CalculateTangentArray(long vertexCount, const Point3D *vertex, const Vector3D *normal,
      //  const Point2D *texcoord, long triangleCount, const Triangle *triangle, Vector4D *tangent)
	  long vertexCount=meshElementCreate.m_VertexCount;
	std::vector<vec3> tan1(vertexCount * 2);
    vec3 *tan2 = tan1.data() + vertexCount;
    memset(tan1.data(),0,vertexCount * sizeof(vec3) * 2);
    const vec2 *texcoord=meshElementCreate.m_UV0s;
	const indexType *indices=(const indexType *)meshElementCreate.m_Indices;
    for (long a = 0; a < meshElementCreate.m_IndexCount/3; a++)
    {
        long i1 = indices[a*3+0];
        long i2 = indices[a*3+1];
        long i3 = indices[a*3+2];
        
        const vec3& v1 = meshElementCreate.m_Vertices[i1];
        const vec3& v2 = meshElementCreate.m_Vertices[i2];
        const vec3& v3 = meshElementCreate.m_Vertices[i3];
        
        const vec2& w1 = texcoord[i1];
        const vec2& w2 = texcoord[i2];
        const vec2& w3 = texcoord[i3];
        
        float x1 = v2.x - v1.x;
        float x2 = v3.x - v1.x;
        float y1 = v2.y - v1.y;
        float y2 = v3.y - v1.y;
        float z1 = v2.z - v1.z;
        float z2 = v3.z - v1.z;
        
        float s1 = w2.x - w1.x;
        float s2 = w3.x - w1.x;
        float t1 = w2.y - w1.y;
        float t2 = w3.y - w1.y;
        
        float r = 1.0F / (s1 * t2 - s2 * t1);
		if(isinf(r))
			r=1.0f;
        vec3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r);
        vec3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r);
        
        tan1[i1] += sdir;
        tan1[i2] += sdir;
        tan1[i3] += sdir;
        
        tan2[i1] += tdir;
        tan2[i2] += tdir;
        tan2[i3] += tdir;
    }
    for (long a = 0; a < vertexCount; a++)
    {
        const vec3& n = meshElementCreate.m_Normals[a];
        const vec3& t = tan1[a];
        // Gram-Schmidt orthogonalize
        vec3 t3=normalize((t - n * dot(n, t)));
        tangent[a].xyz=t3;
        // Calculate handedness
        tangent[a].w = (dot(cross(n, t), tan2[a]) < 0.0F) ? -1.0F : 1.0F;
    }
	return true;
}

void ResourceCreator::CreateLinkNode( avs::uid server_uid, avs::uid id, const avs::Node &avsNode)
{
	std::shared_ptr<GeometryCache> geometryCache = GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	std::shared_ptr<Node> node;
	if (geometryCache->mNodeManager.HasNode(id))
	{
	}
	else
	{
		std::lock_guard g(geometryCache->missingResourcesMutex);
		node = geometryCache->mNodeManager.CreateNode(geometryCache->GetSessionTimeUs(), id, avsNode);
		geometryCache->CompleteNode(node->id, node);
	}
}

avs::Result ResourceCreator::CreateMesh(avs::MeshCreate& meshCreate)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(meshCreate.cache_uid);
	geometryCache->ReceivedResource(meshCreate.mesh_uid);
	if (!renderPlatform)
	{
		TELEPORT_CERR << "No valid render platform was found." << std::endl;
		return avs::Result::GeometryDecoder_ClientRendererError;
	}
	clientrender::Mesh::MeshCreateInfo mesh_ci;
	mesh_ci.inverseBindMatrices=meshCreate.inverseBindMatrices;
	mesh_ci.name = meshCreate.name;
	mesh_ci.id = meshCreate.mesh_uid;
	mesh_ci.clockwiseFaces=meshCreate.clockwiseFaces;
	size_t num=meshCreate.m_MeshElementCreate.size();
	mesh_ci.vertexBuffers.resize(num);
	mesh_ci.indexBuffers.resize(num);
	static std::vector<uint8_t> empty_data;
	static std::vector<vec4> default_tangents;
	for (size_t i = 0; i < num; i++)
	{
		avs::MeshElementCreate& meshElementCreate = meshCreate.m_MeshElementCreate[i];
		if(meshElementCreate.internalMaterial)
		{
			mesh_ci.internalMaterials.push_back(std::make_shared<Material::MaterialCreateInfo>());
			auto &materialInfo=mesh_ci.internalMaterials.back();
			
			auto &avsMaterial=meshElementCreate.internalMaterial;
			materialInfo->name=avsMaterial->name;
			materialInfo->materialMode=avsMaterial->materialMode;
			materialInfo->diffuse.textureOutputScalar=avsMaterial->pbrMetallicRoughness.baseColorFactor;
		}
		size_t indicesByteSize = meshElementCreate.m_IndexCount * meshElementCreate.m_IndexSize;
		std::shared_ptr<std::vector<uint8_t>> _indices = std::make_unique<std::vector<uint8_t>>(indicesByteSize);
		memcpy(_indices->data(), meshElementCreate.m_Indices, indicesByteSize);
		// Our standard materials have either five, or seven inputs. So if we don't have those,
		// we must create dummy inputs.
	/*	if(!meshElementCreate.m_UV0s)
		{
			if(empty_data.size()<meshElementCreate.m_VertexCount*sizeof(vec2))
			{
				size_t old_size=empty_data.size();
				empty_data.resize(meshElementCreate.m_VertexCount*sizeof(vec2),0);
			}
			meshElementCreate.m_UV0s=(const vec2*)empty_data.data();
		}
		if(!meshElementCreate.m_UV1s)
		{
			meshElementCreate.m_UV1s=meshElementCreate.m_UV0s;
		}
		if(!meshElementCreate.m_Tangents)
		{
			if(default_tangents.size()<meshElementCreate.m_VertexCount)
			{
				size_t old_size=default_tangents.size();
				default_tangents.resize(meshElementCreate.m_VertexCount);
				for(size_t s=old_size;s<default_tangents.size();s++)
					default_tangents[s]={1.f,0,0,0};
			}
			bool ok=false;
			if(meshElementCreate.m_IndexSize==4)
				ok=GenerateTangents<uint32_t>(meshElementCreate,default_tangents);
			else if(meshElementCreate.m_IndexSize==2)
				ok=GenerateTangents<uint16_t>(meshElementCreate,default_tangents);
			if(!ok)
				continue;
			meshElementCreate.m_Tangents=default_tangents.data();
		}*/
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
		//if(layout->m_Attributes.size()!=5&&layout->m_Attributes.size()!=7)
		//	continue;
		layout->CalculateStride();
		layout->m_PackingStyle = this->m_PackingStyle;

		size_t constructedVBByteSize = layout->m_Stride * meshElementCreate.m_VertexCount;

		std::shared_ptr<std::vector<uint8_t>> constructedVB = std::make_shared<std::vector<uint8_t>>(constructedVBByteSize);

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
						tvector4<signed char>& x8 = *((tvector4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						tvector4<signed char>& n8 = *((tvector4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						tvector4<short>& x8 = *((tvector4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						tvector4<short>& n8 = *((tvector4<short>*)(nt + 8));
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
					size_t typeSize	=	4*avs::GetSizeOfComponentType(meshElementCreate.colourType);
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, (uint8_t*)meshElementCreate.m_Colors + j*typeSize, typeSize);
					intraStrideOffset += typeSize;
				}
				if (meshElementCreate.m_Joints)
				{
					// TODO: Fully handle all possible component types: don't just expect floats or shorts, and implement this for all attributes.
					size_t typeSize	=	sizeof(vec4);
					float *target=(float *)(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset);
					if(meshElementCreate.jointType==avs::ComponentType::FLOAT)
					{
						memcpy(target, (uint8_t*)meshElementCreate.m_Joints + j*sizeof(vec4), sizeof(vec4));
					}
					if(meshElementCreate.jointType==avs::ComponentType::USHORT)
					{
						unsigned short *source = (unsigned short *)meshElementCreate.m_Joints + j*4;
						for(int e=0;e<4;e++)
						{
							target[e]=(double)source[e];
						}
					}
					intraStrideOffset += typeSize;
				}
				if (meshElementCreate.m_Weights)
				{
					size_t typeSize	=4*avs::GetSizeOfComponentType(meshElementCreate.weightType);
					memcpy(constructedVB->data() + (layout->m_Stride  * j) + intraStrideOffset, (uint8_t*)meshElementCreate.m_Weights + j*typeSize, typeSize);
					intraStrideOffset += typeSize;
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
						tvector4<signed char>& x8 = *((tvector4<signed char>*)(nt));
						tangent.x = float(x8.x) / 127.0f;
						tangent.y = float(x8.y) / 127.0f;
						tangent.z = float(x8.z) / 127.0f;
						tangent.w = float(x8.w) / 127.0f;
						tvector4<signed char>& n8 = *((tvector4<signed char>*)(nt + 4));
						normal.x = float(n8.x) / 127.0f;
						normal.y = float(n8.y) / 127.0f;
						normal.z = float(n8.z) / 127.0f;
					}
					else // 16
					{
						tvector4<short>& x8 = *((tvector4<short>*)(nt));
						tangent.x = float(x8.x) / 32767.0f;
						tangent.y = float(x8.y) / 32767.0f;
						tangent.z = float(x8.z) / 32767.0f;
						tangent.w = float(x8.w) / 32767.0f;
						tvector4<short>& n8 = *((tvector4<short>*)(nt + 8));
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
				size_t typeSize	=	avs::GetSizeOfComponentType(meshElementCreate.colourType);
				size_t size = 4*typeSize * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Colors, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Joints)
			{
				size_t typeSize	=	avs::GetSizeOfComponentType(meshElementCreate.jointType);
				size_t size = 4*typeSize * meshElementCreate.m_VertexCount;
				assert(constructedVBByteSize >= vertexBufferOffset + size);
				memcpy(constructedVB->data() + vertexBufferOffset, meshElementCreate.m_Joints, size);
				vertexBufferOffset += size;
			}
			if (meshElementCreate.m_Weights)
			{
				size_t typeSize	=	avs::GetSizeOfComponentType(meshElementCreate.weightType);
				size_t size = 4*typeSize * meshElementCreate.m_VertexCount;
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

		avs::uid vb_uid = geometryCache->GenerateUid(meshElementCreate.vb_id);
		geometryCache->mVertexBufferManager.Add(vb_uid, vb);
		//TELEPORT_CERR << "Cache " << meshCreate.cache_uid << " Mesh " << mesh_ci.name << " has vertex buffer " << vb_uid << std::endl;
		avs::uid ib_uid = geometryCache->GenerateUid(meshElementCreate.ib_id);
		geometryCache->mIndexBufferManager.Add(ib_uid, ib);
		//TELEPORT_CERR << "Cache " << meshCreate.cache_uid << " Mesh " << mesh_ci.name << " has index buffer " << ib_uid << std::endl;

		mesh_ci.vertexBuffers[i] = vb;
		mesh_ci.indexBuffers[i] = ib;
	}
	if (!geometryCache->mMeshManager.Has(meshCreate.mesh_uid))
	{
		geometryCache->CompleteMesh(meshCreate.mesh_uid, mesh_ci);
	}

	return avs::Result::OK;
}

avs::Result ResourceCreator::CreateSubScene(avs::uid server_uid,const SubSceneCreate& subSceneCreate)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	std::shared_ptr<GeometryCache> subSceneCache=GeometryCache::GetGeometryCache(subSceneCreate.subscene_cache_uid);
	return geometryCache->CreateSubScene(subSceneCreate);
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


void ResourceCreator::CreateTexture(avs::uid server_uid,avs::uid id, const avs::Texture& texture)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	if(geometryCache->mTextureManager.Get(id))
	{
		return;
	}
	std::shared_ptr<clientrender::Texture::TextureCreateInfo> texInfo =std::make_shared<clientrender::Texture::TextureCreateInfo>();
	texInfo->name			=texture.name;
	texInfo->uid			=id;
	texInfo->compression	=clientrender::Texture::CompressionFormat::UNCOMPRESSED;
	texInfo->width=texture.width;
	texInfo->height=texture.height;

	TELEPORT_LOG("Received texture {0} ({1}), awaiting decompression.",id, texture.name);
	{
		std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
		texturesToTranscode.emplace_back(std::make_shared<UntranscodedTexture>(server_uid, id, texture.compressedData, texInfo, texture.name, texture.compression, texture.valueScale));
	}
}

void ResourceCreator::CreateMaterial(avs::uid server_uid,avs::uid id, const avs::Material& material)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	if(id)
		geometryCache->ReceivedResource(id);
	if(geometryCache->mMaterialManager.Get(id))
		return;
	std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::make_shared<IncompleteMaterial>(id, "",avs::GeometryPayloadType::Material);
	//A list of unique resources that the material is missing, and needs to be completed.
 	std::set<avs::uid> missingResources;

	incompleteMaterial->ResetMissingResourceCount();
	incompleteMaterial->materialInfo.uid = id;
	incompleteMaterial->materialInfo.name = material.name;
	incompleteMaterial->materialInfo.materialMode=material.materialMode;
	incompleteMaterial->materialInfo.doubleSided=material.doubleSided;
	
	incompleteMaterial->materialInfo.lightmapTexCoordIndex=material.lightmapTexCoordIndex;
	//Colour/Albedo/Diffuse
	geometryCache->AddTextureToMaterial(material.pbrMetallicRoughness.baseColorTexture,
		*((vec4*)&material.pbrMetallicRoughness.baseColorFactor),
		m_DummyWhite,
		incompleteMaterial,
		incompleteMaterial->materialInfo.diffuse);

	//Normal
	geometryCache->AddTextureToMaterial(material.normalTexture,
		vec4{ material.normalTexture.scale, material.normalTexture.scale, 1.0f, 1.0f },
		m_DummyNormal,
		incompleteMaterial,
		incompleteMaterial->materialInfo.normal);

	//Combined
	if(material.occlusionTexture.index)
		geometryCache->AddTextureToMaterial(material.occlusionTexture,
			vec4{ 0,0, material.occlusionTexture.strength,1.0f},
			m_DummyCombined,
			incompleteMaterial,
			incompleteMaterial->materialInfo.combined);
	else
		incompleteMaterial->materialInfo.combined.textureOutputScalar={0,0,0,1.0f};
	
	//if(material.pbrMetallicRoughness.metallicRoughnessTexture.index)
		geometryCache->AddTextureToMaterial(material.pbrMetallicRoughness.metallicRoughnessTexture,
			vec4{ material.pbrMetallicRoughness.roughnessMultiplier, material.pbrMetallicRoughness.metallicFactor, material.occlusionTexture.strength, material.pbrMetallicRoughness.roughnessOffset },
			m_DummyCombined,
			incompleteMaterial,
			incompleteMaterial->materialInfo.combined);

	//Emissive
	geometryCache->AddTextureToMaterial(material.emissiveTexture,
		vec4(material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z, 1.0f),
		m_DummyWhite,
		incompleteMaterial,
		incompleteMaterial->materialInfo.emissive);
// Add it to the manager, even if incomplete.
	std::shared_ptr<clientrender::Material> scrMaterial = std::make_shared<clientrender::Material>(incompleteMaterial->materialInfo);
	geometryCache->mMaterialManager.Add(id, scrMaterial);
	scrMaterial->id = id;
	if (incompleteMaterial->missingTextureUids.size() == 0)
	{
		std::lock_guard g(geometryCache->missingResourcesMutex);
		geometryCache->CompleteMaterial( id, incompleteMaterial->materialInfo);
	}
	else
	{
		std::string texlist;
		for(auto t_uid:incompleteMaterial->missingTextureUids)
		{
			texlist+=fmt::format("{0},",t_uid);
		}
		TELEPORT_INTERNAL_COUT("CreateMaterial {0} ({1}) as incomplete: missing textures: {2} awaiting {3} resources.", id, material.name, texlist, RESOURCES_AWAITED(incompleteMaterial));
	}
}

void ResourceCreator::CreateFontAtlas(avs::uid server_uid,avs::uid id,const std::string &path,teleport::core::FontAtlas &fontAtlas)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	if(geometryCache->mFontAtlasManager.Get(id))
		return;
	std::lock_guard g(geometryCache->missingResourcesMutex);
	std::shared_ptr<clientrender::FontAtlas> f = std::make_shared<clientrender::FontAtlas>(id,path);
	clientrender::FontAtlas &F=*f;
	*(static_cast<teleport::core::FontAtlas*>(&F))=fontAtlas;
	geometryCache->mFontAtlasManager.Add(id, f);

	const std::shared_ptr<clientrender::Texture> texture = geometryCache->mTextureManager.Get(fontAtlas.font_texture_uid);
	// The texture hasn't arrived yet. Mark it as missing.
	if (!texture)
	{
		RESOURCECREATOR_DEBUG_COUT("FontAtlas {0} missing Texture {1}", id, f->font_texture_uid);
		clientrender::MissingResource& missing=geometryCache->GetMissingResource(f->font_texture_uid, avs::GeometryPayloadType::Texture);
		std::shared_ptr<IncompleteFontAtlas> i = f;
		RESOURCE_AWAITS(i, f->font_texture_uid);
		missing.waitingResources.insert(i);
		RESOURCE_AWAITS(f,f->font_texture_uid);
		f->missingTextureUid=fontAtlas.font_texture_uid;
	}
	else
		geometryCache->CompleteFontAtlas(id,f);
}

void ResourceCreator::CreateTextCanvas(clientrender::TextCanvasCreateInfo &textCanvasCreateInfo)
{
	std::shared_ptr<GeometryCache> geometryCache = GeometryCache::GetGeometryCache(textCanvasCreateInfo.server_uid);
	geometryCache->ReceivedResource(textCanvasCreateInfo.uid);
	std::shared_ptr<clientrender::TextCanvas> textCanvas=geometryCache->mTextCanvasManager.Get(textCanvasCreateInfo.uid);
	if(!textCanvas)
	{
		textCanvas = std::make_shared<clientrender::TextCanvas>(textCanvasCreateInfo);
		geometryCache->mTextCanvasManager.Add(textCanvas->textCanvasCreateInfo.uid, textCanvas);
	}
	else
		return;
	textCanvas->textCanvasCreateInfo=textCanvasCreateInfo;
	
	std::lock_guard g(geometryCache->missingResourcesMutex);
	const std::shared_ptr<clientrender::FontAtlas> fontAtlas = geometryCache->mFontAtlasManager.Get(textCanvas->textCanvasCreateInfo.font);
	// The fontAtlas hasn't arrived yet. Mark it as missing.
	if (!fontAtlas)
	{
		RESOURCECREATOR_DEBUG_COUT( "TextCanvas {0} missing fontAtlas {1}", textCanvasCreateInfo.uid,textCanvas->textCanvasCreateInfo.font);
		clientrender::MissingResource& missing=geometryCache->GetMissingResource(textCanvas->textCanvasCreateInfo.font, avs::GeometryPayloadType::FontAtlas);
		std::shared_ptr<IncompleteTextCanvas> i = textCanvas;
		missing.waitingResources.insert(i);
		textCanvas->missingFontAtlasUid=textCanvas->textCanvasCreateInfo.font;
	}
	else
	{
		textCanvas->SetFontAtlas(fontAtlas);
		geometryCache->CompleteTextCanvas(textCanvasCreateInfo.uid);
	}
}

void ResourceCreator::CreateSkeleton(avs::uid server_uid,avs::uid id, const avs::Skeleton& skeleton)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	if(geometryCache->mSkeletonManager.Get(id))
		return;
	TELEPORT_INTERNAL_COUT ( "CreateSkeleton({0}, {1})",id,skeleton.name);

	std::shared_ptr<IncompleteSkeleton> incompleteSkeleton = std::make_shared<IncompleteSkeleton>(id,skeleton.name, avs::GeometryPayloadType::Skeleton);

	incompleteSkeleton->ResetMissingResourceCount();
	//Create skeleton.
	incompleteSkeleton->skeleton = std::make_shared<clientrender::Skeleton>(id,skeleton.name, skeleton.boneIDs.size());

	std::vector<avs::uid> bone_ids;
	bone_ids.resize(skeleton.boneIDs.size());
	incompleteSkeleton->skeleton->SetRootId(skeleton.rootBoneId);
	incompleteSkeleton->skeleton->SetExternalBoneIds(skeleton.boneIDs);
	std::lock_guard g(geometryCache->missingResourcesMutex);
	// each bone that hasn't been loaded is a missing resource. The Skelsseton can only be completed when all missing bones are here.
	// Don't include the root, as that would be a circular reference.
	for (int i=1;i<skeleton.boneIDs.size();i++)
	{
		avs::uid b=skeleton.boneIDs[i];
		if(!geometryCache->mNodeManager.GetNode(b))
		{
			RESOURCECREATOR_DEBUG_COUT("Skeleton {0}({1}) missing bone Node {2}", id, incompleteSkeleton->skeleton->getName(), b);
			clientrender::MissingResource &missing = geometryCache->GetMissingResource(b, avs::GeometryPayloadType::Node);
			missing.waitingResources.insert(incompleteSkeleton);
			RESOURCE_AWAITS(incompleteSkeleton, b);
			incompleteSkeleton->missingBones.insert(b);
		}
	}
//	incompleteSkeleton->skeleton->SetInverseBindMatrices(skeleton.inverseBindMatrices);
	// Add it to the manager, even if incomplete.
	geometryCache->mSkeletonManager.Add(id, incompleteSkeleton->skeleton);
	if(incompleteSkeleton->missingBones.size() == 0)
	{
		geometryCache->CompleteSkeleton(id, incompleteSkeleton);
	}
	MissingResource *missingResource = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Skeleton);
	if (missingResource)
		for (auto waiting = missingResource->waitingResources.begin(); waiting != missingResource->waitingResources.end(); waiting++)
		{
			if (waiting->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_CERR << "Waiting resource is not a node, it's " << int(waiting->get()->type) << std::endl;
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*waiting);
			incompleteNode->SetSkeleton(incompleteSkeleton->skeleton);
			incompleteNode->GetOrCreateComponent<AnimationComponent>();
			RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Skeleton {2}({3})", incompleteNode->id, incompleteNode->name, id, "");

			// If the waiting resource has no incomplete resources, it is now itself complete.
			if (RESOURCE_IS_COMPLETE(*waiting))
			{
				geometryCache->CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
}

avs::Result ResourceCreator::CreateAnimation(avs::uid server_uid,avs::uid id, teleport::core::Animation& animation)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	if(geometryCache->mAnimationManager.Get(id))
		return avs::Result::OK;
	if(animation.compressedData.size()>0)
	{
		std::shared_ptr<clientrender::Animation> completeAnimation = std::make_shared<clientrender::Animation>(animation.name);
		completeAnimation->LoadFromGlb(animation.compressedData.data(),animation.compressedData.size());
		geometryCache->CompleteAnimation(id, completeAnimation);
	}
	else
	{
		RESOURCECREATOR_DEBUG_COUT("CreateAnimation({0}, {1})", id ,animation.name);

		std::vector<clientrender::BoneKeyframeList> boneKeyframeLists;
		boneKeyframeLists.reserve(animation.boneKeyframes.size());

		for(size_t i = 0; i < animation.boneKeyframes.size(); i++)
		{
			const teleport::core::TransformKeyframeList& avsKeyframes = animation.boneKeyframes[i];

			clientrender::BoneKeyframeList boneKeyframeList(animation.duration);
			boneKeyframeList.boneIndex = avsKeyframes.boneIndex;
			boneKeyframeList.positionKeyframes = avsKeyframes.positionKeyframes;
			boneKeyframeList.rotationKeyframes = avsKeyframes.rotationKeyframes;

			boneKeyframeLists.push_back(boneKeyframeList);
		}

		std::shared_ptr<clientrender::Animation> completeAnimation = std::make_shared<clientrender::Animation>(animation.name,animation.duration,boneKeyframeLists);
		geometryCache->CompleteAnimation(id, completeAnimation);
	}
	return avs::Result::OK;
}

void ResourceCreator::DeleteNode(avs::uid server_uid, avs::uid id)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->mNodeManager.RemoveNode(id);
}

void ResourceCreator::CreateNode( avs::uid server_uid, avs::uid id, const avs::Node &avsNode)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	geometryCache->ReceivedResource(id);
	switch (avsNode.data_type)
	{
	case avs::NodeDataType::Light:
		CreateLight(server_uid,id, avsNode);
		return;
	case avs::NodeDataType::Link:
		CreateLinkNode(  server_uid, id, avsNode);
		return;
	default:
		break;
	}

	std::shared_ptr<Node> node;

	std::lock_guard g(geometryCache->missingResourcesMutex);
// If the node exists already we have a problem.
// If we recreate the node here, we must either reset the missing resource count or add to it.
	if (geometryCache->mNodeManager.HasNode(id))
	{
		TELEPORT_WARN("CreateNode {} {} Already created." , id , avsNode.name);
		// leaves nodes with no children. why?
		auto n=geometryCache->mNodeManager.GetNode(id);
		node = geometryCache->mNodeManager.GetNode(id);
		MissingResource *missingResource = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Node);
		if(missingResource)
		{
			TELEPORT_WARN("But it's listed as missing.");
		}
		return;
	}
	else
	{
		std::chrono::microseconds session_time_us=geometryCache->GetSessionTimeUs();
		node = geometryCache->mNodeManager.CreateNode( session_time_us, id, avsNode);
	}
	node->ResetMissingResourceCount();
	// Was this node being awaited?
	MissingResource* missingResource = geometryCache->GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Node);
	if(missingResource)
	{
		for(auto waiting = missingResource->waitingResources.begin(); waiting != missingResource->waitingResources.end(); waiting++)
		{
			// Is this the bone of a skeleton?
			if (waiting->get()->type == avs::GeometryPayloadType::Skeleton)
			{
				std::shared_ptr<IncompleteSkeleton> waitingSkeleton = std::static_pointer_cast<IncompleteSkeleton>(*waiting);
				if (waitingSkeleton)
				{
					RESOURCE_RECEIVES(*waiting, id);
					// If the waiting resource has no incomplete resources, it is now itself complete.
					if ((*waiting)->GetMissingResourceCount() == 0)
					{
						geometryCache->CompleteSkeleton((*waiting)->id, waitingSkeleton);
					}
				}
			}
		}
	}
	// Whether the node is missing any resource before, and must wait for them before it can be completed.
	bool isMissingResources = false;
	if(avsNode.skeletonID!=0)
	{

		auto skeleton=geometryCache->mSkeletonManager.Get(avsNode.skeletonID);
		if(!skeleton)
		{
			TELEPORT_COUT<<"MeshNode_" << id << "(" << avsNode.name << ") missing Skeleton " << avsNode.skeletonID << std::endl;
			isMissingResources = true;
			auto &missing=geometryCache->GetMissingResource(avsNode.skeletonID, avs::GeometryPayloadType::Skeleton);
			missing.waitingResources.insert(node);
			node->IncrementMissingResources();
		}
		else
		{
			node->SetSkeleton(skeleton);
			node->GetOrCreateComponent<AnimationComponent>();
		}
	}
		
	if(avsNode.skeletonNodeID!=0)
	{
		auto skeletonNode=geometryCache->mNodeManager.GetNode(avsNode.skeletonNodeID);
		if(!skeletonNode)
		{
			TELEPORT_COUT<<"MeshNode_" << id << "(" << avsNode.name << ") missing Skeleton Node " << avsNode.skeletonNodeID << std::endl;
			isMissingResources = true;
			auto &missing=geometryCache->GetMissingResource(avsNode.skeletonNodeID, avs::GeometryPayloadType::Node);
			missing.waitingResources.insert(node);
			node->IncrementMissingResources();
		}
		else
		{
			std::weak_ptr<clientrender::Node> skn=skeletonNode;
			node->SetSkeletonNode(skn);
		}
	}
	if (avsNode.data_uid != 0)
	{
		std::shared_ptr<clientrender::SubSceneComponent> subSceneComponent;
		if(avsNode.data_type==avs::NodeDataType::Mesh)
		{
			subSceneComponent=node->GetOrCreateComponent<SubSceneComponent>();
		
			subSceneComponent->mesh_uid=avsNode.data_uid;
			auto mesh=geometryCache->mMeshManager.Get(subSceneComponent->mesh_uid);
			node->SetMesh(mesh);
			if (!mesh)
			{
				isMissingResources = true;
				node->IncrementMissingResources();
				TELEPORT_INTERNAL_COUT("Node {0} ({1}) missing Mesh {2}, total missing resources: {3}",id,avsNode.name,avsNode.data_uid,node->GetMissingResourceCount());
				geometryCache->GetMissingResource(avsNode.data_uid, avs::GeometryPayloadType::Mesh).waitingResources.insert(node);
			}
		}
		if(avsNode.data_type==avs::NodeDataType::TextCanvas)
		{
			node->SetTextCanvas(geometryCache->mTextCanvasManager.Get(avsNode.data_uid));
			if (!node->GetTextCanvas())
			{
				TELEPORT_CERR<< "MeshNode " << id << "(" << avsNode.name << ") missing Mesh " << avsNode.data_uid << std::endl;

				isMissingResources = true;
				node->IncrementMissingResources();
				geometryCache->GetMissingResource(avsNode.data_uid, avs::GeometryPayloadType::TextCanvas).waitingResources.insert(node);
			}
			else
				geometryCache->mNodeManager.NotifyModifiedMaterials(node);
		}
		if (avsNode.skeletonID)
		{
			auto skeleton=geometryCache->mSkeletonManager.Get(avsNode.skeletonID);
			if(skeleton)
			{
				node->SetSkeleton(skeleton);
				node->GetOrCreateComponent<AnimationComponent>();
				node->SetInverseBindMatrices(avsNode.inverseBindMatrices);
				if(node->GetJointIndices().size()!=node->GetInverseBindMatrices().size())
				{
					TELEPORT_WARN("Inverse bind matrices don't match joint list.");
				}
			}
			else
			{
				node->IncrementMissingResources();
				isMissingResources = true;
				TELEPORT_CERR << "MeshNode " << id << "(" << avsNode.name << ") missing skeleton." << std::endl;
				geometryCache->GetMissingResource(avsNode.data_uid, avs::GeometryPayloadType::Skeleton).waitingResources.insert(node);
			}
		}
	}


	for (size_t i = 0; i < avsNode.animations.size(); i++)
	{
		avs::uid animationID = avsNode.animations[i];
		std::shared_ptr<clientrender::Animation> animation = geometryCache->mAnimationManager.Get(animationID);

		if (animation)
		{
			auto animC=node->GetOrCreateComponent<AnimationComponent>();
			//animC->addAnimation(animationID,animation);
		}
		else
		{
			RESOURCECREATOR_DEBUG_COUT("MeshNode {0}({1} missing Animation {2})", id, avsNode.name, animationID);
			node->IncrementMissingResources();
			isMissingResources = true;
			geometryCache->GetMissingResource(animationID, avs::GeometryPayloadType::Animation).waitingResources.insert(node);
		}
	}

	if (placeholderMaterial == nullptr)
	{
		clientrender::Material::MaterialCreateInfo materialCreateInfo;
		materialCreateInfo.name="Placeholder";
		materialCreateInfo.diffuse.texture = m_DummyWhite;
		materialCreateInfo.combined.texture = m_DummyCombined;
		materialCreateInfo.normal.texture = m_DummyNormal;
		materialCreateInfo.emissive.texture = m_DummyGreen;
		placeholderMaterial = std::make_shared<clientrender::Material>(materialCreateInfo);
	}
	if(avsNode.renderState.globalIlluminationUid>0)
	{
		std::shared_ptr<clientrender::Texture> gi_texture = geometryCache->mTextureManager.Get(avsNode.renderState.globalIlluminationUid);
		if(!gi_texture)
		{
			isMissingResources = true;
			node->IncrementMissingResources();
			TELEPORT_CERR << "MeshNode_" << id << "(" << avsNode.name << ") missing giTexture." << std::endl;
			geometryCache->GetMissingResource(avsNode.renderState.globalIlluminationUid, avs::GeometryPayloadType::Texture).waitingResources.insert(node);
		}
	}
	std::set<avs::uid> missing_material_uids;
	for (size_t i = 0; i < avsNode.materials.size(); i++)
	{
		auto mat_uid=avsNode.materials[i];
		std::shared_ptr<clientrender::Material> material = geometryCache->mMaterialManager.Get(mat_uid);

		if (material)
		{
			node->SetMaterial(i, material);
			geometryCache->mNodeManager.NotifyModifiedMaterials(node);
		}
		else 
		{
			// If we don't know have the information on the material yet, we use placeholder OVR surfaces.
			node->SetMaterial(i, placeholderMaterial);
			// but if the material is uid 0, this means *no* material, so we will either use the subMesh's internal material,
			// or not draw the subMesh.
			if(mat_uid!=0)
			{
				missing_material_uids.insert(mat_uid);
				node->materialSlots[mat_uid].push_back(i);
				isMissingResources = true;
			}
		}
	}
	for (avs::uid mat_uid : missing_material_uids)
	{
		geometryCache->GetMissingResource(mat_uid, avs::GeometryPayloadType::Material).waitingResources.insert(node);
		node->IncrementMissingResources();
		//	TELEPORT_CERR << "MeshNode_" << id << "(" << avsNode.name << ") missing Material " << avsNode.materials[i] << std::endl;
		TELEPORT_INTERNAL_COUT("Node {0} ({1}) missing Material {2}, total missing resources: {3}", id, avsNode.name, mat_uid, node->GetMissingResourceCount());
	}

	size_t num_remaining = RESOURCES_AWAITED(node);
	TELEPORT_ASSERT(RESOURCE_IS_COMPLETE(node) == (!isMissingResources));
	//Complete node now, if we aren't missing any resources.
	if (!isMissingResources)
	{
		RESOURCECREATOR_DEBUG_COUT("Complete on creation, node: {0} use_count-2= {1}\n",id,num_remaining);
		geometryCache->CompleteNode(id, node);
	}
	else
	{
		RESOURCECREATOR_DEBUG_COUT ( "Incomplete on creation, node: {0}  missing {1} or {2}\n",id,num_remaining,node->GetMissingResourceCount());
		node->InitDigitizing();
	}
}

void ResourceCreator::CreateLight(avs::uid server_uid,avs::uid id,const avs::Node& node)
{
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
	RESOURCECREATOR_DEBUG_COUT( "CreateLight {0}({1})" , id , node.name);
	geometryCache->ReceivedResource(id);
	if(geometryCache->mLightManager.Get(id))
		return;
	clientrender::Light::LightCreateInfo lci;
	lci.renderPlatform = renderPlatform;
	lci.type = (clientrender::Light::Type)node.lightType;
	lci.position = vec3(node.localTransform.position);
	lci.direction = node.lightDirection;
	lci.orientation = clientrender::quat(node.localTransform.rotation);
	lci.shadowMapTexture = geometryCache->mTextureManager.Get(node.data_uid);
	lci.lightColour = node.lightColour;
	lci.lightRadius = node.lightRadius;
	lci.lightRange = node.lightRange;
	lci.uid = id;
	lci.name = node.name;
	std::shared_ptr<clientrender::Light> light = std::make_shared<clientrender::Light>(&lci);
	geometryCache->mLightManager.Add(id, light);
}

#include <vkformat_enum.h>
static clientrender::Texture::Format VkFormatToTeleportFormat(VkFormat f)
{
	switch(f)
	{
		case VK_FORMAT_R8_UNORM: return clientrender::Texture::Format::R8;
		case VK_FORMAT_R8G8B8A8_UNORM: return clientrender::Texture::Format::RGBA8; 
		case VK_FORMAT_R8G8B8A8_SNORM: return clientrender::Texture::Format::RGBA8; 
	    // Compression formats ------------ GPU Mapping DirectX, Vulkan and OpenGL formats and comments --------
    // Compressed Format 0xSnn1..0xSnnF   (Keys 0x00Bv..0x00Bv) S =1 is signed, 0 = unsigned, B =Block Compressors 1..7 (BC1..BC7) and v > 1 is a variant like signed or swizzle
		case VK_FORMAT_BC2_UNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                   // compressed texture format with explicit alpha for Microsoft DirectX10. Identical to DXT3. Eight bits per pixel.
		case VK_FORMAT_BC3_UNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                   // compressed texture format with interpolated alpha for Microsoft DirectX10. Identical to DXT5. Eight bits per pixel.
		case VK_FORMAT_BC4_UNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
		case VK_FORMAT_BC4_SNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                  // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
		case VK_FORMAT_BC5_UNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
		case VK_FORMAT_BC5_SNORM_BLOCK: return clientrender::Texture::Format::RGBA8;                                   // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
		case VK_FORMAT_BC6H_UFLOAT_BLOCK: return clientrender::Texture::Format::RGBA16F;  //       CMP_FORMAT_BC6H_SF = 0x1061,  //  VK_FORMAT_BC6H_SFLOAT_BLOCK     CMP_FORMAT_BC7     = 0x0071,  //  VK_FORMAT_BC7_UNORM_BLOCK 
		case VK_FORMAT_R32G32B32A32_SFLOAT: return clientrender::Texture::Format::RGBA32F;
		default:
			return clientrender::Texture::Format::FORMAT_UNKNOWN;
	};
}
static teleport::clientrender::Texture::CompressionFormat VkFormatToCompressionFormat(VkFormat f)
{
	switch(f)
	{
	    // Compression formats ------------ GPU Mapping DirectX, Vulkan and OpenGL formats and comments --------
    // Compressed Format 0xSnn1..0xSnnF   (Keys 0x00Bv..0x00Bv) S =1 is signed, 0 = unsigned, B =Block Comprd v > 1 is a variant like signed or swizzle
    case VK_FORMAT_BC2_UNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC3;         // compressed texture format with explicit alpha for Microsoft DirectX10. Identical to DXT3. Eight bits per pixel.
    case VK_FORMAT_BC3_UNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC3;         // compressed texture format with interpolated alpha for Microsoft DirectX10. Identical to DXT5. Eight bits per pixel.
    case VK_FORMAT_BC4_UNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC4;         // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC4_SNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC4;         // compressed texture format for Microsoft DirectX10. Identical to ATI1N. Four bits per pixel.
    case VK_FORMAT_BC5_UNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC5;         // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC5_SNORM_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC5;         // compressed texture format for Microsoft DirectX10. Identical to ATI2N_XY. Eight bits per pixel.
    case VK_FORMAT_BC6H_UFLOAT_BLOCK: return teleport::clientrender::Texture::CompressionFormat::BC6H;		//       CMP_FORMAT_BC6H_SF = 0x1061,  //  VK_FORMAT_BC6H_SFLOAT_BLOCK     CMP_FORMAT_BC7     = 0x0071,  //  VK_FORMAT_BC7_UNORM_BLOCK 
	default:
		return teleport::clientrender::Texture::CompressionFormat::UNCOMPRESSED;
	};
}

/*
* 6406:			//Alpha			
* 6407:			// Rgb			
* 6408:			// Rgba			
* 6409:			// Luminance	
* 6410:			// LuminanceAlpha
*/

static clientrender::Texture::Format GlInternalFormatToTeleportFormat(GLuint p)
{
	using namespace clientrender;
	switch(p)
	{
		case GL_RGBA16F:
			return Texture::Format::RGBA16F;
		case GL_RGBA32F:
			return Texture::Format::RGBA32F;
		case GL_RGBA32UI:
			return Texture::Format::RGBA32UI;
		case GL_RGB32F:
			return Texture::Format::RGB32F;
		case GL_RG32F:
			return Texture::Format::RG32F;
		case GL_R32F:
			return Texture::Format::R32F;
		case GL_R16F:
			return Texture::Format::R16F;
		case GL_LUMINANCE32F_ARB:
			return Texture::Format::R32F;
		case GL_INTENSITY32F_ARB:
			return Texture::Format::R32F;
		case GL_RGBA8:
			return Texture::Format::RGBA8;
		case GL_SRGB8_ALPHA8:
			return Texture::Format::RGBA8;
		case GL_RGBA8_SNORM:
			return Texture::Format::RGBA8_SNORM;
		case GL_RGB8:
			return Texture::Format::RGB8;
		case GL_RGB8_SNORM:
			return Texture::Format::RGB8;
		case GL_R8:
			return Texture::Format::R8;
		case GL_R32UI:
			return Texture::Format::R32UI;
		case GL_RG32UI:
			return Texture::Format::RG32UI;
	default:
		return Texture::Format::FORMAT_UNKNOWN;
	};
}
static teleport::clientrender::Texture::CompressionFormat GlFormatToCompressionFormat(uint32_t f)
{
	switch(f)
	{
	case 0:
	default:
		return teleport::clientrender::Texture::CompressionFormat::UNCOMPRESSED;
	};
}

struct KtxCallbackData
{
	int numFaces=0;
	int numMips=0;
	int numLayers=0;
	std::shared_ptr<std::vector<std::vector<uint8_t>>> images;
};
KTX_error_code ktxImageExtractionCallback(int miplevel, int face,
                      int width, int height, int depth,
                      ktx_uint64_t faceLodSize,
                      void* pixels, void* userdata)
{
	KtxCallbackData* ud = (KtxCallbackData*)userdata;
	int idx=miplevel+ud->numMips*face;
	auto &img =(* ud->images.get())[idx];
	img.resize(faceLodSize);
    memcpy(img.data(), pixels, faceLodSize);
    return KTX_SUCCESS;
}

void ResourceCreator::thread_TranscodeTextures()
{
	SetThisThreadName("thread_TranscodeTextures");
	auto &config=teleport::client::Config::GetInstance();
	while (shouldBeTranscoding)
	{
		if(!config.debugOptions.enableTextureTranscodingThread)
		{
			std::this_thread::sleep_for(1700ms);
			continue;
		}
		//std::this_thread::yield(); //Yield at the start, as we don't want to yield before we unlock (when lock goes out of scope).
		
		//Copy the data out of the shared data structure and minimise thread stalls due to mutexes
		std::shared_ptr<UntranscodedTexture> transcoding;
		{
			std::lock_guard<std::mutex> lock_texturesToTranscode(mutex_texturesToTranscode);
			if(!texturesToTranscode.size())
			{
				std::this_thread::yield();
				continue;
			}
			transcoding = texturesToTranscode[0];
			texturesToTranscode.erase(texturesToTranscode.begin());
		}

		{
			auto geometryCache=GeometryCache::GetGeometryCache(transcoding->cache_or_server_uid);
			if (transcoding->compressionFormat == avs::TextureCompression::MULTIPLE_PNG)
			{
				RESOURCECREATOR_DEBUG_COUT("Transcoding {0} with PNG",transcoding->name.c_str());
				int mipWidth=0, mipHeight=0;
				const uint8_t *srcPtr = transcoding->data.data();
				const uint8_t *basePtr=srcPtr;
				read_from_buffer(transcoding->textureCI->width,srcPtr);
				read_from_buffer(transcoding->textureCI->height,srcPtr);
				read_from_buffer(transcoding->textureCI->depth,srcPtr);

				// additional information.
				read_from_buffer(transcoding->textureCI->arrayCount,srcPtr);
				read_from_buffer(transcoding->textureCI->mipCount,srcPtr);
				bool cubemap=false;
				read_from_buffer(cubemap,srcPtr);
				transcoding->textureCI->type			=transcoding->textureCI->depth>1?clientrender::Texture::Type::TEXTURE_3D:clientrender::Texture::Type::TEXTURE_2D;
				if(cubemap)
				{
					transcoding->textureCI->type=clientrender::Texture::Type::TEXTURE_CUBE_MAP;
					if(transcoding->textureCI->arrayCount>6)
						transcoding->textureCI->type=clientrender::Texture::Type::TEXTURE_CUBE_MAP_ARRAY;
				}
				else
				{
					if(transcoding->textureCI->arrayCount>1)
						transcoding->textureCI->type=(clientrender::Texture::Type)(((int)transcoding->textureCI->type)|16);
				}

				//Push format.
				read_from_buffer(transcoding->textureCI->format,srcPtr);

				//Value scale - brightness number to scale the final texel by.
				read_from_buffer(transcoding->textureCI->valueScale,srcPtr);
				// let's have a uint16 here, N with the number of images, then a list of N uint32 offsets. Each is a subresource image. Then image 0 starts.
				uint16_t num_images=*((uint16_t*)srcPtr);
				std::vector<uint32_t> imageOffsets(num_images);
				srcPtr+=sizeof(uint16_t);
				size_t dataSize = transcoding->data.size() - sizeof(uint16_t);
				for(int i=0;i<num_images;i++)
				{
					imageOffsets[i]=*((uint32_t*)srcPtr);
					srcPtr+=sizeof(uint32_t);
					dataSize-=sizeof(uint32_t);
				}
				imageOffsets.push_back((uint32_t)transcoding->data.size());
				std::vector<uint32_t> imageSizes(num_images);
				bool bad=false;
				for(int i=0;i<num_images;i++)
				{
					imageSizes[i]=imageOffsets[i+1]-imageOffsets[i];
					if(imageOffsets[i+1]>transcoding->data.size())
					{
						TELEPORT_WARN("Bad texture data in {0}",transcoding->textureCI->name);
						bad=true;
						break;
					}
				}
				if(bad)
					continue;
				transcoding->textureCI->images = std::make_shared<std::vector<std::vector<uint8_t>>>();
				transcoding->textureCI->images->resize(num_images);
				for(int i=0;i<num_images;i++)
				{
					// Convert from Png to raw data:
				#if TELEPORT_INTERNAL_CHECKS
					std::ofstream ofs(fmt::format("temp/{0}_{1}.png",transcoding->name,i).c_str(),std::ios::binary);
					ofs.write(((char*)basePtr+imageOffsets[i]),(size_t) imageSizes[i]);
				#endif
					int num_channels=0;
					// request 4 channels because e.g. d3d can't handle 3-channel data for upload.
					unsigned char *target = teleport::stbi_load_from_memory(basePtr+imageOffsets[i],(int) imageSizes[i], &mipWidth, &mipHeight, &num_channels,(int)4);
					// note. num_channels now holds the number of channels in the original data. The data received has four channels.
					num_channels=4;
					
					if( mipWidth > 0 && mipHeight > 0&&target&&transcoding->data.size()>2)
					{
						// this is for 8-bits-per-channel textures:
						size_t outDataSize = (size_t)(mipWidth * mipHeight * num_channels);
						(*transcoding->textureCI->images)[i].resize(outDataSize);
						memcpy((*transcoding->textureCI->images)[i].data(), target, outDataSize);
						transcoding->textureCI->valueScale=transcoding->valueScale;

					}
					else
					{
						TELEPORT_CERR << "Failed to transcode PNG-format texture \"" << transcoding->name << "\"." << std::endl;
					}
					teleport::stbi_image_free(target);
					// fill in values from file.
					if(!i)
					{
						transcoding->textureCI->width=mipWidth;
						transcoding->textureCI->height=mipHeight;
						transcoding->textureCI->depth=1;
						transcoding->textureCI->format=((num_channels==4)?clientrender::Texture::Format::RGBA8:clientrender::Texture::Format::RGB8);
					}
				}
				if (transcoding->textureCI->images->size() != 0)
				{
					//TELEPORT_CERR << "Texture \"" << transcoding->name << "\" uid "<< transcoding->texture_uid<<", Type "<<int(transcoding->textureCI->type) << std::endl;
					geometryCache->CompleteTexture(transcoding->texture_uid, *(transcoding->textureCI));
				}
				else
				{
					TELEPORT_CERR << "Texture \"" << transcoding->name << "\" failed to transcode, no images found." << std::endl;
				}
			}
			else if (transcoding->compressionFormat == avs::TextureCompression::JPEG||transcoding->compressionFormat == avs::TextureCompression::PNG
						||transcoding->compressionFormat == avs::TextureCompression::UNCOMPRESSED)
			{
				RESOURCECREATOR_DEBUG_COUT("Transcoding {0} with JPEG/PNG",transcoding->name.c_str());

				transcoding->textureCI->images = std::make_shared<std::vector<std::vector<uint8_t>>>();
				transcoding->textureCI->images->resize(1);
				for(int i=0;i<1;i++)
				{
					int num_channels=0;
					// request 4 channels because e.g. d3d can't handle 3-channel data for upload.
					int mipWidth=0, mipHeight=0;
					unsigned char *stb = teleport::stbi_load_from_memory(transcoding->data.data(),transcoding->data.size(), &mipWidth, &mipHeight, &num_channels,(int)4);
					unsigned char *target=stb;
					if(!stb)
					{
						// as a fallback, maybe it's just plain uncompressed data.
						size_t fullSize = transcoding->textureCI->height*transcoding->textureCI->width*4;
						if(transcoding->data.size()==fullSize)
						{
							target=transcoding->data.data();
							mipWidth=transcoding->textureCI->width;
							mipHeight=transcoding->textureCI->height;
						}
					}
					if( mipWidth > 0 && mipHeight > 0&&target&&transcoding->data.size()>2)
					{
					num_channels=4;
						size_t numTexels = mipWidth * mipHeight;
						// this is for 8-bits-per-channel textures:
						if(num_channels==4||num_channels==1)
						{
							size_t outDataSize = (size_t)(mipWidth * mipHeight * num_channels);
							(*transcoding->textureCI->images)[i].resize(outDataSize);
							memcpy((*transcoding->textureCI->images)[i].data(), target, outDataSize);
						}
						else if(num_channels==3)
						{
							size_t outDataSize = (size_t)(numTexels * 4);
							auto &img=(*transcoding->textureCI->images)[i];
							img.resize(outDataSize);
							for(size_t j=0;j<numTexels;j++)
							{
								img[j*4  ]=target[j*3];
								img[j*4+1]=target[j*3+1];
								img[j*4+2]=target[j*3+2];
								img[j*4+3]=255;
							}
						}
						else
						{
							TELEPORT_WARN("Can't translate texture with {} channels.\n",num_channels);
							transcoding->textureCI->images->resize(0);
							continue;
						}
						transcoding->textureCI->valueScale=transcoding->valueScale;

					}
					else
					{
						TELEPORT_CERR << "Failed to transcode JPEG or PNG format texture \"" << transcoding->name << "\"." << std::endl;
					}
					teleport::stbi_image_free(stb);
					// fill in values from file.
					if(!i)
					{
						transcoding->textureCI->width=mipWidth;
						transcoding->textureCI->height=mipHeight;
						transcoding->textureCI->depth=1;
						transcoding->textureCI->arrayCount=1;
						transcoding->textureCI->mipCount=1;
						transcoding->textureCI->type=Texture::Type::TEXTURE_2D;
						transcoding->textureCI->format=clientrender::Texture::Format::RGBA8;
					}
				}
				if (transcoding->textureCI->images->size() != 0)
				{
					//TELEPORT_CERR << "Texture \"" << transcoding->name << "\" uid "<< transcoding->texture_uid<<", Type "<<int(transcoding->textureCI->type) << std::endl;
					geometryCache->CompleteTexture(transcoding->texture_uid, *(transcoding->textureCI));
				}
				else
				{
					TELEPORT_CERR << "Texture \"" << transcoding->name << "\" failed to transcode, no images found." << std::endl;
				}
			}
			else if(transcoding->compressionFormat==avs::TextureCompression::KTX)
			{
				ktxTextureCreateFlags createFlags=KTX_TEXTURE_CREATE_NO_FLAGS;
				ktxTexture *ktxt=nullptr;
				KTX_error_code 	result=ktxTexture_CreateFromMemory(transcoding->data.data(), transcoding->data.size(),  createFlags, &ktxt);
				if (result != KTX_SUCCESS)
				{
					const char *errstr=ktxErrorString(result);
					TELEPORT_WARN("Texture {0} failed to create ktx. KTX Error code {1}.\n",transcoding->name,errstr?errstr:"");
					continue;
				}
				if(ktxt->classId==ktxTexture1_c)
				{
					ktxTexture1 *ktx1Texture = (ktxTexture1* )ktxt;
					transcoding->textureCI->width	=ktx1Texture->baseWidth;
					transcoding->textureCI->height	=ktx1Texture->baseHeight;
					transcoding->textureCI->depth	=ktx1Texture->baseDepth;
					transcoding->textureCI->arrayCount=ktx1Texture->numLayers?ktx1Texture->numLayers:1;
					transcoding->textureCI->mipCount	=ktx1Texture->numLevels; 
					ktx_uint32_t glInternalFormat=(ktx_uint32_t)(ktx1Texture->glInternalformat);
					transcoding->textureCI->format	=GlInternalFormatToTeleportFormat(glInternalFormat);
					if(transcoding->textureCI->format==clientrender::Texture::Format::FORMAT_UNKNOWN)
					{
						TELEPORT_WARN("Texture {0} failed to obtain upload data from ktx.\n",transcoding->name);
						continue;
					}
					{
						transcoding->textureCI->valueScale=1.0f;
						transcoding->textureCI->compression=GlFormatToCompressionFormat(glInternalFormat);
				
						bool cubemap=(ktx1Texture->isCubemap);
						transcoding->textureCI->type			=cubemap?clientrender::Texture::Type::TEXTURE_CUBE_MAP:clientrender::Texture::Type::TEXTURE_2D; //Assumed
						uint32_t numFaces=cubemap?6:1;
						uint16_t numImages = transcoding->textureCI->arrayCount * transcoding->textureCI->mipCount*numFaces;
						transcoding->textureCI->images = std::make_shared<std::vector<std::vector<uint8_t>>>();
						transcoding->textureCI->images->resize(ktx1Texture->numFaces*ktx1Texture->numLayers*ktx1Texture->numLevels);
						
					}
				}
				else if(ktxt->classId==ktxTexture2_c)
				{
					ktxTexture2 *ktx2Texture = (ktxTexture2* )ktxt;
					transcoding->textureCI->width		=ktx2Texture->baseWidth;
					transcoding->textureCI->height		=ktx2Texture->baseHeight;
					transcoding->textureCI->depth		=ktx2Texture->baseDepth;
					transcoding->textureCI->arrayCount	=ktx2Texture->numLayers?ktx2Texture->numLayers:1;
					transcoding->textureCI->mipCount	=ktx2Texture->numLevels;
					VkFormat vkfmt=(VkFormat)(ktx2Texture->vkFormat);
					transcoding->textureCI->format	=VkFormatToTeleportFormat(vkfmt);
					if(transcoding->textureCI->format==clientrender::Texture::Format::FORMAT_UNKNOWN)
					{
						ktx_transcode_fmt_e outputFormat=KTX_TTF_RGBA32;
						ktx_transcode_flags transcodeFlags=KTX_TF_HIGH_QUALITY;
						result= ktxTexture2_TranscodeBasis 	( 	ktx2Texture,outputFormat,transcodeFlags ) 	;
						if (result != KTX_SUCCESS)
						{
							TELEPORT_WARN("Texture {0} failed to obtain upload data from ktx.\n",transcoding->name);
							continue;
						}
						vkfmt=(VkFormat)(ktx2Texture->vkFormat);
						transcoding->textureCI->format	=VkFormatToTeleportFormat(vkfmt);
					}
					{
						transcoding->textureCI->valueScale=1.0f;
						transcoding->textureCI->compression=VkFormatToCompressionFormat((VkFormat)(ktx2Texture->vkFormat));
				
						bool cubemap=(ktx2Texture->isCubemap);
						transcoding->textureCI->type			=cubemap?clientrender::Texture::Type::TEXTURE_CUBE_MAP:clientrender::Texture::Type::TEXTURE_2D; //Assumed
						uint32_t numFaces=cubemap?6:1;
						uint16_t numImages = transcoding->textureCI->arrayCount * transcoding->textureCI->mipCount*numFaces;
						transcoding->textureCI->images = std::make_shared<std::vector<std::vector<uint8_t>>>();
						transcoding->textureCI->images->resize(ktx2Texture->numFaces*ktx2Texture->numLayers*ktx2Texture->numLevels);
						
					}
				}
				else
				{
					TELEPORT_WARN("Invalid ktx class {0}",(uint64_t)ktxt->classId);
				}
				if (result != KTX_SUCCESS)
				{
					TELEPORT_WARN("Texture {0} failed to obtain upload data from ktx.\n",transcoding->name);
				}
				else
				{
					// For ktx textures, they will be already encoded, so we upload the whole thing as a single chunk of memory.
					size_t textureSize = ktxTexture_GetDataSizeUncompressed(ktxt);
					KtxCallbackData cbData;
					cbData.images=transcoding->textureCI->images;
					cbData.numMips=transcoding->textureCI->mipCount;
					cbData.numFaces=ktxt->numFaces;
					cbData.numLayers=ktxt->numLayers;
					if (ktxt->pData) {
						result = ktxTexture_IterateLevelFaces(
													ktxt,
													ktxImageExtractionCallback,
													&cbData);
					} else {
						result = ktxTexture_IterateLoadLevelFaces(
													ktxt,
													ktxImageExtractionCallback,
													&cbData);
					}
					if (result == KTX_SUCCESS)
						geometryCache->CompleteTexture(transcoding->texture_uid, *(transcoding->textureCI));
					else
					{
						if(ktxt->classId==ktxTexture2_c)
						{
							ktxTexture2 *ktx2Texture = (ktxTexture2* )ktxt;
							result = ktxTexture2_IterateLoadLevelFaces(ktx2Texture,
													   ktxImageExtractionCallback,
													   &cbData);
						}
					}
				}
				if(ktxt)
					ktxTexture_Destroy(ktxt);
			}
		}
	}
}
