#include "GeometryStore.h"
#include "ErrorHandling.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#if defined ( _WIN32 )
#include <sys/stat.h>
#endif

#include "libavstream/geometry/animation_interface.h"
#include "draco/compression/encode.h"
#include "draco/compression/decode.h"
using namespace teleport;

//We need to use the experimental namespace if we are using MSVC 2017, but not for 2019+.
#if _MSC_VER < 1920
namespace filesystem = std::experimental::filesystem;
#else
namespace filesystem = std::filesystem;
#endif

template<class T>
std::vector<avs::uid> getVectorOfIDs(const std::map<avs::uid, T>& resourceMap)
{
	std::vector<avs::uid> ids(resourceMap.size());

	size_t i = 0;
	for(const auto& it : resourceMap)
	{
		ids[i++] = it.first;
	}

	return ids;
}

template<class T> T* getResource(std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	auto resource = resourceMap.find(id);
	return (resource != resourceMap.end()) ? &resource->second : nullptr;
}

//Const version.
template<class T> const T* getResource(const std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	auto resource = resourceMap.find(id);
	return (resource != resourceMap.end()) ? &resource->second : nullptr;
}

std::time_t GetFileWriteTime(const std::filesystem::path& filename)
{
#if defined ( _WIN32 )
	{
		struct _stat64 fileInfo;
		if(_wstati64(filename.wstring().c_str(), &fileInfo) != 0)
		{
			throw std::runtime_error("Failed to get last write time.");
		}
		return fileInfo.st_mtime;
	}
#else
	{
		auto fsTime = std::filesystem::last_write_time(filename);
		return decltype (fsTime)::clock::to_time_t(fsTime);
	}
#endif
}

const char* stringOf(avs::NodeDataType type)
{
	switch(type)
	{
	case avs::NodeDataType::Invalid:	return "Invalid";
	case avs::NodeDataType::None:		return "None";
	case avs::NodeDataType::Mesh:		return "Mesh";
	case avs::NodeDataType::Light:		return "Light";
	case avs::NodeDataType::Bone:		return "Bone";
	default:							return "Unimplemented";
	};
}


GeometryStore::GeometryStore()
{
	//Create look-up maps.
	meshes[avs::AxesStandard::EngineeringStyle];
	meshes[avs::AxesStandard::GlStyle];
	animations[avs::AxesStandard::EngineeringStyle];
	animations[avs::AxesStandard::GlStyle];
	skins[avs::AxesStandard::EngineeringStyle];
	skins[avs::AxesStandard::GlStyle];

}

GeometryStore::~GeometryStore()
{
}

void GeometryStore::saveToDisk() const
{
	saveResources(cachePath + "/" +TEXTURE_CACHE_PATH, textures);
	saveResources(cachePath + "/" +MATERIAL_CACHE_PATH, materials);
	saveResources(cachePath + "/" +MESH_PC_CACHE_PATH, meshes.at(avs::AxesStandard::EngineeringStyle));
	saveResources(cachePath + "/" +MESH_ANDROID_CACHE_PATH, meshes.at(avs::AxesStandard::GlStyle));
}

