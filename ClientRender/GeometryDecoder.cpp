#pragma optimize("", off)
#include "GeometryDecoder.h"

#include "Common.h"
#include "Platform/Core/FileLoader.h"
#include "ResourceCreator.h"
#include "TeleportCore/FontAtlas.h"
#include "TeleportClient/Config.h"
#include "TeleportCore/Animation.h"
#include "TeleportCore/ErrorHandling.h"
#include "ThisPlatform/Threads.h"
#include <fstream>
#include <iostream>

#ifdef _MSC_VER
#pragma warning(disable : 4018; disable : 4804)
#endif
#include "draco/compression/decode.h"
#include "draco/io/gltf_decoder.h"
#include "MeshDecoder.h"
#include <libavstream/httputil.hpp>
avs::HTTPUtil hTTPUtil;
#include <filesystem>
using std::filesystem::path;
using namespace std::chrono_literals;
using namespace teleport;
using namespace clientrender;
#pragma optimize("", off)

#define NextUint64 get<uint64_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextUint32 get<uint32_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextInt32 get<int32_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextUint16 get<uint16_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextInt16 get<int16_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextByte get<uint8_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextFloat get<float>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextVec4 get<vec4>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextVec3 get<vec3>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define NextChunk(T) get<T>(geometryDecodeData.data.data(), &geometryDecodeData.offset)
#define CopyChunk(target, size) copy_chunk(target, geometryDecodeData.data.data(), &geometryDecodeData.offset, size)
#define NextString64                                                                                                                                           \
	{                                                                                                                                                          \
		get<size_t>(geometryDecodeData.data.data(), &geometryDecodeData.offset)

#define FAIL_IF_INSUFFICIENT_BYTES_REMAINING(bytes)                                                                                                            \
	{                                                                                                                                                          \
		if (geometryDecodeData.offset + (size_t)bytes > geometryDecodeData.data.size()) return avs::Result::Failed;                                            \
	}

template <typename T> void copy(T *target, const uint8_t *data, size_t &dataOffset, size_t count)
{
	memcpy(target, data + dataOffset, count * sizeof(T));
	dataOffset += count * sizeof(T);
}

using std::string;
using namespace std::string_literals;

template <typename T> T get(const uint8_t *data, size_t *offset)
{
	T *t = (T *)(data + (*offset));
	*offset += sizeof(T);
	return *t;
}

template <typename T, typename U> bool getList(std::vector<U> &list, std::vector<uint8_t> &data, size_t &offset)
{
	if (offset >= data.size()) return false;
	T size = get<T>(data.data(), &offset);
	if (size == 0) return true;
	size_t byteSize = size * sizeof(U);
	if (offset + byteSize > data.size()) return false;
	if (offset + byteSize < offset) return false;
	list.resize(size);
	copy<U>(list.data(), data.data(), offset, size);
	return true;
}
#define NextList(T, U, list)                                                                                                                                   \
	{                                                                                                                                                          \
		if (!getList<T, U>(list, geometryDecodeData.data, geometryDecodeData.offset))                                                                          \
		{                                                                                                                                                      \
			return avs::Result::Failed;                                                                                                                        \
		}                                                                                                                                                      \
	}

static void copy_chunk(uint8_t *target, const uint8_t *data, size_t *offset, size_t num_bytes)
{
	const void *src = ((const void *)(data + (*offset)));
	void *dst = (void *)target;
	memcpy(dst, src, num_bytes);
	(*offset) += num_bytes;
}

static avs::uid GenerateLocalUid()
{
	static avs::uid u = 0;
	auto r = u;
	u++;
	if (!r) r = 10000;
	return r;
};

void CreateSubScene(core::DecodedGeometry &subSceneDG, clientrender::ResourceCreator *target,std::string filename_url, avs::uid parent_cache_uid, avs::uid asset_uid,platform::crossplatform::AxesStandard sourceAxesStandard)
{
	// We will do two things here.
	// 1. We will create a new Geometry Cache containing the whole scene from the draco file.
	// 2. We will create a new asset in the containing cache that refers to that cache.
	subSceneDG.axesStandard = sourceAxesStandard;
	// The subscene uid in the server/cache list:
	subSceneDG.server_or_cache_uid = avs::GenerateUid();
	// The SubScene's own uid is the asset id.
	clientrender::SubSceneCreate subSceneCreate;
	subSceneCreate.uid = asset_uid;
	subSceneCreate.subscene_cache_uid = subSceneDG.server_or_cache_uid;
	avs::Result result = target->CreateSubScene(parent_cache_uid, subSceneCreate);
	// this is a new cache, so create it:
	clientrender::GeometryCache::CreateGeometryCache(subSceneDG.server_or_cache_uid, parent_cache_uid, filename_url);
}

GeometryDecoder::GeometryDecoder()
{
	decodeThread = std::thread(&GeometryDecoder::decodeAsync, this);
	decodeThreadActive = true;
	avs::HTTPUtilConfig httpUtilConfig;
	httpUtilConfig.remoteHTTPPort = 443;
	httpUtilConfig.maxConnections = 12;
	httpUtilConfig.useSSL = true;
	auto &config = teleport::client::Config::GetInstance();
	httpUtilConfig.cacheDirectory = (path(config.GetStorageFolder()) / "http_cache"s).string().c_str();
	hTTPUtil.initialize(httpUtilConfig);
}

GeometryDecoder::~GeometryDecoder()
{
	decodeThreadActive = false;
	decodeThread.join();
}

void GeometryDecoder::setCacheFolder(const std::string &f) { cacheFolder = f; }

avs::Result GeometryDecoder::decode(avs::uid server_uid,
									const void *buffer,
									size_t bufferSizeInBytes,
									avs::GeometryPayloadType type,
									avs::GeometryTargetBackendInterface *target,
									avs::uid resource_uid)
{
	GeometryFileFormat geometryFileFormat = GeometryFileFormat::TELEPORT_NATIVE;
	decodeData.emplace(server_uid,
					   "",
					   buffer,
					   bufferSizeInBytes,
					   type,
					   geometryFileFormat,
					   (clientrender::ResourceCreator *)target,
					   true,
					   resource_uid,
					   platform::crossplatform::AxesStandard::Engineering);

	switch (type)
	{
	case avs::GeometryPayloadType::Mesh:
	case avs::GeometryPayloadType::Material:
	case avs::GeometryPayloadType::MaterialInstance:
	case avs::GeometryPayloadType::Texture:
	case avs::GeometryPayloadType::Animation:
	case avs::GeometryPayloadType::Node:
	case avs::GeometryPayloadType::Skeleton:
	case avs::GeometryPayloadType::FontAtlas:
	case avs::GeometryPayloadType::TextCanvas:
	case avs::GeometryPayloadType::TexturePointer:
	case avs::GeometryPayloadType::MeshPointer:
	case avs::GeometryPayloadType::RemoveNodes:
		break;
	default:
		TELEPORT_BREAK_ONCE("Invalid Geometry payload");
	};

	return avs::Result::OK;
}
#include <filesystem>
avs::uid GeometryDecoder::decodeFromFile(avs::uid server_uid,
											const std::string &filename,
											avs::GeometryPayloadType type,
											clientrender::ResourceCreator *target,
											avs::uid resource_uid,
											platform::crossplatform::AxesStandard sourceAxesStandard)
{
	platform::core::FileLoader *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader->FileExists(filename.c_str()))
	{
		TELEPORT_CERR << "Failed to load file: " << filename << std::endl;
		return 0;
	}
	void *ptr = nullptr;
	unsigned int sz = 0;
	fileLoader->AcquireFileContents(ptr, sz, filename.c_str(), false);
	if(resource_uid==0)
		resource_uid=avs::GenerateUid();
	auto res = decodeFromBuffer(server_uid, (const uint8_t *)ptr, (size_t)sz, filename, type, target, resource_uid, sourceAxesStandard);
	fileLoader->ReleaseFileContents(ptr);
	return resource_uid;
}