void GeometryStore::loadFromDisk(size_t& numMeshes, LoadedResource*& loadedMeshes, size_t& numTextures, LoadedResource*& loadedTextures, size_t& numMaterials, LoadedResource*& loadedMaterials)
{
	// Load in order of non-dependent to dependent resources, so that we can apply dependencies.
	loadResources(cachePath + "/" + TEXTURE_CACHE_PATH, textures);
	loadResources(cachePath + "/" + MATERIAL_CACHE_PATH, materials);
	loadResources(cachePath + "/" + MESH_PC_CACHE_PATH, meshes.at(avs::AxesStandard::EngineeringStyle));
	loadResources(cachePath + "/" + MESH_ANDROID_CACHE_PATH, meshes.at(avs::AxesStandard::GlStyle));

	numMeshes = meshes.at(avs::AxesStandard::EngineeringStyle).size();
	numTextures = textures.size();
	numMaterials = materials.size();

	int i = 0;
	loadedMeshes = new LoadedResource[numMeshes];
	for(auto& meshDataPair : meshes.at(avs::AxesStandard::EngineeringStyle))
	{
		BSTR meshName = _com_util::ConvertStringToBSTR(meshDataPair.second.mesh.name.c_str());
		loadedMeshes[i] = LoadedResource(meshDataPair.first, meshDataPair.second.guid, meshName, meshDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedTextures = new LoadedResource[numTextures];
	for(auto& textureDataPair : textures)
	{
		BSTR textureName = _com_util::ConvertStringToBSTR(textureDataPair.second.texture.name.c_str());
		loadedTextures[i] = LoadedResource(textureDataPair.first, textureDataPair.second.guid, textureName, textureDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedMaterials = new LoadedResource[numMaterials];
	for(auto& materialDataPair : materials)
	{
		BSTR materialName = _com_util::ConvertStringToBSTR(materialDataPair.second.material.name.c_str());
		loadedMaterials[i] = LoadedResource(materialDataPair.first, materialDataPair.second.guid, materialName, materialDataPair.second.lastModified);

		++i;
	}
}

void GeometryStore::reaffirmResources(int32_t numMeshes, ReaffirmedResource* reaffirmedMeshes, int32_t numTextures, ReaffirmedResource* reaffirmedTextures, int32_t numMaterials, ReaffirmedResource* reaffirmedMaterials)
{
	TELEPORT_COUT << "Renumbering resources.\node";

	//Copy data on the resources that were loaded.
	std::map<avs::AxesStandard, std::map<avs::uid, ExtractedMesh>> oldMeshes = meshes;
	std::map<avs::uid, ExtractedMaterial> oldMaterials = materials;
	std::map<avs::uid, ExtractedTexture> oldTextures = textures;

	//Delete the old data; we don't want to use the GeometryStore::clear(...) function as that would delete the pointers we want to copy.
	//Clear mesh lookup without messing with the structure.
	meshes[avs::AxesStandard::EngineeringStyle].clear();
	meshes[avs::AxesStandard::GlStyle].clear();
	materials.clear();
	textures.clear();

	TELEPORT_COUT << "Replacing " << oldMaterials.size() << " materials loaded from disk with " << numMaterials << " confirmed from managed code.\node";

	//Replace old IDs with their new IDs; fixing any links that need to be changed.

	//ASSUMPTION: Mesh sub-resources(e.g. index buffers) aren't shared, and as such it doesn't matter that their IDs aren't re-assigned.
	for(auto meshMapPair : oldMeshes)
	{
		avs::AxesStandard mapStandard = meshMapPair.first;
		for(int i = 0; i < numMeshes; i++)
		{
			meshes[mapStandard][reaffirmedMeshes[i].newID] = oldMeshes[mapStandard][reaffirmedMeshes[i].oldID];
		}
	}

	//Lookup to find the new ID from the old ID. For replacing texture IDs in materials. <Old ID, New ID>
	std::map<avs::uid, avs::uid> textureIDLookup;
	for(int i = 0; i < numTextures; i++)
	{
		avs::uid newID = reaffirmedTextures[i].newID;
		avs::uid oldID = reaffirmedTextures[i].oldID;

		textures[newID] = oldTextures[oldID];
		textureIDLookup[oldID] = newID;
	}

	//Takes old ID reference and replaces with the new ID in the lookup.
	auto replaceTextureID = [&textureIDLookup](avs::uid& textureID)
	{
		//If the texture doesn't exist it should have a zero filled in.
		//Which means the material has been updated, and will be re-extracted anyway (assuming the old index isn't zero).
		textureID = textureIDLookup[textureID];
	};

	for(int i = 0; i < numMaterials; i++)
	{
		avs::uid newID = reaffirmedMaterials[i].newID;

		materials[newID] = oldMaterials[reaffirmedMaterials[i].oldID];
		TELEPORT_COUT << "New ID: Material " << newID << ", was material " << reaffirmedMaterials[i].oldID << ".\n";

		//Replace all texture accessor indexes with their new index.
		replaceTextureID(materials[newID].material.pbrMetallicRoughness.baseColorTexture.index);
		replaceTextureID(materials[newID].material.pbrMetallicRoughness.metallicRoughnessTexture.index);
		replaceTextureID(materials[newID].material.normalTexture.index);
		replaceTextureID(materials[newID].material.occlusionTexture.index);
		replaceTextureID(materials[newID].material.emissiveTexture.index);
	}
}

void GeometryStore::clear(bool freeMeshBuffers)
{
	//Free memory for primitive attributes and geometry buffers.
	for(auto& standardPair : meshes)
	{
		for(auto& meshPair : standardPair.second)
		{
			for(avs::PrimitiveArray& primitive : meshPair.second.mesh.primitiveArrays)
			{
				delete[] primitive.attributes;
			}

			//Unreal just uses the pointer, but Unity copies them on the native side.
			if(freeMeshBuffers)
			{
				for(auto& bufferPair : meshPair.second.mesh.buffers)
				{
					delete[] bufferPair.second.data;
				}
			}
		}
	}

	//Free memory for texture pixel data.
	for(auto& idTexturePair : textures)
	{
		delete[] idTexturePair.second.texture.data;
	}

	//Free memory for shadow map pixel data.
	for(auto& idShadowPair : shadowMaps)
	{
		delete[] idShadowPair.second.texture.data;
	}

	//Clear lookup tables; we want to clear the resources inside them, not their structure.
	nodes.clear();
	skins[avs::AxesStandard::EngineeringStyle].clear();
	skins[avs::AxesStandard::GlStyle].clear();
	animations[avs::AxesStandard::EngineeringStyle].clear();
	animations[avs::AxesStandard::GlStyle].clear();
	meshes[avs::AxesStandard::EngineeringStyle].clear();
	meshes[avs::AxesStandard::GlStyle].clear();
	materials.clear();
	textures.clear();
	shadowMaps.clear();

	texturesToCompress.clear();
	lightNodes.clear();
}
void GeometryStore::setCompressionLevels(uint8_t str, uint8_t q)
{
	compressionStrength = str;
	compressionQuality= q;
}

const char* GeometryStore::getNodeName(avs::uid nodeID) const
{
	const avs::Node* node = getNode(nodeID);
	return node ? node->name.c_str() : "NULL";
}

std::vector<avs::uid> GeometryStore::getNodeIDs() const
{
	return getVectorOfIDs(nodes);
}

avs::Node* GeometryStore::getNode(avs::uid nodeID)
{
	return getResource(nodes, nodeID);
}

const avs::Node* GeometryStore::getNode(avs::uid nodeID) const
{
	return getResource(nodes, nodeID);
}

const std::map<avs::uid, avs::Node>& GeometryStore::getNodes() const
{
	return nodes;
}

avs::Skin* GeometryStore::getSkin(avs::uid skinID, avs::AxesStandard standard)
{
	return getResource(skins.at(standard), skinID);
}

const avs::Skin* GeometryStore::getSkin(avs::uid skinID, avs::AxesStandard standard) const
{
	return getResource(skins.at(standard), skinID);
}

avs::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard)
{
	return getResource(animations.at(standard), id);
}

const avs::Animation* GeometryStore::getAnimation(avs::uid id, avs::AxesStandard standard) const
{
	return getResource(animations.at(standard), id);
}

std::vector<avs::uid> GeometryStore::getMeshIDs() const
{
	//Every mesh map should be identical, so we just use the engineering style.
	return getVectorOfIDs(meshes.at(avs::AxesStandard::EngineeringStyle));
}

const avs::CompressedMesh* GeometryStore::getCompressedMesh(avs::uid meshID, avs::AxesStandard standard) const
{
	const ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->compressedMesh : nullptr);
}

avs::Mesh* GeometryStore::getMesh(avs::uid meshID, avs::AxesStandard standard)
{
	ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->mesh : nullptr);
}

const avs::Mesh* GeometryStore::getMesh(avs::uid meshID, avs::AxesStandard standard) const
{
	const ExtractedMesh* meshData = getResource(meshes.at(standard), meshID);
	return (meshData ? &meshData->mesh : nullptr);
}

std::vector<avs::uid> GeometryStore::getTextureIDs() const
{
	return getVectorOfIDs(textures);
}

avs::Texture* GeometryStore::getTexture(avs::uid textureID)
{
	ExtractedTexture* textureData = getResource(textures, textureID);
	return (textureData ? &textureData->texture : nullptr);
}

const avs::Texture* GeometryStore::getTexture(avs::uid textureID) const
{
	const ExtractedTexture* textureData = getResource(textures, textureID);
	return (textureData ? &textureData->texture : nullptr);
}

std::vector<avs::uid> GeometryStore::getMaterialIDs() const
{
	return getVectorOfIDs(materials);
}

avs::Material* GeometryStore::getMaterial(avs::uid materialID)
{
	ExtractedMaterial* materialData = getResource(materials, materialID);
	return materialData ? &materialData->material : nullptr;
}

const avs::Material* GeometryStore::getMaterial(avs::uid materialID) const
{
	const ExtractedMaterial* materialData = getResource(materials, materialID);
	return materialData ? &materialData->material : nullptr;
}

std::vector<avs::uid> GeometryStore::getShadowMapIDs() const
{
	return getVectorOfIDs(shadowMaps);
}

avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID)
{
	ExtractedTexture* textureData = getResource(shadowMaps, shadowID);
	return (textureData ? &textureData->texture : nullptr);
}

const avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID) const
{
	const ExtractedTexture* shadowMapData = getResource(shadowMaps, shadowID);
	return (shadowMapData ? &shadowMapData->texture : nullptr);
}

const std::map<avs::uid,avs::LightNodeResources>& GeometryStore::getLightNodes() const
{
	return lightNodes;
}

bool GeometryStore::hasNode(avs::uid id) const
{
	return nodes.find(id) != nodes.end();
}

bool GeometryStore::hasMesh(avs::uid id) const
{
	return meshes.at(avs::AxesStandard::EngineeringStyle).find(id) != meshes.at(avs::AxesStandard::EngineeringStyle).end();
}

bool GeometryStore::hasMaterial(avs::uid id) const
{
	return materials.find(id) != materials.end();
}

bool GeometryStore::hasTexture(avs::uid id) const
{
	return textures.find(id) != textures.end();
}

bool GeometryStore::hasShadowMap(avs::uid id) const
{
	return shadowMaps.find(id) != shadowMaps.end();
}

void GeometryStore::storeNode(avs::uid id, avs::Node& newNode)
{
	//Remove node before re-adding with new data. Why?
	//removeNode(id);
	nodes[id] = newNode;
	if(newNode.data_type == avs::NodeDataType::Light)
	{
		lightNodes[id]=avs::LightNodeResources{id, newNode.data_uid};
	}
}

void GeometryStore::storeSkin(avs::uid id, avs::Skin& newSkin, avs::AxesStandard sourceStandard)
{
	skins[avs::AxesStandard::EngineeringStyle][id] = avs::Skin::convertToStandard(newSkin, sourceStandard, avs::AxesStandard::EngineeringStyle);
	skins[avs::AxesStandard::GlStyle][id] = avs::Skin::convertToStandard(newSkin, sourceStandard, avs::AxesStandard::GlStyle);
}

void GeometryStore::storeAnimation(avs::uid id, avs::Animation& animation, avs::AxesStandard sourceStandard)
{
	animations[avs::AxesStandard::EngineeringStyle][id] = avs::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::EngineeringStyle);
	animations[avs::AxesStandard::GlStyle][id] = avs::Animation::convertToStandard(animation, sourceStandard, avs::AxesStandard::GlStyle);
}

draco::DataType ToDracoDataType(avs::Accessor::ComponentType componentType)
{
	switch(componentType)
	{
	case avs::Accessor::ComponentType::FLOAT:
		return draco::DataType::DT_FLOAT32;
	case avs::Accessor::ComponentType::DOUBLE:
		return draco::DataType::DT_FLOAT64;
	case avs::Accessor::ComponentType::HALF:
		return draco::DataType::DT_INVALID;
	case avs::Accessor::ComponentType::UINT:
		return draco::DataType::DT_INT32;
	case avs::Accessor::ComponentType::USHORT:
		return draco::DataType::DT_INT16;
	case avs::Accessor::ComponentType::UBYTE:
		return draco::DataType::DT_INT8;
	case avs::Accessor::ComponentType::INT:
		return draco::DataType::DT_INT32;
	case avs::Accessor::ComponentType::SHORT:
		return draco::DataType::DT_INT16;
	case avs::Accessor::ComponentType::BYTE:
		return draco::DataType::DT_INT8;
	};
}