avs::Result GeometryDecoder::decodeFromWeb(avs::uid server_uid,
										   const std::string &uri,
										   avs::GeometryPayloadType type,
										   clientrender::ResourceCreator *target,
										   avs::uid resource_uid,
										   platform::crossplatform::AxesStandard sourceAxesStandard)
{
	avs::HTTPPayloadRequest req;
	req.url = uri;
	if (req.url.find("://") == std::string::npos)
	{
		std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(server_uid);
		std::string url_root = geometryCache->GetDefaultURLRoot();
		req.url = "https://" + url_root + req.url;
	}
	auto f = std::bind(
		&GeometryDecoder::receiveFromWeb, this, server_uid, uri, std::placeholders::_1, std::placeholders::_2, type, target, resource_uid, sourceAxesStandard);
	req.callbackFn = std::move(f);
	req.shouldCache = true;
	hTTPUtil.GetRequestQueue().push(req);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::receiveFromWeb(avs::uid server_uid,
											std::string uri,
											const uint8_t *buffer,
											size_t bufferSize,
											avs::GeometryPayloadType type,
											clientrender::ResourceCreator *target,
											avs::uid resource_uid,
											platform::crossplatform::AxesStandard sourceAxesStandard)
{
	if (bufferSize)
	{
		return decodeFromBuffer(server_uid, buffer, bufferSize, uri, type, target, resource_uid, sourceAxesStandard);
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeFromBuffer(avs::uid server_uid,
											  const uint8_t *buffer,
											  size_t bufferSize,
											  const std::string &filename,
											  avs::GeometryPayloadType type,
											  clientrender::ResourceCreator *target,
											  avs::uid resource_uid,
											  platform::crossplatform::AxesStandard sourceAxesStandard)
{
	std::string extens = std::filesystem::path(filename).extension().string();
	GeometryFileFormat geometryFileFormat = GeometryFileFormat::TELEPORT_NATIVE;
	if (extens == ".mesh_compressed" || extens == ".mesh")
	{
	}
	else if (extens == ".gltf")
	{
		geometryFileFormat = GeometryFileFormat::GLTF_TEXT;
	}
	else if (extens == ".glb" || extens == ".vrm")
	{
		geometryFileFormat = GeometryFileFormat::GLTF_BINARY;
	}
	else if (extens == ".vrma")
	{
		geometryFileFormat = GeometryFileFormat::GLTF_BINARY;
	}
	else if (extens.length() > 0)
	{
		geometryFileFormat = GeometryFileFormat::FROM_EXTENSION;
	}
	decodeData.emplace(server_uid, filename, buffer, bufferSize, type, geometryFileFormat, target, false, resource_uid, sourceAxesStandard);
	return avs::Result::OK;
}

void GeometryDecoder::decodeAsync()
{
	SetThisThreadName("GeometryDecoder::decodeAsync");
	auto &config = teleport::client::Config::GetInstance();
	while (decodeThreadActive)
	{
		if (!config.debugOptions.enableGeometryTranscodingThread)
		{
			std::this_thread::sleep_for(2000ms);
			continue;
		}
		if (!decodeData.empty())
		{
			decodeInternal(decodeData.front());
			decodeData.pop();
		}
		hTTPUtil.process();
		std::this_thread::yield();
	}
}

avs::Result GeometryDecoder::decodeInternal(GeometryDecodeData &geometryDecodeData)
{
	//std::cout << "GeometryDecoder::decodeInternal " << avs::stringOf(geometryDecodeData.type) << ", size " << geometryDecodeData.data.size() << "\n";
	switch (geometryDecodeData.type)
	{
	case avs::GeometryPayloadType::Mesh:
		return decodeMesh(geometryDecodeData);
	case avs::GeometryPayloadType::Material:
		return decodeMaterial(geometryDecodeData);
	case avs::GeometryPayloadType::MaterialInstance:
		return decodeMaterialInstance(geometryDecodeData);
	case avs::GeometryPayloadType::Texture:
		return decodeTexture(geometryDecodeData);
	case avs::GeometryPayloadType::Animation:
		return decodeAnimation(geometryDecodeData);
	case avs::GeometryPayloadType::Node:
		return decodeNode(geometryDecodeData);
	case avs::GeometryPayloadType::RemoveNodes:
		return decodeRemoveNodes(geometryDecodeData);
	case avs::GeometryPayloadType::Skeleton:
		return decodeSkeleton(geometryDecodeData);
	case avs::GeometryPayloadType::FontAtlas:
		return decodeFontAtlas(geometryDecodeData);
	case avs::GeometryPayloadType::TextCanvas:
		return decodeTextCanvas(geometryDecodeData);
	case avs::GeometryPayloadType::TexturePointer:
		return decodeTexturePointer(geometryDecodeData);
	case avs::GeometryPayloadType::MeshPointer:
		return decodeMeshPointer(geometryDecodeData);
	default:
		TELEPORT_BREAK_ONCE("Invalid Geometry payload");
		return avs::Result::GeometryDecoder_InvalidPayload;
	};
}

#pragma region DracoDecoding

avs::Result GeometryDecoder::DecodeGltf(const GeometryDecodeData &geometryDecodeData)
{
	draco::GltfDecoder gltfDecoder;
	draco::DecoderBuffer dracoDecoderBuffer;
	dracoDecoderBuffer.Init((const char *)(geometryDecodeData.data.data()), geometryDecodeData.data.size());
	std::unique_ptr<draco::Scene> scene;
	std::unique_ptr<draco::Mesh> mesh;
	if (geometryDecodeData.geometryFileFormat == GeometryFileFormat::GLTF_BINARY)
	{
		draco::StatusOr<std::unique_ptr<draco::Scene>> s = gltfDecoder.DecodeFromBufferToScene(&dracoDecoderBuffer);
		if(s.status().code() == draco::Status::UNSUPPORTED_FEATURE)
		{
			core::DecodedGeometry subSceneDG;
			CreateSubScene(subSceneDG,geometryDecodeData.target,geometryDecodeData.filename_or_url, geometryDecodeData.server_or_cache_uid,geometryDecodeData.uid,geometryDecodeData.sourceAxesStandard);
			// Todo: Sometimes stationary should be true.
			if(!DecodeScene(geometryDecodeData, subSceneDG, false))
				return avs::Result::Failed;
			return CreateFromDecodedGeometry(geometryDecodeData.target, subSceneDG, geometryDecodeData.filename_or_url);
			// try
		}
		else if (s.status().code() != draco::Status::OK)
		{
			TELEPORT_CERR << "Failed to decode " << geometryDecodeData.filename_or_url << ": " << s.status().error_msg_string() << "\n";
			return avs::Result::Failed;
		}
		scene = std::move(s).value();
		draco::StatusOr<std::unique_ptr<draco::Mesh>> m = gltfDecoder.DecodeFromBuffer(&dracoDecoderBuffer);
		if (m.status().code() != draco::Status::OK)
		{
			TELEPORT_CERR << m.status().error_msg_string() << "\n";
			return avs::Result::Failed;
		}
		mesh = std::move(m).value();
	}
	else if (geometryDecodeData.geometryFileFormat == GeometryFileFormat::GLTF_TEXT)
	{
		draco::StatusOr<std::unique_ptr<draco::Scene>> s = gltfDecoder.DecodeFromTextBufferToScene(&dracoDecoderBuffer);
		if (s.status().code() != draco::Status::OK)
		{
			TELEPORT_CERR << s.status().error_msg_string() << "\n";
			return avs::Result::Failed;
		}
		scene = std::move(s).value();
	}
	avs::Result res = avs::Result::Failed;
	if (scene.get())
	{
		core::DecodedGeometry subSceneDG;	
		CreateSubScene(subSceneDG,geometryDecodeData.target,geometryDecodeData.filename_or_url, geometryDecodeData.server_or_cache_uid,geometryDecodeData.uid,geometryDecodeData.sourceAxesStandard);
		return DecodeDracoScene(subSceneDG, geometryDecodeData.target,
								geometryDecodeData.filename_or_url,
								geometryDecodeData.server_or_cache_uid,
								geometryDecodeData.uid,
								*(scene.get()),
								geometryDecodeData.sourceAxesStandard);
								}
	else if (mesh.get())
	{
		core::DecodedGeometry dg;
		res = DracoMeshToDecodedGeometry(geometryDecodeData.uid, dg, *(mesh.get()), geometryDecodeData.sourceAxesStandard);
		if (res != avs::Result::OK)
			return res;
		return CreateFromDecodedGeometry(geometryDecodeData.target, dg, geometryDecodeData.filename_or_url);
	}

	return res;
}
avs::AttributeSemantic DracoToAvsAttributeSemantic(draco::PointAttribute::Type dracoType, int index = 0)
{
	switch (dracoType)
	{
	case draco::PointAttribute::Type::POSITION:
		return avs::AttributeSemantic::POSITION;
	case draco::PointAttribute::Type::NORMAL:
		return avs::AttributeSemantic::NORMAL;
	case draco::PointAttribute::Type::COLOR:
		return avs::AttributeSemantic::COLOR_0;
	case draco::PointAttribute::Type::TEX_COORD:
		return avs::AttributeSemantic::TEXCOORD_0;
	case draco::PointAttribute::Type::GENERIC:
		return avs::AttributeSemantic::TEXCOORD_1;
	case draco::PointAttribute::Type::TANGENT:
		return avs::AttributeSemantic::TANGENT;
	case draco::PointAttribute::Type::MATERIAL:
		return avs::AttributeSemantic::COUNT;
	case draco::PointAttribute::Type::JOINTS:
		return avs::AttributeSemantic::JOINTS_0;
	case draco::PointAttribute::Type::WEIGHTS:
		return avs::AttributeSemantic::WEIGHTS_0;
	case draco::PointAttribute::Type::INVALID:
	default:
		return avs::AttributeSemantic::COUNT;
	};
}
static vec3 convert(const draco::Vector3f &v) { return vec3(v[0], v[1], v[2]); }
static vec4 convert(const draco::Vector4f &v) { return vec4(v[0], v[1], v[2], v[3]); }

avs::Result GeometryDecoder::DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid,
														core::DecodedGeometry &dg,
														draco::Mesh &dracoMesh,
														platform::crossplatform::AxesStandard sourceAxesStandard)
{
	avs::CompressedSubMesh compressedSubMesh;
	dg.axesStandard = sourceAxesStandard;
	compressedSubMesh.indices_accessor = dg.next_id++;
	compressedSubMesh.material = dracoMesh.GetMaterialLibrary().NumMaterials() > 0 ? 1 : 0;
	compressedSubMesh.first_index = 0;
	compressedSubMesh.num_indices = dracoMesh.num_faces() * 3;
	size_t numAttributeSemantics = dracoMesh.num_attributes();
	for (size_t i = 0; i < numAttributeSemantics; i++)
	{
		auto *attr = dracoMesh.attribute(i);
		compressedSubMesh.attributeSemantics[i] = DracoToAvsAttributeSemantic(attr->attribute_type(), 0);
	}
	DracoMeshToPrimitiveArray(
		primitiveArrayUid, dg, dracoMesh, compressedSubMesh, platform::crossplatform::AxesStandard::OpenGL, m_DecompressedBuffers, m_DecompressedBufferIndex);
	auto &dracoMaterials = dracoMesh.GetMaterialLibrary();
	for (int i = 0; i < dracoMaterials.NumMaterials(); i++)
	{
		const draco::Material *dracoMaterial = dracoMaterials.GetMaterial(i);
		if (dracoMaterial)
		{
			avs::Material &material = dg.internalMaterials[i];
			material.name = dracoMaterial->GetName();
			material.materialMode = avs::MaterialMode::OPAQUE_MATERIAL;
			material.pbrMetallicRoughness.baseColorFactor = convert(dracoMaterial->GetColorFactor());
			material.pbrMetallicRoughness.baseColorTexture = {0};
			material.pbrMetallicRoughness.metallicFactor = dracoMaterial->GetMetallicFactor();
			material.pbrMetallicRoughness.metallicRoughnessTexture = {0};
			material.pbrMetallicRoughness.roughnessMultiplier = dracoMaterial->GetRoughnessFactor();
			material.pbrMetallicRoughness.roughnessOffset = 0.0f;
			material.normalTexture = {0};
			material.occlusionTexture = {0};
			material.emissiveTexture = {0};
			material.emissiveFactor = {0.0f, 0.0f, 0.0f};
		}
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::DecodeDracoScene( core::DecodedGeometry &subSceneDG,clientrender::ResourceCreator *target,
											  std::string filename_url,
											  avs::uid subscene_cache_uid,
											  avs::uid asset_uid,
											  draco::Scene &dracoScene,
											  platform::crossplatform::AxesStandard sourceAxesStandard)
{
	std::vector<avs::uid> node_uids(dracoScene.NumNodes());
	for (int n = 0; n < dracoScene.NumNodes(); n++)
	{
		const auto &dracoNode = dracoScene.GetNode(draco::SceneNodeIndex(n));
		node_uids[n] = GenerateLocalUid();
	}
	auto &dracoMaterials = dracoScene.GetMaterialLibrary();
	auto &dracoTextures = dracoMaterials.GetTextureLibrary();
	std::map<const draco::Texture *, avs::uid> texture_uids;
	// existing subSceneDG.server_or_cache_uid is where the subscene should be added.
	//  a new cache_uid should be created to identify it.
	std::vector<avs::uid> mesh_uids(dracoScene.NumMeshGroups());
	std::vector<avs::uid> material_uids(dracoMaterials.NumMaterials());
	std::map<avs::uid, std::string> texture_types;
	for (int i = 0; i < dracoMaterials.NumMaterials(); i++)
	{
		const draco::Material *dracoMaterial = dracoMaterials.GetMaterial(i);
		if (dracoMaterial)
		{
			avs::uid material_uid = GenerateLocalUid();
			avs::Material &material = subSceneDG.internalMaterials[material_uid];
			material.name = dracoMaterial->GetName();
			material.materialMode = avs::MaterialMode::OPAQUE_MATERIAL;
			material.pbrMetallicRoughness.baseColorFactor = convert(dracoMaterial->GetColorFactor());
			material.pbrMetallicRoughness.baseColorTexture = {0};
			material.pbrMetallicRoughness.metallicFactor = dracoMaterial->GetMetallicFactor();
			material.pbrMetallicRoughness.metallicRoughnessTexture = {0};
			material.pbrMetallicRoughness.roughnessMultiplier = dracoMaterial->GetRoughnessFactor();
			material.pbrMetallicRoughness.roughnessOffset = 0.0f;
			material.normalTexture = {0};
			material.occlusionTexture = {0};
			material.emissiveTexture = {0};
			material.emissiveFactor = convert(dracoMaterial->GetEmissiveFactor());
			material.doubleSided = dracoMaterial->GetDoubleSided();
			auto diffuseTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::COLOR);
			auto TextureUid = [&texture_uids](const draco::Texture *t)
			{
				auto f = texture_uids.find(t);
				if (f != texture_uids.end()) return f->second;
				avs::uid u = GenerateLocalUid();
				texture_uids[t] = u;
				return u;
			};
			if (diffuseTexture && diffuseTexture->texture())
			{
				avs::uid texture_uid = TextureUid(diffuseTexture->texture());
				material.pbrMetallicRoughness.baseColorTexture.index = texture_uid;
				material.pbrMetallicRoughness.baseColorTexture.texCoord = diffuseTexture->tex_coord_index();
				texture_uids[diffuseTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_diffuse";
			}
			auto metallicRoughnessTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::METALLIC_ROUGHNESS);
			if (metallicRoughnessTexture && metallicRoughnessTexture->texture())
			{
				avs::uid texture_uid = TextureUid(metallicRoughnessTexture->texture());
				material.pbrMetallicRoughness.metallicRoughnessTexture.index = texture_uid;
				material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = metallicRoughnessTexture->tex_coord_index();
				texture_uids[metallicRoughnessTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_metallicRoughness";
			}
			auto normalTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::NORMAL_TANGENT_SPACE);
			if (normalTexture && normalTexture->texture())
			{
				avs::uid texture_uid = TextureUid(normalTexture->texture());
				material.normalTexture.index = texture_uid;
				material.normalTexture.texCoord = normalTexture->tex_coord_index();
				texture_uids[normalTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_normal";
			}
			auto emissiveTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::EMISSIVE);
			if (emissiveTexture && emissiveTexture->texture())
			{
				avs::uid texture_uid = TextureUid(emissiveTexture->texture());
				material.emissiveTexture.index = texture_uid;
				material.emissiveTexture.texCoord = emissiveTexture->tex_coord_index();
				texture_uids[emissiveTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_emissive";
			}
			auto ambientOcclusionTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::AMBIENT_OCCLUSION);
			if (ambientOcclusionTexture && ambientOcclusionTexture->texture())
			{
				avs::uid texture_uid = TextureUid(ambientOcclusionTexture->texture());
				material.occlusionTexture.index = texture_uid;
				material.occlusionTexture.texCoord = ambientOcclusionTexture->tex_coord_index();
				texture_uids[ambientOcclusionTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_ambientOcclusion";
			}
			auto clearcoatTexture = dracoMaterial->GetTextureMapByType(draco::TextureMap::Type::CLEARCOAT);
			if (clearcoatTexture && clearcoatTexture->texture())
			{
				// material.pbrMetallicRoughness.baseColorTexture.index=texture_uids[clearcoatTexture->texture()];
				avs::uid texture_uid = TextureUid(clearcoatTexture->texture());
				texture_uids[clearcoatTexture->texture()] = texture_uid;
				texture_types[texture_uid] += material.name + "_clearcoat";
			}
			material_uids[i] = material_uid;
		}
	}
	for (int i = 0; i < dracoTextures.NumTextures(); i++)
	{
		const draco::Texture *dracoTexture = dracoTextures.GetTexture(i);
		avs::uid texture_uid = texture_uids[dracoTexture];
		if (!texture_uid)
			continue;
		auto &img = dracoTexture->source_image();
		std::string mime = img.mime_type();
		std::vector<uint8_t> data;
		// start with a uint16 N with the number of images
		// then a list of N uint32 offsets. Each is a subresource image. Then image 0 starts.
		data.resize(img.encoded_data().size() + 2 + 4);
		avs::TextureCompression compr=avs::TextureCompression::UNCOMPRESSED;
		if(dracoTexture->source_image().mime_type()=="image/png")
		{
			memcpy(data.data(), img.encoded_data().data(), img.encoded_data().size());
			compr=avs::TextureCompression::PNG;
		}
		if(dracoTexture->source_image().mime_type()=="image/jpeg")
		{
			compr=avs::TextureCompression::JPEG;
			memcpy(data.data(), img.encoded_data().data(), img.encoded_data().size());
		}
		std::string name = img.filename();
		if (!name.length())
		{
			name = filename_url;
			size_t slash = filename_url.rfind("/");
			if (slash < filename_url.size()) name = filename_url.substr(slash + 1, filename_url.size() - slash - 1);
			name += "_"s + texture_types[texture_uid];
		}
		if(compr!=avs::TextureCompression::UNCOMPRESSED)
		{
			avs::Texture avsTexture = {name, 0, 0, 0, 1, 1, false, avs::TextureFormat::RGBA8, 1.0f, compr, true};
			avsTexture.compressedData = std::move(data);
			target->CreateTexture(subscene_cache_uid, texture_uid, avsTexture);
		}
	}
	for (int m = 0; m < dracoScene.NumMeshGroups(); m++)
	{
		mesh_uids[m] = GenerateLocalUid();
		auto *dracoMeshGroup = dracoScene.GetMeshGroup(draco::MeshGroupIndex(m));
		avs::uid mesh_uid = mesh_uids[m];
		for (int j = 0; j < dracoMeshGroup->NumMeshInstances(); j++)
		{
			auto &dracoMeshInstance = dracoMeshGroup->GetMeshInstance(j);
			auto &dracoMesh = dracoScene.GetMesh(draco::MeshIndex(dracoMeshInstance.mesh_index));
			avs::CompressedSubMesh compressedSubMesh;
			compressedSubMesh.indices_accessor = subSceneDG.next_id++;
			compressedSubMesh.material = material_uids[dracoMeshInstance.material_index];
			compressedSubMesh.first_index = 0;
			compressedSubMesh.num_indices = dracoMesh.num_faces() * 3;
			size_t numAttributeSemantics = dracoMesh.num_attributes();
			for (size_t i = 0; i < numAttributeSemantics; i++)
			{
				auto *attr = dracoMesh.attribute(i);
				compressedSubMesh.attributeSemantics[i] = DracoToAvsAttributeSemantic(attr->attribute_type(), 0);
			}
			DracoMeshToPrimitiveArray(mesh_uid,
									  subSceneDG,
									  dracoMesh,
									  compressedSubMesh,
									  platform::crossplatform::AxesStandard::OpenGL,
									  m_DecompressedBuffers,
									  m_DecompressedBufferIndex);
		}
	}
	for (int n = 0; n < dracoScene.NumNodes(); n++)
	{
		const auto &dracoNode = dracoScene.GetNode(draco::SceneNodeIndex(n));
		// Each node points to a meshgroup. Each meshgroup has a list of mesh instances, each mesh instance has a mesh index and a material index.
		Eigen::Matrix4d m = dracoNode->GetTrsMatrix().ComputeTransformationMatrix();
		mat4d mat = *((mat4d *)&m);
		auto meshGroupIndex = dracoNode->GetMeshGroupIndex();
		avs::Node avsNode;
		avsNode.name = dracoNode->GetName();
		avsNode.stationary = true;
		avsNode.holder_client_id = 0;
		auto p = dracoNode->NumParents() ? dracoNode->Parent(0) : draco::SceneNodeIndex(0);
		avsNode.parentID = (dracoNode->NumParents() ? node_uids[p.value()] : (avs::uid)(0));
		auto mt = dracoNode->GetTrsMatrix();

		platform::crossplatform::AxesStandard axesStandard = platform::crossplatform::AxesStandard::OpenGL;

		if (mt.MatrixSet())
		{
			auto matrix = mt.Matrix().value();
			Eigen::Affine3d aff;
			aff = matrix;

			auto tr = aff.translation();
			avsNode.localTransform.position = vec3(tr.coeff(0), tr.coeff(1), tr.coeff(2));

			{
				auto rt = aff.rotation();
				Eigen::Quaterniond q(rt);
				avsNode.localTransform.rotation = {(float)q.x(), (float)q.y(), (float)q.z(), (float)q.w()};
			}
		}
		else
		{
			auto tr = mt.Translation().value();
			auto rt = mt.Rotation().value();
			auto sc = mt.Scale().value();
			if (mt.TranslationSet())
			{
				avsNode.localTransform.position = {(float)tr.coeff(0), (float)tr.coeff(1), (float)tr.coeff(2)};
			}
			if (mt.RotationSet())
			{
				avsNode.localTransform.rotation = {(float)rt.x(), (float)rt.y(), (float)rt.z(), (float)rt.w()};
			}
			if (mt.ScaleSet())
			{
				avsNode.localTransform.scale = {(float)sc.coeff(0), (float)sc.coeff(1), (float)sc.coeff(2)};
			}
		}
		if (axesStandard != platform::crossplatform::AxesStandard::Engineering)
		{
			avsNode.localTransform.position = platform::crossplatform::ConvertPosition(
				subSceneDG.axesStandard, platform::crossplatform::AxesStandard::Engineering, avsNode.localTransform.position);
			platform::crossplatform::Quaternionf q = platform::crossplatform::ConvertRotation(
				subSceneDG.axesStandard, platform::crossplatform::AxesStandard::Engineering, avsNode.localTransform.rotation);
			avsNode.localTransform.rotation = (const float *)&q;
			avsNode.localTransform.scale = platform::crossplatform::ConvertScale(
				subSceneDG.axesStandard, platform::crossplatform::AxesStandard::Engineering, avsNode.localTransform.scale);
		}
		avsNode.data_type = avs::NodeDataType::Mesh;
		if (meshGroupIndex.value() < dracoScene.NumMeshGroups())
		{
			const draco::MeshGroup *dracoMeshGroup = dracoScene.GetMeshGroup(meshGroupIndex);
			if (dracoMeshGroup)
				for (int i = 0; i < dracoMeshGroup->NumMeshInstances(); i++)
				{
					const auto &meshInstance = dracoMeshGroup->GetMeshInstance(i);
					avs::uid material_uid = material_uids[meshInstance.material_index];
					avsNode.materials.push_back(material_uid);
				}
			avsNode.data_uid = mesh_uids[meshGroupIndex.value()];
		}
		subSceneDG.nodes.emplace(node_uids[n], avsNode);
	}
	std::vector<avs::uid> skeleton_uids;
	for (int i = 0; i < dracoScene.NumSkins(); i++)
	{
		draco::SkinIndex skinIndex(i);
		const auto &dracoSkin = dracoScene.GetSkin(skinIndex);
		avs::uid skeleton_uid = GenerateLocalUid();
		skeleton_uids.push_back(skeleton_uid);
		avs::Skeleton &avsSkeleton = subSceneDG.skeletons[skeleton_uid];

		avsSkeleton.name = fmt::format("{0} skeleton {1}", filename_url, i);
		int numBones = dracoSkin->NumJoints();
		avsSkeleton.boneIDs.resize(numBones);
		//  We'll only use boneIDs in this case because the joints are actual nodes.
		for (int j = 0; j < numBones; j++)
		{
			int draco_joint_index = dracoSkin->GetJoint(j).value();
			avsSkeleton.boneIDs[j] = node_uids[draco_joint_index];
		}
		draco::NodeAnimationData dracoNodeAnimData = dracoSkin->GetInverseBindMatrices();
		if (dracoNodeAnimData.type() != draco::NodeAnimationData::Type::MAT4)
		{
			// problem...
			TELEPORT_CERR << "Wrong data type for inverse bind matrices\n";
		}
	}
	// find skeleton nodes and skinned mesh nodes.
	for (int n = 0; n < dracoScene.NumNodes(); n++)
	{
		auto &avsNode = subSceneDG.nodes[node_uids[n]];
		const auto &dracoNode = dracoScene.GetNode(draco::SceneNodeIndex(n));
		auto skinIndex = dracoNode->GetSkinIndex();
		if (skinIndex < dracoScene.NumSkins())
		{
			int i = skinIndex.value();
			avs::uid skeleton_uid = skeleton_uids[i];
			avs::Skeleton &avsSkeleton = subSceneDG.skeletons[skeleton_uid];
			avsNode.skeletonID = skeleton_uid;
			// Just directly map the bones to joints:
			avsNode.joint_indices.resize(avsSkeleton.boneIDs.size());
			for (int j = 0; j < avsNode.joint_indices.size(); j++)
			{
				avsNode.joint_indices[j] = j;
			}
			// TODO: Not where this should go.
			const auto &dracoSkin = dracoScene.GetSkin(skinIndex);
			draco::NodeAnimationData dracoNodeAnimData = dracoSkin->GetInverseBindMatrices();
			int numInverseBinds = dracoNodeAnimData.count();
			avsNode.inverseBindMatrices.resize(numInverseBinds);
			const float *invBindPtr = dracoNodeAnimData.GetData()->data();
			for (int j = 0; j < numInverseBinds; j++)
			{
				const mat4 &b = *((const mat4 *)invBindPtr);
				avsNode.inverseBindMatrices[j] =
					platform::crossplatform::ConvertMatrix(subSceneDG.axesStandard, platform::crossplatform::AxesStandard::Engineering, b);
				avsNode.inverseBindMatrices[j].transpose();
				invBindPtr += 16;
			}
		}
	}
	for (int n = 0; n < dracoScene.NumNodes(); n++)
	{
		auto &avsNode = subSceneDG.nodes[node_uids[n]];
		const auto &dracoNode = dracoScene.GetNode(draco::SceneNodeIndex(n));
		for (int i = 0; i < dracoNode->NumChildren(); i++)
		{
			const auto &childNode = dracoScene.GetNode(draco::SceneNodeIndex(dracoNode->Child(i)));
			auto &avsChild = subSceneDG.nodes[node_uids[(int)dracoNode->Child(i).value()]];
			if (childNode->NumParents() != 1)
			{
				std::cerr << "" << std::endl;
				continue;
			}
			if (childNode->Parent(0) != n)
			{
				std::cerr << "" << std::endl;
				continue;
			}
		}
	}
	return CreateFromDecodedGeometry(target, subSceneDG, filename_url);
}

// NOTE the inefficiency here, we're coding into "DecodedGeometry", but that is then immediately converted to a MeshCreate.
avs::Result GeometryDecoder::DracoMeshToDecodedGeometry(avs::uid primitiveArrayUid,
														core::DecodedGeometry &dg,
														const avs::CompressedMesh &compressedMesh,
														platform::crossplatform::AxesStandard sourceAxesStandard)
{
	size_t primitiveArraysSize = compressedMesh.subMeshes.size();
	dg.primitiveArrays[primitiveArrayUid].reserve(primitiveArraysSize);
	for (size_t i = 0; i < primitiveArraysSize; i++)
	{
		draco::Mesh dracoMesh;
		const avs::CompressedSubMesh &subMesh = compressedMesh.subMeshes[i];
		{
			draco::Decoder dracoDecoder;
			draco::DecoderBuffer dracoDecoderBuffer;
			dracoDecoderBuffer.Init((const char *)subMesh.buffer.data(), subMesh.buffer.size());
			draco::Status dracoStatus = dracoDecoder.DecodeBufferToGeometry(&dracoDecoderBuffer, &dracoMesh);
			if (!dracoStatus.ok())
			{
				TELEPORT_CERR << "Draco decode failed: " << (uint32_t)dracoStatus.code() << std::endl;
				return avs::Result::DecoderBackend_DecodeFailed;
			}
			DracoMeshToPrimitiveArray(primitiveArrayUid, dg, dracoMesh, subMesh, sourceAxesStandard, m_DecompressedBuffers, m_DecompressedBufferIndex);
		}
	}
	return avs::Result::OK;
}
#pragma endregion DracoDecoding

avs::Result GeometryDecoder::CreateFromDecodedGeometry(clientrender::ResourceCreator *target, core::DecodedGeometry &dg, const std::string &name)
{
	// Create the materials:
	for (auto m : dg.internalMaterials)
	{
		auto mat_uid = m.first;
		const auto &avsMaterial = m.second;
		target->CreateMaterial(dg.server_or_cache_uid, mat_uid, avsMaterial);
	}
	for (auto s : dg.skeletons)
	{
		auto s_uid = s.first;
		const auto &avsSkeleton = s.second;
		target->CreateSkeleton(dg.server_or_cache_uid, s_uid, avsSkeleton);
	}
	// Create the meshes:
	// TODO: Is there any point in FIRST creating DecodedGeometry THEN translating that to MeshCreate, THEN using MeshCreate to
	// 	   create the mesh? Why not go direct to MeshCreate??
	// dg is complete, now send to avs::GeometryTargetBackendInterface
	for (phmap::flat_hash_map<avs::uid, std::vector<core::PrimitiveArray>>::iterator it = dg.primitiveArrays.begin(); it != dg.primitiveArrays.end(); it++)
	{
		size_t index = 0;
		avs::MeshCreate meshCreate;
		meshCreate.cache_uid = dg.server_or_cache_uid;
		meshCreate.mesh_uid = it->first;
		std::vector<core::PrimitiveArray> &primArrays = it->second;
		meshCreate.m_MeshElementCreate.resize(it->second.size());
		// Primitive array elements in each mesh.
		for (const auto &primitiveArray : it->second)
		{
			avs::MeshElementCreate &meshElementCreate = meshCreate.m_MeshElementCreate[index];
			if (primitiveArray.material > 0)
			{
				meshElementCreate.internalMaterial = std::make_shared<avs::Material>(dg.internalMaterials[(int)primitiveArray.material - 1]);
			}
			meshElementCreate.vb_id = primitiveArray.attributes[0].accessor;
			size_t vertexCount = 0;
			for (size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				const avs::Attribute &attrib = primitiveArray.attributes[i];
				const avs::Accessor &accessor = dg.accessors[attrib.accessor];
				if (attrib.semantic == avs::AttributeSemantic::POSITION) meshElementCreate.m_VertexCount = vertexCount = accessor.count;
			}
			for (size_t i = 0; i < primitiveArray.attributeCount; i++)
			{
				// Vertices
				const avs::Attribute &attrib = primitiveArray.attributes[i];
				const avs::Accessor &accessor = dg.accessors[attrib.accessor];
				const avs::BufferView &bufferView = dg.bufferViews[accessor.bufferView];
				const avs::GeometryBuffer &buffer = dg.buffers[bufferView.buffer];
				const uint8_t *data = buffer.data + bufferView.byteOffset;

				switch (attrib.semantic)
				{
				case avs::AttributeSemantic::POSITION:
					meshElementCreate.m_Vertices = reinterpret_cast<const vec3 *>(data);
					continue;
				case avs::AttributeSemantic::TANGENTNORMALXZ:
				{
					size_t tnSize = 0;
					tnSize = avs::GetComponentSize(accessor.componentType) * avs::GetDataTypeSize(accessor.type);
					meshElementCreate.m_TangentNormalSize = tnSize;
					meshElementCreate.m_TangentNormals = reinterpret_cast<const uint8_t *>(data);
					continue;
				}
				case avs::AttributeSemantic::NORMAL:
					meshElementCreate.m_Normals = reinterpret_cast<const vec3 *>(data);
					if (accessor.count != vertexCount)
					{
						TELEPORT_CERR << "Accessor count mismatch in " << name.c_str() << "\n";
					}
					continue;
				case avs::AttributeSemantic::TANGENT:
					meshElementCreate.m_Tangents = reinterpret_cast<const vec4 *>(data);
					if (accessor.count != vertexCount)
					{
						TELEPORT_CERR << "Accessor count mismatch in " << name.c_str() << "\n";
					}
					continue;
				case avs::AttributeSemantic::TEXCOORD_0:
					meshElementCreate.m_UV0s = reinterpret_cast<const vec2 *>(data);
					if (accessor.count != vertexCount)
					{
						TELEPORT_CERR << "Accessor count mismatch in " << name.c_str() << "\n";
					}
					continue;
				case avs::AttributeSemantic::TEXCOORD_1:
					meshElementCreate.m_UV1s = reinterpret_cast<const vec2 *>(data);
					if (accessor.count != vertexCount)
					{
						TELEPORT_CERR << "Accessor count mismatch in " << name.c_str() << "\n";
					}
					continue;
				case avs::AttributeSemantic::COLOR_0:
					meshElementCreate.m_Colors = reinterpret_cast<const vec4 *>(data);
					meshElementCreate.colourType = accessor.componentType;
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::JOINTS_0:
					meshElementCreate.m_Joints = reinterpret_cast<const vec4 *>(data);
					meshElementCreate.jointType = accessor.componentType;
					assert(accessor.count == vertexCount);
					continue;
				case avs::AttributeSemantic::WEIGHTS_0:
					meshElementCreate.m_Weights = reinterpret_cast<const vec4 *>(data);
					meshElementCreate.weightType = accessor.componentType;
					assert(accessor.count == vertexCount);
					continue;
				default:
					TELEPORT_CERR << "Unknown attribute semantic: " << (uint32_t)attrib.semantic << std::endl;
					continue;
				}
			}
			if (dg.axesStandard != platform::crossplatform::AxesStandard::Engineering)
			{
				for (size_t i = 0; i < meshElementCreate.m_VertexCount; i++)
				{
					vec3 p = platform::crossplatform::ConvertPosition(
						dg.axesStandard, platform::crossplatform::AxesStandard::Engineering, meshElementCreate.m_Vertices[i]);
					const_cast<vec3 *>(meshElementCreate.m_Vertices)[i] = p;
					if (meshElementCreate.m_Normals)
					{
						vec3 n = platform::crossplatform::ConvertPosition(
							dg.axesStandard, platform::crossplatform::AxesStandard::Engineering, meshElementCreate.m_Normals[i]);
						const_cast<vec3 *>(meshElementCreate.m_Normals)[i] = n;
					}
					if (meshElementCreate.m_Tangents)
					{
						vec3 t = platform::crossplatform::ConvertPosition(
							dg.axesStandard, platform::crossplatform::AxesStandard::Engineering, meshElementCreate.m_Tangents[i].xyz);
						vec4 T;
						T.xyz = t;
						T.w = 1.0;
						const_cast<vec4 *>(meshElementCreate.m_Tangents)[i] = T;
					}
				}
			}
			// Indices
			const avs::Accessor &indicesAccessor = dg.accessors[primitiveArray.indices_accessor];
			const avs::BufferView &indicesBufferView = dg.bufferViews[indicesAccessor.bufferView];
			const avs::GeometryBuffer &indicesBuffer = dg.buffers[indicesBufferView.buffer];
			size_t componentSize = avs::GetComponentSize(indicesAccessor.componentType);
			meshElementCreate.ib_id = primitiveArray.indices_accessor;
			meshElementCreate.m_Indices = (indicesBuffer.data + indicesBufferView.byteOffset + indicesAccessor.byteOffset);
			if (indicesBuffer.byteLength < indicesBufferView.byteOffset + indicesAccessor.byteOffset + indicesAccessor.count * componentSize)
			{
				TELEPORT_CERR << "Index buffer too small in " << name.c_str() << "\n";
				meshElementCreate.m_IndexCount=(indicesBuffer.byteLength-indicesBufferView.byteOffset-indicesAccessor.byteOffset)/componentSize;
			}
			else
				meshElementCreate.m_IndexCount = indicesAccessor.count;
			meshElementCreate.m_IndexSize = componentSize;
			meshElementCreate.m_ElementIndex = index;
			index++;
		}
		meshCreate.name = name;
		meshCreate.clockwiseFaces = dg.clockwiseFaces;
		avs::Result result = target->CreateMesh(meshCreate);
		if (result != avs::Result::OK)
		{
			return result;
		}
	}

	for (auto m : dg.nodes)
	{
		auto node_uid = m.first;
		const auto &avsNode = m.second;
		target->CreateNode(dg.server_or_cache_uid, node_uid, avsNode);
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMesh(GeometryDecodeData &geometryDecodeData)
{
	// Parse buffer and fill struct DecodedGeometry
	core::DecodedGeometry dg = {};
	dg.axesStandard = platform::crossplatform::AxesStandard::Engineering;
	dg.server_or_cache_uid = geometryDecodeData.server_or_cache_uid;
	dg.clear();
	avs::uid uid = geometryDecodeData.uid;
	std::string name;
	m_DecompressedBuffers.clear();
	const size_t MAX_ATTR_COUNT = 20;
	if (MAX_ATTR_COUNT >= m_DecompressedBuffers.size()) m_DecompressedBuffers.resize(MAX_ATTR_COUNT);
	m_DecompressedBufferIndex = 0;
	avs::CompressedMesh compressedMesh;
	uint16_t version = 0;
	if (geometryDecodeData.geometryFileFormat == GeometryFileFormat::GLTF_TEXT || geometryDecodeData.geometryFileFormat == GeometryFileFormat::GLTF_BINARY)
	{
		return DecodeGltf(geometryDecodeData);
	}
	else
	{
		compressedMesh.meshCompressionType = (avs::MeshCompressionType)NextByte;
		if (compressedMesh.meshCompressionType == avs::MeshCompressionType::DRACO)
		{
			version = NextUint16;
			int32_t version_number = NextUint32;

			if (!readString(geometryDecodeData, compressedMesh.name)) return avs::Result::Failed;

			// inverse bind matrices, if required. These are not considered to be part of the mesh
			// by GLTF, but Unity exports them as part of the mesh.
			if (version >= 1)
			{
				size_t inv_bind_datasize = NextUint64;
				if (inv_bind_datasize > 0)
				{
					std::vector<uint8_t> inv_bind_data(inv_bind_datasize);
					dg.meshInverseBindMatrices.resize(inv_bind_datasize / sizeof(mat4));
					CopyChunk((uint8_t *)inv_bind_data.data(), inv_bind_datasize);
					for (int i = 0; i < dg.meshInverseBindMatrices.size(); i++)
					{
						dg.meshInverseBindMatrices[i] = mat4::identity();
					}
					memcpy(dg.meshInverseBindMatrices.data(), inv_bind_data.data(), sizeof(mat4) * dg.meshInverseBindMatrices.size());
				}
			}
			if (geometryDecodeData.saveToDisk)
				saveBuffer(geometryDecodeData, std::string("meshes/" + name + ".draco"));
			size_t num_elements = (size_t)NextUint32;
			if (num_elements > 1000)
				return avs::Result::Failed;
			compressedMesh.subMeshes.resize(num_elements);
			for (size_t i = 0; i < num_elements; i++)
			{
				auto &subMesh				 = compressedMesh.subMeshes[i];
				subMesh.indices_accessor	 = 0;
				subMesh.material			 = 0;
				subMesh.first_index			 = 0;
				subMesh.num_indices			 = 0;
				size_t numAttributeSemantics = 0;
				size_t bufferSize = NextUint64;
				if (bufferSize > 1024 * 1024 * 128 || bufferSize > geometryDecodeData.bytesRemaining())
				{
					TELEPORT_WARN("Failed to decode mesh {} {}: buffer size is too big.", uid, name);
					return avs::Result::Failed;
				}
				subMesh.buffer.resize(bufferSize);
				copy<uint8_t>(subMesh.buffer.data(), geometryDecodeData.data.data(), geometryDecodeData.offset, bufferSize);
			}
			// Anything sent to us is already in the correct form.
			avs::Result result = DracoMeshToDecodedGeometry(uid, dg, compressedMesh, platform::crossplatform::AxesStandard::Engineering);
			if (result != avs::Result::OK) return result;
		}
		else if (compressedMesh.meshCompressionType == avs::MeshCompressionType::NONE)
		{
			int32_t version_number = NextUint32;
			if (!readString(geometryDecodeData, name)) return avs::Result::Failed;
			compressedMesh.name = name;
			size_t primitiveArraysSize = NextUint64;
			dg.primitiveArrays[uid].reserve(primitiveArraysSize);

			for (size_t j = 0; j < primitiveArraysSize; j++)
			{
				size_t attributeCount = NextUint64;
				avs::uid indices_accessor = NextUint64;
				avs::uid material = NextUint64;
				avs::PrimitiveMode primitiveMode = (avs::PrimitiveMode)NextUint32;

				std::vector<avs::Attribute> attributes;
				attributes.reserve(attributeCount);
				for (size_t k = 0; k < attributeCount; k++)
				{
					avs::AttributeSemantic semantic = (avs::AttributeSemantic)NextUint64;
					avs::uid accessor = NextUint64;
					attributes.push_back({semantic, accessor});
				}

				dg.primitiveArrays[uid].push_back({attributeCount, attributes, indices_accessor, material, primitiveMode});
			}
			size_t accessorsSize = NextUint64;
			for (size_t j = 0; j < accessorsSize; j++)
			{
				avs::uid acc_uid = NextUint64;
				avs::Accessor::DataType type = (avs::Accessor::DataType)NextUint32;
				avs::ComponentType componentType = (avs::ComponentType)NextUint32;
				size_t count = NextUint64;
				avs::uid bufferView = NextUint64;
				size_t byteOffset = NextUint64;

				dg.accessors[acc_uid] = {type, componentType, count, bufferView, byteOffset};
			}
			size_t bufferViewsSize = NextUint64;
			for (size_t j = 0; j < bufferViewsSize; j++)
			{
				avs::uid bv_uid = NextUint64;
				avs::uid buffer = NextUint64;
				size_t byteOffset = NextUint64;
				size_t byteLength = NextUint64;
				size_t byteStride = NextUint64;

				dg.bufferViews[bv_uid] = {buffer, byteOffset, byteLength, byteStride};
			}

			size_t buffersSize = NextUint64;
			for (size_t j = 0; j < buffersSize; j++)
			{
				avs::uid key = NextUint64;
				dg.buffers[key] = {0, nullptr};
				dg.buffers[key].byteLength = NextUint64;
				if (geometryDecodeData.data.size() < geometryDecodeData.offset + dg.buffers[key].byteLength)
				{
					return avs::Result::GeometryDecoder_InvalidBufferSize;
				}

				dg.buffers[key].data = (geometryDecodeData.data.data() + geometryDecodeData.offset);
				geometryDecodeData.offset += dg.buffers[key].byteLength;
			}
		}
		else
		{
			TELEPORT_CERR << "Unknown meshCompressionType: " << (uint32_t)compressedMesh.meshCompressionType << std::endl;
			return avs::Result::DecoderBackend_DecodeFailed;
		}
		return CreateFromDecodedGeometry(geometryDecodeData.target, dg, name);
	}
}

avs::Result GeometryDecoder::decodeMaterial(GeometryDecodeData &geometryDecodeData)
{
	avs::Material material;
	avs::uid mat_uid = geometryDecodeData.uid;
	if (!readString(geometryDecodeData, material.name)) return avs::Result::Failed;
	material.materialMode = (avs::MaterialMode)NextByte;
	material.pbrMetallicRoughness.baseColorTexture.index = NextUint64;
	material.pbrMetallicRoughness.baseColorTexture.texCoord = NextByte;
	material.pbrMetallicRoughness.baseColorTexture.tiling.x = NextFloat;
	material.pbrMetallicRoughness.baseColorTexture.tiling.y = NextFloat;
	material.pbrMetallicRoughness.baseColorFactor.x = NextFloat;
	material.pbrMetallicRoughness.baseColorFactor.y = NextFloat;
	material.pbrMetallicRoughness.baseColorFactor.z = NextFloat;
	material.pbrMetallicRoughness.baseColorFactor.w = NextFloat;

	material.pbrMetallicRoughness.metallicRoughnessTexture.index = NextUint64;
	material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord = NextByte;
	material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.x = NextFloat;
	material.pbrMetallicRoughness.metallicRoughnessTexture.tiling.y = NextFloat;
	material.pbrMetallicRoughness.metallicFactor = NextFloat;
	material.pbrMetallicRoughness.roughnessMultiplier = NextFloat;
	material.pbrMetallicRoughness.roughnessOffset = NextFloat;

	material.normalTexture.index = NextUint64;
	material.normalTexture.texCoord = NextByte;
	material.normalTexture.tiling.x = NextFloat;
	material.normalTexture.tiling.y = NextFloat;
	material.normalTexture.scale = NextFloat;

	material.occlusionTexture.index = NextUint64;
	material.occlusionTexture.texCoord = NextByte;
	material.occlusionTexture.tiling.x = NextFloat;
	material.occlusionTexture.tiling.y = NextFloat;
	material.occlusionTexture.strength = NextFloat;

	material.emissiveTexture.index = NextUint64;
	material.emissiveTexture.texCoord = NextByte;
	material.emissiveTexture.tiling.x = NextFloat;
	material.emissiveTexture.tiling.y = NextFloat;
	material.emissiveFactor.x = NextFloat;
	material.emissiveFactor.y = NextFloat;
	material.emissiveFactor.z = NextFloat;

	material.doubleSided = NextByte;
	material.lightmapTexCoordIndex = NextByte;

	size_t extensionCount = NextUint64;
	for (size_t i = 0; i < extensionCount; i++)
	{
		std::unique_ptr<avs::MaterialExtension> newExtension;
		avs::MaterialExtensionIdentifier id = static_cast<avs::MaterialExtensionIdentifier>(NextUint32);

		switch (id)
		{
		case avs::MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
			newExtension = std::make_unique<avs::SimpleGrassWindExtension>();
			newExtension->deserialise(geometryDecodeData.data, geometryDecodeData.offset);
			break;
		}

		material.extensions[id] = std::move(newExtension);
	}

	geometryDecodeData.target->CreateMaterial(geometryDecodeData.server_or_cache_uid, mat_uid, material);

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeMaterialInstance(GeometryDecodeData &geometryDecodeData) { return avs::Result::GeometryDecoder_Incomplete; }

avs::Result GeometryDecoder::decodeTexturePointer(GeometryDecodeData &geometryDecodeData)
{
	avs::uid texture_uid = geometryDecodeData.uid;
	uint16_t urlLength = NextUint16;
	FAIL_IF_INSUFFICIENT_BYTES_REMAINING(urlLength);
	// Mark the resource as received. As far as the Server is concerned, its job is done.
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(geometryDecodeData.server_or_cache_uid);
	geometryCache->ReceivedResource(texture_uid);
	string url((size_t)urlLength, ' ');
	copy<char>(url.data(), geometryDecodeData.data.data(), geometryDecodeData.offset, urlLength);
	return decodeFromWeb(geometryDecodeData.server_or_cache_uid, url, avs::GeometryPayloadType::Texture, geometryDecodeData.target, texture_uid);
}

avs::Result GeometryDecoder::decodeMeshPointer(GeometryDecodeData &geometryDecodeData)
{
	avs::uid mesh_uid = geometryDecodeData.uid;

	uint16_t urlLength = NextUint16;
	FAIL_IF_INSUFFICIENT_BYTES_REMAINING(urlLength);
	// Mark the resource as received. As far as the Server is concerned, its job is done.
	std::shared_ptr<GeometryCache> geometryCache=GeometryCache::GetGeometryCache(geometryDecodeData.server_or_cache_uid);
	geometryCache->ReceivedResource(mesh_uid);
	string url((size_t)urlLength, ' ');
	copy<char>(url.data(), geometryDecodeData.data.data(), geometryDecodeData.offset, urlLength);
	return decodeFromWeb(geometryDecodeData.server_or_cache_uid, url, avs::GeometryPayloadType::Mesh, geometryDecodeData.target, mesh_uid);
}

avs::Result GeometryDecoder::decodeTextureFromExtension(GeometryDecodeData &geometryDecodeData)
{
	path p(geometryDecodeData.filename_or_url);
	string ext = p.extension().generic_string();
	avs::Texture texture;
	if (ext == ".texture")
	{
		texture.compression = avs::TextureCompression::MULTIPLE_PNG;
	}
	else if (ext == ".png")
	{
		texture.compression = avs::TextureCompression::PNG;
	}
	else if (ext == ".ktx2" || ext == ".ktx")
	{
		texture.compression = avs::TextureCompression::KTX;
	}
	else
	{
		TELEPORT_WARN("Unknown texture format {0}", ext);
		return avs::Result::Failed;
	}
	texture.compressedData = std::move(geometryDecodeData.data);
	texture.name = p.filename().replace_extension("").generic_string();
	geometryDecodeData.target->CreateTexture(geometryDecodeData.server_or_cache_uid, geometryDecodeData.uid, texture);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeTexture(GeometryDecodeData &geometryDecodeData)
{
	avs::Texture texture;
	avs::uid texture_uid = geometryDecodeData.uid;
	if (geometryDecodeData.geometryFileFormat == GeometryFileFormat::FROM_EXTENSION)
	{
		return decodeTextureFromExtension(geometryDecodeData);
	}
	if (!readString(geometryDecodeData, texture.name)) return avs::Result::Failed;
	if (geometryDecodeData.saveToDisk) saveBuffer(geometryDecodeData, std::string("textures/" + texture.name + ".texture"));
	// what is the largest possible texture size? say 4096*4096*16?
	const uint64_t MAX_TEXTURE_SIZE = 4096;
	const uint64_t MAX_TEXTURE_BYTES = 16;

	// compression, or really the wrapper format, e.g. .ktx2, .basis, array-of-png's:
	texture.compression = static_cast<avs::TextureCompression>(NextUint32);
	if (texture.compression > avs::TextureCompression::KTX)
	{
		TELEPORT_WARN("Invalid Texture: {0}", texture.name);
		return avs::Result::Failed;
	}
	if (geometryDecodeData.offset >= geometryDecodeData.data.size()) return avs::Result::Failed;
	size_t dataSize = geometryDecodeData.data.size() - geometryDecodeData.offset;
	if (dataSize > MAX_TEXTURE_SIZE * MAX_TEXTURE_SIZE * MAX_TEXTURE_BYTES) return avs::Result::Failed;
	texture.compressedData.resize(dataSize);
	memcpy(texture.compressedData.data(), geometryDecodeData.data.data() + geometryDecodeData.offset, dataSize);
	geometryDecodeData.offset += dataSize;
	geometryDecodeData.target->CreateTexture(geometryDecodeData.server_or_cache_uid, texture_uid, texture);

	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeAnimation(GeometryDecodeData &geometryDecodeData)
{
	teleport::core::Animation animation;
	avs::uid animationID = geometryDecodeData.uid;
	if(geometryDecodeData.geometryFileFormat==GeometryFileFormat::GLTF_BINARY)
	{
		animation.name=geometryDecodeData.filename_or_url;
		animation.compressedData.resize(geometryDecodeData.bytesRemaining());
		memcpy(animation.compressedData.data(),geometryDecodeData.data.data()+geometryDecodeData.offset,animation.compressedData.size());
		return geometryDecodeData.target->CreateAnimation(geometryDecodeData.server_or_cache_uid, animationID, animation);
	}

	if (!readString(geometryDecodeData, animation.name))
		return avs::Result::Failed;
	if (geometryDecodeData.saveToDisk)
		saveBuffer(geometryDecodeData, std::string("animations/" + animation.name + ".anim"));

	animation.duration = NextFloat;
	if (animation.duration < 0)
		return avs::Result::Failed;
	size_t numk	   = NextUint64;
	size_t kf_size = sizeof(int16_t) + sizeof(teleport::core::Vector3Keyframe) + sizeof(teleport::core::Vector4Keyframe);
	if (geometryDecodeData.bytesRemaining() / numk < kf_size)
		return avs::Result::Failed;
	animation.boneKeyframes.resize(numk);
	for (size_t i = 0; i < animation.boneKeyframes.size(); i++)
	{
		teleport::core::TransformKeyframeList &transformKeyframe = animation.boneKeyframes[i];
		transformKeyframe.boneIndex								 = NextInt16;

		if (decodeVector3Keyframes(geometryDecodeData, transformKeyframe.positionKeyframes) != avs::Result::OK)
			return avs::Result::Failed;
		if (decodeVector4Keyframes(geometryDecodeData, transformKeyframe.rotationKeyframes) != avs::Result::OK)
			return avs::Result::Failed;
	}

	geometryDecodeData.target->CreateAnimation(geometryDecodeData.server_or_cache_uid, animationID, animation);

	return avs::Result::OK;
}

bool GeometryDecoder::readString(GeometryDecodeData &geometryDecodeData, std::string &str) const
{
	uint16_t nameLength = NextUint16;
	if (nameLength > geometryDecodeData.data.size() - geometryDecodeData.offset)
	{
		TELEPORT_WARN("Tried to read {} bytes for a string, only {} left in buffer.", nameLength, geometryDecodeData.data.size() - geometryDecodeData.offset);
		return false;
	}
	str.resize((size_t)nameLength);
	copy<char>(str.data(), geometryDecodeData.data.data(), geometryDecodeData.offset, nameLength);

	return true;
}

avs::Result GeometryDecoder::decodeRemoveNodes(GeometryDecodeData &geometryDecodeData)
{
	uint16_t num=NextUint16;
	if (num*sizeof(avs::uid) > geometryDecodeData.data.size() - geometryDecodeData.offset)
	{
		TELEPORT_WARN("Tried to read {} bytes for decodeRemoveNodes, only {} left in buffer.", num, geometryDecodeData.data.size() - geometryDecodeData.offset);
		return avs::Result::Failed;
	}
	for(uint16_t i=0;i<num;i++)
	{
		avs::uid u=NextUint64;
		geometryDecodeData.target->DeleteNode(geometryDecodeData.server_or_cache_uid,u);
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeNode(GeometryDecodeData &geometryDecodeData)
{
	avs::uid uid = geometryDecodeData.uid;

	avs::Node node;

	if (!readString(geometryDecodeData, node.name))
		return avs::Result::Failed;
	// TODO: Check bytes remaining vs min node size.
	node.localTransform = NextChunk(avs::Transform);
	node.stationary = (NextByte) != 0;
	node.holder_client_id = NextUint64;
	node.priority = NextUint32;
	node.parentID = NextUint64;

	size_t numComponents = (size_t)NextByte;
	for (size_t i = 0; i < numComponents; i++)
	{
		node.data_type = static_cast<avs::NodeDataType>(NextByte);

		switch (node.data_type)
		{
		case avs::NodeDataType::Mesh:
		{
			node.data_uid = NextUint64;

			node.skeletonID = NextUint64;
			NextList(uint16_t, int16_t, node.joint_indices) NextList(uint16_t, avs::uid, node.animations) NextList(uint16_t, avs::uid, node.materials)
			node.renderState.lightmapScaleOffset = NextVec4;
			node.renderState.globalIlluminationUid = NextUint64;
		}
		break;
		case avs::NodeDataType::Light:
			node.lightColour = NextVec4;
			node.lightRadius = NextFloat;
			node.lightRange = NextFloat;
			node.lightDirection = NextVec3;
			node.lightType = NextByte;
			break;
		case avs::NodeDataType::Link:
		{
			if (!readString(geometryDecodeData, node.url))
				return avs::Result::Failed;
			if (!readString(geometryDecodeData, node.query_url))
				return avs::Result::Failed;
		}
		case avs::NodeDataType::TextCanvas:
		{
			// nothing node-specific to add at present.
			node.data_uid = NextUint64;
		}
		break;
		default:
			break;
		};
	}
	geometryDecodeData.target->CreateNode(geometryDecodeData.server_or_cache_uid, uid, node);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeSkeleton(GeometryDecodeData &geometryDecodeData)
{
	static size_t skip = 0;
	if (skip)
	{
		geometryDecodeData.data.erase(geometryDecodeData.data.begin(), geometryDecodeData.data.begin() + skip);
	}
	avs::uid skeletonID = geometryDecodeData.uid;
	avs::Skeleton skeleton;
	if (!readString(geometryDecodeData, skeleton.name))
		return avs::Result::Failed;
	if (geometryDecodeData.saveToDisk)
		saveBuffer(geometryDecodeData, std::string("skeletons/" + skeleton.name + ".skeleton"));
	size_t numBones=NextUint64;
	skeleton.boneIDs.resize(numBones);
	for (size_t i = 0; i < numBones; i++)
	{
		skeleton.boneIDs[i] = NextUint64;
	}
	geometryDecodeData.target->CreateSkeleton(geometryDecodeData.server_or_cache_uid, skeletonID, skeleton);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeFontAtlas(GeometryDecodeData &geometryDecodeData)
{
	avs::uid fontAtlasUid = geometryDecodeData.uid;
	teleport::core::FontAtlas fontAtlas(fontAtlasUid);
	fontAtlas.name = geometryDecodeData.filename_or_url;
	if (geometryDecodeData.saveToDisk&&geometryDecodeData.filename_or_url.length())
		saveBuffer(geometryDecodeData, std::string("fonts/" + fontAtlas.name + ".fontAtlas"));
	fontAtlas.font_texture_uid = NextUint64;
	int numMaps = NextByte;
	for (int i = 0; i < numMaps; i++)
	{
		int sz = NextUint16;
		auto &fontMap = fontAtlas.fontMaps[sz];
		fontMap.lineHeight = NextFloat;
		uint16_t numGlyphs = NextUint16;
		size_t glyphsBytes = numGlyphs*sizeof(core::Glyph);
		if (glyphsBytes > geometryDecodeData.bytesRemaining())
		{
			TELEPORT_WARN("Tried to read {} glyphs = {} bytes but only {} bytes left in buffer.", numGlyphs, glyphsBytes, geometryDecodeData.bytesRemaining());
			return avs::Result::Failed;
		}
		fontMap.glyphs.resize(numGlyphs);
		for (uint16_t j = 0; j < numGlyphs; j++)
		{
			auto &glyph = fontMap.glyphs[j];
			copy<char>((char *)&glyph, geometryDecodeData.data.data(), geometryDecodeData.offset, sizeof(glyph));
		}
	}
	geometryDecodeData.target->CreateFontAtlas(geometryDecodeData.server_or_cache_uid, fontAtlasUid, geometryDecodeData.filename_or_url, fontAtlas);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeTextCanvas(GeometryDecodeData &geometryDecodeData) const
{
	clientrender::TextCanvasCreateInfo textCanvasCreateInfo;
	textCanvasCreateInfo.server_uid = geometryDecodeData.server_or_cache_uid;
	textCanvasCreateInfo.uid = geometryDecodeData.uid;
	textCanvasCreateInfo.font = NextUint64;
	textCanvasCreateInfo.size = NextUint32;
	textCanvasCreateInfo.lineHeight = NextFloat;
	copy<char>((char *)&textCanvasCreateInfo.colour, geometryDecodeData.data.data(), geometryDecodeData.offset, sizeof(textCanvasCreateInfo.colour));

	if (!readString(geometryDecodeData, textCanvasCreateInfo.text))
		return avs::Result::Failed;
	geometryDecodeData.target->CreateTextCanvas(textCanvasCreateInfo);
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeFloatKeyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::FloatKeyframe> &keyframes)
{
	uint16_t numk = NextUint16;
	keyframes.resize(numk);
	for (size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextFloat;
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeVector3Keyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::Vector3Keyframe> &keyframes)
{
	uint16_t numk = NextUint16;
	if (numk == 0) return avs::Result::OK;
	if (geometryDecodeData.bytesRemaining() / numk < sizeof(teleport::core::Vector3Keyframe)) return avs::Result::Failed;
	keyframes.resize(numk);
	for (size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextChunk(vec3);
	}
	return avs::Result::OK;
}

avs::Result GeometryDecoder::decodeVector4Keyframes(GeometryDecodeData &geometryDecodeData, std::vector<teleport::core::Vector4Keyframe> &keyframes)
{
	uint16_t numk = NextUint16;
	if (numk == 0) return avs::Result::OK;
	if (geometryDecodeData.bytesRemaining() / numk < sizeof(teleport::core::Vector4Keyframe)) return avs::Result::Failed;
	keyframes.resize(numk);
	for (size_t i = 0; i < keyframes.size(); i++)
	{
		keyframes[i].time = NextFloat;
		keyframes[i].value = NextChunk(vec4);
	}
	return avs::Result::OK;
}

void GeometryDecoder::saveBuffer(GeometryDecodeData &geometryDecodeData, const std::string &filename)
{
	platform::core::FileLoader *fileLoader = platform::core::FileLoader::GetFileLoader();
	std::string f = cacheFolder.length() ? (cacheFolder + "/") + filename : filename;
	fileLoader->Save((const void *)geometryDecodeData.data.data(), (unsigned)geometryDecodeData.data.size(), f.c_str(), false);
}