draco::GeometryAttribute::Type ToDracoGeometryAttribute(avs::AttributeSemantic attributeSemantic)
{
	switch (attributeSemantic)
	{
	case avs::AttributeSemantic::POSITION:
		return draco::GeometryAttribute::Type::POSITION;
	case avs::AttributeSemantic::NORMAL:
		return draco::GeometryAttribute::Type::NORMAL;
	case avs::AttributeSemantic::TANGENT:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::TEXCOORD_0:
		return draco::GeometryAttribute::Type::TEX_COORD;
	case avs::AttributeSemantic::TEXCOORD_1:
		return draco::GeometryAttribute::Type::TEX_COORD;
	case avs::AttributeSemantic::COLOR_0:
		return draco::GeometryAttribute::Type::COLOR;
	case avs::AttributeSemantic::JOINTS_0:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::WEIGHTS_0:
		return draco::GeometryAttribute::Type::GENERIC;
	case avs::AttributeSemantic::TANGENTNORMALXZ:
		return draco::GeometryAttribute::Type::GENERIC;
	};
}

static bool CompressMesh(avs::CompressedMesh &compressedMesh,avs::Mesh &sourceMesh)
{
	compressedMesh.meshCompressionType = avs::MeshCompressionType::NONE;
	compressedMesh.name=sourceMesh.name;
	draco::Encoder dracoEncoder;
	static int encode_speed=3, decode_speed =7;
	static int pos_quantization=11;
	static int normal_quantization = 8;
	static int texc_quantization = 8;
	static int colour_quantization = 8;
	static int generic_quantization = 8;
	static bool allow_edgebreaker_encoding = true;
	dracoEncoder.SetSpeedOptions(encode_speed, decode_speed);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::POSITION, pos_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::NORMAL, normal_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::COLOR, colour_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::TEX_COORD, texc_quantization);
	dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::Type::GENERIC, generic_quantization);
	if (allow_edgebreaker_encoding)
		dracoEncoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
	else
		dracoEncoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
	size_t sourceSize=0;
	size_t compressedSize = 0;
	// create zero-based material indices:
	compressedMesh.subMeshes.resize(sourceMesh.primitiveArrays.size());
	// Primitive array elements in each mesh.
	draco::FaceIndex face_index(0);
	std::map<avs::uid, avs::AttributeSemantic> attributeSemantics;
	for (size_t i=0;i<sourceMesh.primitiveArrays.size();i++)
	{
		const auto& primitive = sourceMesh.primitiveArrays[i];
		auto &subMesh= compressedMesh.subMeshes[i];
		const avs::Accessor& indices_accessor = sourceMesh.accessors[primitive.indices_accessor];
		subMesh.material=primitive.material;
		subMesh.indices_accessor=primitive.indices_accessor;
		subMesh.first_index= 0;
		subMesh.num_indices = indices_accessor.count;
		size_t numTriangles = indices_accessor.count / 3;
	
		draco::Mesh dracoMesh;
		draco::EncoderBuffer dracoEncoderBuffer;
		size_t numVertices = 0;
		for (size_t j = 0; j < primitive.attributeCount; j++)
		{
			const avs::Attribute& attrib = primitive.attributes[j];
			const auto &attrib_accessor=sourceMesh.accessors[attrib.accessor];
			numVertices=std::max(numVertices, attrib_accessor.count);
			auto s= attributeSemantics.find(attrib.accessor);
			if(s== attributeSemantics.end())
				attributeSemantics[attrib.accessor]=attrib.semantic;
			 if(attrib.semantic!= attributeSemantics[attrib.accessor])
			{
				TELEPORT_CERR<<"Different attributes for submeshes, can't compress this: "<<sourceMesh.name.c_str()<<std::endl;
				return false;
			}
		}
		dracoMesh.set_num_points((uint32_t)numVertices);
		dracoMesh.SetNumFaces(numTriangles);
		for (const auto& a : sourceMesh.accessors)
		{
			const auto &accessor=a.second;
			draco::DataType dracoDataType = ToDracoDataType(accessor.componentType);
			auto s = attributeSemantics.find(a.first);
			if (s == attributeSemantics.end())
				continue;	// not an attribute.
			auto semantic=s->second;
			draco::GeometryAttribute::Type dracoGeometryAttributeType = ToDracoGeometryAttribute(semantic);
			avs::BufferView& bufferView = sourceMesh.bufferViews[accessor.bufferView];
			avs::GeometryBuffer& geometryBuffer = sourceMesh.buffers[bufferView.buffer];
			const uint8_t* data = geometryBuffer.data + bufferView.byteOffset;
			// Naively convert enum to integer.
			int8_t num_components = (int8_t)accessor.type;	
			int att_id = -1;
			draco::PointAttribute* attr = nullptr;
			draco::GeometryAttribute dracoGeometryAttribute;
			size_t stride = draco::DataTypeLength(dracoDataType) * num_components;
			dracoGeometryAttribute.Init(dracoGeometryAttributeType, nullptr, num_components, dracoDataType, semantic == avs::AttributeSemantic::NORMAL, stride, 0);
			att_id = dracoMesh.AddAttribute(dracoGeometryAttribute, true, (uint32_t)geometryBuffer.byteLength / stride);
			subMesh.attributeSemantics[att_id]=semantic;
			attr = dracoMesh.attribute(att_id);
			for (size_t j = 0; j < accessor.count; j++)
			{
				attr->SetAttributeValue(draco::AttributeValueIndex(j), data + j * bufferView.byteStride);
			}
			sourceSize += bufferView.byteLength;
		}
		//Indices
		const avs::BufferView& indicesBufferView = sourceMesh.bufferViews[indices_accessor.bufferView];
		sourceSize += indicesBufferView.byteLength;
		const avs::GeometryBuffer& indicesBuffer = sourceMesh.buffers[indicesBufferView.buffer];
		size_t componentSize = avs::GetComponentSize(indices_accessor.componentType);
		size_t triangleCount= indices_accessor.count / 3;
		if(indicesBufferView.byteStride==4)
		{
			uint32_t* data=(uint32_t*)(indicesBuffer.data + indicesBufferView.byteOffset+ indices_accessor.byteOffset);
			for(size_t j=0;j< triangleCount;j++)
			{
				draco::Mesh::Face dracoMeshFace;
				for(int k=0;k<3;k++)
					dracoMeshFace[k]= data[j*3+k];
				dracoMesh.SetFace(face_index,dracoMeshFace);
				face_index++;
			}
		}
		else if (indicesBufferView.byteStride == 2)
		{
			uint16_t* data = (uint16_t*)(indicesBuffer.data + indicesBufferView.byteOffset + indices_accessor.byteOffset);
			for (size_t j = 0; j < triangleCount; j++)
			{
				draco::Mesh::Face dracoMeshFace;
				for (int k = 0; k < 3; k++)
					dracoMeshFace[k] = data[j * 3 + k];
				dracoMesh.SetFace(face_index, dracoMeshFace);
				face_index++;
			}
		}
		else
		{
			TELEPORT_ASSERT(false);
		}
		//dracoMesh.DeduplicateAttributeValues();
		//dracoMesh.DeduplicatePointIds();
		draco::Status status= dracoEncoder.EncodeMeshToBuffer(dracoMesh,&dracoEncoderBuffer);
		if(!status.ok())
		{
			TELEPORT_INTERNAL_LOG_UNSAFE("dracoEncoder failed\n");
			return false;
		}
		subMesh.buffer.resize(dracoEncoderBuffer.size());
		memcpy(subMesh.buffer.data(), dracoEncoderBuffer.data(), subMesh.buffer.size());
		compressedSize+= subMesh.buffer.size();

		std::string test_str="C:\\temp\\test";
		test_str +=char('0')+(char)i;
		test_str +=".drc";
		std::ofstream saveFile(test_str.c_str(), std::ofstream::out | std::ofstream::binary);
		saveFile.write(dracoEncoderBuffer.data(), dracoEncoderBuffer.size());
		saveFile.close();
	}
	TELEPORT_INTERNAL_LOG_UNSAFE("Compressed from %uk to %uk\n",(sourceSize+1023)/1024,(compressedSize +1023)/1024);
	compressedMesh.meshCompressionType=avs::MeshCompressionType::DRACO;
/*
	draco::Decoder dracoDecoder;
	draco::DecoderBuffer dracoDecoderBuffer;
	dracoDecoderBuffer.Init((const char*)dracoEncoderBuffer.data(), dracoEncoderBuffer.size());
	draco::Mesh dracoMesh2;
	dracoDecoder.DecodeBufferToGeometry(&dracoDecoderBuffer,&dracoMesh2);*/
	
	return true;
}

static bool FloatCompare(float a,float b)
{
	float diff=(a-b);
	float m=std::max(b,1.0f);
	float dif_rel=diff/m;
	if(dif_rel>0.1f)
		return false;
	return true;
}

static bool VecCompare(avs::vec2 a, avs::vec2 b)
{
	float A=sqrt(a.x*a.x+a.y*a.y);
	float B=sqrt(b.x*b.x+b.y*b.y);
	float m = std::max(B, 1.0f);
	avs::vec2 diff = a - b;
	float dif_rel = abs(diff.x+diff.y)/ m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool VecCompare(avs::vec3 a, avs::vec3 b)
{
	float B = sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
	float m = std::max(B, 1.0f);
	avs::vec3 diff = a - b;
	float dif_rel = abs(diff.x + diff.y + diff.z) / m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

static bool VecCompare(avs::vec4 a, avs::vec4 b)
{
	float B = sqrt(b.x * b.x + b.y * b.y + b.z * b.z + b.w * b.w);
	float m = std::max(B, 1.0f);
	avs::vec4 diff = a - b;
	float dif_rel = abs(diff.x + diff.y + diff.z + diff.w) / m;
	if (dif_rel > 0.1f)
		return false;
	return true;
}

template<int n> bool IntCompare(const int*a,const int*b)
{
	for(size_t i=0;i<n;i++)
	{
		if(a[i]!=b[i])
			return false;
	}
	return true;
}

static bool VerifyCompressedMesh(avs::CompressedMesh& compressedMesh,const avs::Mesh& sourceMesh)
{
	for(int i=0;i<compressedMesh.subMeshes.size();i++)
	{
		auto &subMesh=compressedMesh.subMeshes[i];
		draco::Decoder dracoDecoder;
		draco::DecoderBuffer dracoDecoderBuffer;
		dracoDecoderBuffer.Init((const char*)subMesh.buffer.data(), subMesh.buffer.size());
		const auto &primitiveArray=sourceMesh.primitiveArrays[i];
		auto statusor = dracoDecoder.DecodeMeshFromBuffer(&dracoDecoderBuffer);
		auto &dm=statusor.value();
		if(!dm.get())
			return false;
		// are there the same number of members for each attrib?
		size_t num_elements=0;
		for(int a=0;a<dm->num_attributes();a++)
		{
			const draco::PointAttribute *attr=dm->attribute(a);
			if(!attr)
				continue;
			if(!num_elements)
				num_elements =attr->size();
			else if(attr->size()!= num_elements)
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Attr size mismatch %ull != %ull",attr->size(), num_elements);
			}
			const uint8_t *dracoDatabuffer=attr->GetAddress(draco::AttributeValueIndex(0));
			const auto &attribute=primitiveArray.attributes[a];
			if(attribute.semantic!=subMesh.attributeSemantics[a])
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Attr semantic mismatch");
				continue;
			}
			const avs::Accessor  &attr_accessor=sourceMesh.accessors.find(attribute.accessor).operator*().second;
			const avs::BufferView& attr_bufferView=(*(sourceMesh.bufferViews.find(attr_accessor.bufferView))).second;
			const avs::GeometryBuffer &attr_buffer=(*(sourceMesh.buffers.find(attr_bufferView.buffer))).second;
			const uint8_t *src_data=attr_buffer.data+attr_bufferView.byteOffset+attr_accessor.byteOffset; 
			for (int i = 0; i < attr->size(); i++)
			{
				if(attr_accessor.componentType==avs::Accessor::ComponentType::INT)
				{
					size_t sz=(size_t)attr_accessor.type;
					const int *compare=nullptr;
					switch(sz)
					{
					case 1:
						compare = ((const int *)&(attr->GetValue<int, 1>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 2:
						compare = ((const int*)&(attr->GetValue<int, 2>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 3:
						compare = ((const int*)&(attr->GetValue<int, 3>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					case 4:
						compare = ((const int*)&(attr->GetValue<int, 4>(draco::AttributeValueIndex(i))));
						if (!IntCompare<1>(compare, (const int*)src_data))
						{
							TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
							break;
						}
					}
					src_data+=sz*sizeof(int);
				}
				else if (attr_accessor.componentType == avs::Accessor::ComponentType::FLOAT)
				{
					switch(attr_accessor.type)
					{
						case avs::Accessor::DataType::SCALAR:
						{
							float compare = attr->GetValue<float,1>(draco::AttributeValueIndex(i))[0];
							if(!FloatCompare(compare,*((const float*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC2:
						{
							auto c= attr->GetValue<float, 2>(draco::AttributeValueIndex(i));
							avs::vec2 compare = *((const avs::vec2*)&c);
							if (!VecCompare(compare,*((const avs::vec2*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC3:
						{
							auto c = attr->GetValue<float, 3>(draco::AttributeValueIndex(i));
							avs::vec3 compare = *((const avs::vec3*)&c);
							if (!VecCompare(compare, *((const avs::vec3*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						case avs::Accessor::DataType::VEC4:
						{
							auto c = attr->GetValue<float, 4>(draco::AttributeValueIndex(i));
							avs::vec4 compare = *((const avs::vec4*)&c);
							if (!VecCompare(compare, *((const avs::vec4*)src_data)))
							{
								TELEPORT_INTERNAL_LOG_UNSAFE("Verify failed");
								break;
							}
						}
						break;
						default:
							break;
					}
				}
			}
		}
		return true;
		draco::Mesh &dracoMesh=*dm;
		uint32_t max_index = 0;
		uint32_t max_value = 0;
		uint32_t unset = 0xffffffff;
		// Go through each unique point and see whether the "mapped indices" are the same for the attributes:
		for(uint32_t idx=0;idx<dracoMesh.num_points();idx++)
		{
			max_index = std::max(max_index, idx);
			//TELEPORT_INTERNAL_LOG_UNSAFE("Point %u:",idx);
			uint32_t index = unset;
			bool mismatch=false;
			for (int a = 0; a < dm->num_attributes(); a++)
			{
				const draco::PointAttribute* attr = dm->attribute(a);
				uint32_t val = attr->mapped_index(draco::PointIndex(idx)).value();
				if(a)
				{
				//	TELEPORT_INTERNAL_LOG_UNSAFE(",");
				}
			//	TELEPORT_INTERNAL_LOG_UNSAFE(" %u", val);
				max_value = std::max(max_value, val);
				if (index == unset)
					index = val;
				else if (val != index)
				{
					mismatch=true;
				}
			}
			if(mismatch)
			{
				TELEPORT_INTERNAL_LOG_UNSAFE("Mismatch");
			}
			TELEPORT_INTERNAL_LOG_UNSAFE("\n");
		}
	}
	return true;
}

void GeometryStore::storeMesh(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Mesh& newMesh, avs::AxesStandard standard, bool compress,bool verify)
{
	auto &mesh=meshes[standard][id] = ExtractedMesh{guid, lastModified, newMesh};
	if(compress)
	{
		CompressMesh(mesh.compressedMesh,mesh.mesh);
		if(verify)
		{
			//Save data to new file.
			{
				std::wofstream saveFile("verify.mesh", std::wofstream::out | std::wofstream::binary);
				saveFile << mesh << std::endl;
				saveFile.close();
			}
			std::wifstream loadFile("verify.mesh", std::wifstream::in | std::wifstream::binary);
			//avs::uid oldID;
			ExtractedMesh testMesh;
			loadFile >> testMesh;
			VerifyCompressedMesh(testMesh.compressedMesh, testMesh.mesh);
		}
	}
}

void GeometryStore::storeMaterial(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Material& newMaterial)
{
	materials[id] = ExtractedMaterial{guid, lastModified, newMaterial};
}

void GeometryStore::storeTexture(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& newTexture, std::string basisFileLocation, bool genMips
	, bool highQualityUASTC,bool forceOverwrite)
{
	//Compress the texture with Basis Universal if the file location is not blank, and bytes per pixel is equal to 4.
	if(!basisFileLocation.empty() && newTexture.bytesPerPixel == 4)
	{
		bool validBasisFileExists = false;
		filesystem::path filePath = basisFileLocation;
		if(filesystem::exists(filePath))
		{
			//Read last write time.
			filesystem::file_time_type rawBasisTime = filesystem::last_write_time(filePath);

			//Convert to std::time_t; imprecise, but good enough.
			std::time_t basisLastModified = rawBasisTime.time_since_epoch().count();

			//The file is valid if the basis file is younger than the texture file.
			validBasisFileExists = basisLastModified >= lastModified;
		}
		//Read from disk if the file exists.
		if(highQualityUASTC||((!forceOverwrite)&&validBasisFileExists))
		{
			std::ifstream basisReader(basisFileLocation, std::ifstream::in | std::ifstream::binary);

			newTexture.dataSize = static_cast<uint32_t>(filesystem::file_size(filePath));
			newTexture.data = new unsigned char[newTexture.dataSize];
			basisReader.read(reinterpret_cast<char*>(newTexture.data), newTexture.dataSize);

			basisReader.close();
		}
		//Otherwise, queue the texture for compression.
		else 	// UASTC doesn't work from inside the dll. Unclear why.
		{
			//Copy data from source, so it isn't lost.
			unsigned char* dataCopy = new unsigned char[newTexture.dataSize];
			memcpy(dataCopy, newTexture.data, newTexture.dataSize);
			newTexture.data = dataCopy;
			texturesToCompress.emplace(id, PrecompressedTexture{basisFileLocation, newTexture.data, newTexture.dataSize, newTexture.mipCount, genMips, highQualityUASTC});
		}
	}
	else
	{
		newTexture.compression = avs::TextureCompression::UNCOMPRESSED;

		unsigned char* dataCopy = new unsigned char[newTexture.dataSize];
		memcpy(dataCopy, newTexture.data, newTexture.dataSize);
		newTexture.data = dataCopy;

		if(newTexture.dataSize > 1048576)
		{
			TELEPORT_WARN << "Texture \"" << newTexture.name << "\" was stored UNCOMPRESSED with a data size larger than 1MB! Size: " << newTexture.dataSize << "B(" << newTexture.dataSize / 1048576.0f << "MB).\n";
		}
	}

	textures[id] = ExtractedTexture{guid, lastModified, newTexture };
}

void GeometryStore::storeShadowMap(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& newShadowMap)
{
	shadowMaps[id] = ExtractedTexture{guid, lastModified, newShadowMap};
}

void GeometryStore::removeNode(avs::uid id)
{
	nodes.erase(id);
	lightNodes.erase(id);
}

void GeometryStore::updateNode(avs::uid id, avs::Transform& newTransform)
{
	auto nodeIt = nodes.find(id);
	if(nodeIt == nodes.end())
		return;

	nodeIt->second.transform = newTransform;
}

size_t GeometryStore::getAmountOfTexturesWaitingForCompression() const
{
	return texturesToCompress.size();
}

const avs::Texture* GeometryStore::getNextCompressedTexture() const
{
	//No textures to compress.
	if(texturesToCompress.size() == 0)
		return nullptr;
	auto compressionPair = texturesToCompress.begin();
	auto foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	return &foundTexture->second.texture;
}

void GeometryStore::compressNextTexture()
{
	//No textures to compress.
	if(texturesToCompress.size() == 0)
		return;
	auto compressionPair = texturesToCompress.begin();
	auto& foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	avs::Texture& newTexture = foundTexture->second.texture;
	PrecompressedTexture& compressionData = compressionPair->second;

	basisu::image image(newTexture.width, newTexture.height);
	basisu::color_rgba_vec& imageData = image.get_pixels();
	memcpy(imageData.data(), compressionData.rawData, compressionData.dataSize);


	basisu::basis_compressor_params basisCompressorParams; //Parameters for basis compressor.
	basisCompressorParams.m_source_images.clear();
	basisCompressorParams.m_source_images.push_back(image);

	basisCompressorParams.m_quality_level = compressionQuality;
	basisCompressorParams.m_compression_level = compressionStrength;

	basisCompressorParams.m_write_output_basis_files = true;
	basisCompressorParams.m_out_filename = compressionData.basisFilePath;
	basisCompressorParams.m_uastc=compressionData.highQualityUASTC;

	uint32_t THREAD_AMOUNT = 32;
	if(compressionData.highQualityUASTC)
	{
		THREAD_AMOUNT = 1;
		// Write this to a different filename, it's just for testing.
		auto ext_pos = basisCompressorParams.m_out_filename.find(".basis");
		basisCompressorParams.m_out_filename = basisCompressorParams.m_out_filename.substr(0, ext_pos) + "-dll.basis";
		return;

		// we want the equivalent of:
		// -uastc -uastc_rdo_m -no_multithreading -debug -stats -output_path "outputPath" "srcPng"
		basisCompressorParams.m_rdo_uastc_multithreading = false;
		basisCompressorParams.m_multithreading=false;
		//basisCompressorParams.m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;//= basist::KTX2_SS_ZSTANDARD;

		int uastc_level = std::clamp<int>(4, 0, 4);

		//static const uint32_t s_level_flags[5] = { basisu::cPackUASTCLevelFastest, basisu::cPackUASTCLevelFaster, basisu::cPackUASTCLevelDefault, basisu::cPackUASTCLevelSlower, basisu::cPackUASTCLevelVerySlow };

		//basisCompressorParams.m_pack_uastc_flags &= ~basisu::cPackUASTCLevelMask;
		//basisCompressorParams.m_pack_uastc_flags |= s_level_flags[uastc_level];

		//basisCompressorParams.m_rdo_uastc_dict_size = 32768;
		//basisCompressorParams.m_check_for_alpha=true;
		basisCompressorParams.m_debug=true;
		basisCompressorParams.m_status_output=true;
		basisCompressorParams.m_compute_stats = true;
		//basisCompressorParams.m_perceptual=true;
		//basisCompressorParams.m_validate=false;
		basisCompressorParams.m_mip_srgb=true;
		basisCompressorParams.m_quality_level = 128;
	}
	else
	{
		basisCompressorParams.m_mip_gen = compressionData.genMips;
		basisCompressorParams.m_mip_smallest_dimension = 4; // ???
		basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexType2D;
	}
	if(!basisCompressorParams.m_pJob_pool)
	{
		basisCompressorParams.m_pJob_pool = new basisu::job_pool(THREAD_AMOUNT);
	}
	basisu::basis_compressor basisCompressor;
	basisu::enable_debug_printf(true);
	
	bool ok=basisCompressor.init(basisCompressorParams);
	if(ok)
	{
		basisu::basis_compressor::error_code result = basisCompressor.process();

		if(result == basisu::basis_compressor::error_code::cECSuccess)
		{
			basisu::uint8_vec basisTex = basisCompressor.get_output_basis_file();

			delete[] newTexture.data;

			newTexture.dataSize = basisCompressor.get_basis_file_size();
			newTexture.data = new unsigned char[newTexture.dataSize];
			memcpy(newTexture.data, basisTex.data(), newTexture.dataSize);
		}
		else
		{
			TELEPORT_CERR << "Failed to compress texture \"" << newTexture.name << "\"!\n";
		}
	}
	else
	{
		TELEPORT_CERR << "Failed to compress texture \"" << newTexture.name << "\"! Basis Universal compressor failed to initialise.\n";
	}

	texturesToCompress.erase(texturesToCompress.begin());
	delete basisCompressorParams.m_pJob_pool;
	basisCompressorParams.m_pJob_pool = nullptr;
}

template<typename ExtractedResource> void GeometryStore::saveResources(const std::string path, const std::map<avs::uid, ExtractedResource>& resourceMap) const
{
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	for(const auto& resourceData : resourceMap)
	{
		std::string file_name=path;
		file_name+="/";
		file_name+=resourceData.second.getName()+"_";
		file_name +=resourceData.second.guid;
		file_name+=resourceData.second.fileExtension();
		bool oldFileExists = filesystem::exists(file_name);

		//Rename old file.
		if (oldFileExists)
			filesystem::rename(file_name, file_name + ".bak");

		//Save data to new file.
		std::wofstream resourceFile(file_name, std::wofstream::out | std::wofstream::binary);
		resourceFile << resourceData.first << std::endl << resourceData.second << std::endl;
		resourceFile.close();

		//Delete old file.
		if (oldFileExists)
			filesystem::remove(file_name + ".bak");
	}
}

template<typename ExtractedResource> void GeometryStore::loadResources(const std::string path, std::map<avs::uid, ExtractedResource>& resourceMap)
{
	//Load resources if the file exists.
	const std::filesystem::path fspath{ path.c_str() };
	std::filesystem::create_directories(fspath);
	std::string search_str=ExtractedResource::fileExtension();
	std::map<avs::uid, std::filesystem::file_time_type> timestamps;
	for (auto const& dir_entry : std::filesystem::directory_iterator{ fspath })
	{
		std::string file_name=dir_entry.path().string();
		if(file_name.find(search_str)<file_name.length())
		if(filesystem::exists(file_name))
		{
			std::wifstream resourceFile(file_name, std::wifstream::in | std::wifstream::binary);
			
			avs::uid oldID;
			//Load resources from the file, while there is still more data in the file.
			if(resourceFile >> oldID)
			{
				auto write_time= std::filesystem::last_write_time(file_name);
				// If there's a duplicate, use the newer file.
				bool use_new=true;
				if(resourceMap.find(oldID)==resourceMap.end())
				{
					// if new file timestamp is older than the last one, don't use it.
					if(write_time<timestamps[oldID])
						use_new=false;
				}
				if(use_new)
				{
					ExtractedResource& newResource = resourceMap[oldID];
					resourceFile >> newResource;
					timestamps[oldID]= write_time;
				}
				else
				{
					ExtractedResource newResource;
					resourceFile >> newResource;
				}
			}
		}
	}
}
