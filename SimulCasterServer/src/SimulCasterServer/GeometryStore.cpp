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

using namespace SCServer;

GeometryStore::GeometryStore()
{
	basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexType2D;

	const uint32_t THREAD_AMOUNT = 16;
	basisCompressorParams.m_pJob_pool = new basisu::job_pool(THREAD_AMOUNT);

	basisCompressorParams.m_quality_level = 1;
	basisCompressorParams.m_compression_level = 1;

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
	delete basisCompressorParams.m_pJob_pool;
}

void GeometryStore::saveToDisk() const
{
	saveResources(TEXTURE_FILE_NAME, textures);
	saveResources(MATERIAL_FILE_NAME, materials);
	saveResources(MESH_PC_FILE_NAME, meshes.at(avs::AxesStandard::EngineeringStyle));
	saveResources(MESH_ANDROID_FILE_NAME, meshes.at(avs::AxesStandard::GlStyle));
}

void GeometryStore::loadFromDisk(size_t& meshAmount, LoadedResource*& loadedMeshes, size_t& textureAmount, LoadedResource*& loadedTextures, size_t& materialAmount, LoadedResource*& loadedMaterials)
{
	loadResources(MESH_PC_FILE_NAME, meshes.at(avs::AxesStandard::EngineeringStyle));
	loadResources(MESH_ANDROID_FILE_NAME, meshes.at(avs::AxesStandard::GlStyle));
	loadResources(TEXTURE_FILE_NAME, textures);
	loadResources(MATERIAL_FILE_NAME, materials);

	meshAmount = meshes.at(avs::AxesStandard::EngineeringStyle).size();
	textureAmount = textures.size();
	materialAmount = materials.size();

	int i = 0;
	loadedMeshes = new LoadedResource[meshAmount];
	for(auto& meshDataPair : meshes.at(avs::AxesStandard::EngineeringStyle))
	{
		loadedMeshes[i] = LoadedResource(meshDataPair.first, meshDataPair.second.guid, meshDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedTextures = new LoadedResource[textureAmount];
	for(auto& textureDataPair : textures)
	{
		loadedTextures[i] = LoadedResource(textureDataPair.first, textureDataPair.second.guid, textureDataPair.second.lastModified);

		++i;
	}

	i = 0;
	loadedMaterials = new LoadedResource[materialAmount];
	for(auto& materialDataPair : materials)
	{
		loadedMaterials[i] = LoadedResource(materialDataPair.first, materialDataPair.second.guid, materialDataPair.second.lastModified);

		++i;
	}
}
void GeometryStore::reaffirmResources(int32_t meshAmount, ReaffirmedResource* reaffirmedMeshes, int32_t textureAmount, ReaffirmedResource* reaffirmedTextures, int32_t materialAmount, ReaffirmedResource* reaffirmedMaterials)
{
	TELEPORT_COUT<<"Reaffirming resources"<<std::endl;
	//Copy data on the resources that were loaded.
	std::map<avs::AxesStandard, std::map<avs::uid, ExtractedMesh>> oldMeshes = meshes;
	std::map<avs::uid, ExtractedMaterial> oldMaterials = materials;
	std::map<avs::uid, ExtractedTexture> oldTextures = textures;

	//Delete the old data; we don't want to use the GeometryStore::clear(...) function as that will call delete on the pointers we want to copy.
	meshes.clear();
	TELEPORT_COUT<<"Had "<<materials.size()<<" materials."<<std::endl;
	TELEPORT_COUT<<"MaterialAmount is "<<materialAmount<<"."<<std::endl;
	materials.clear();
	textures.clear();

	//Ensure these exist even if there were no meshes to load.
	meshes[avs::AxesStandard::EngineeringStyle];
	meshes[avs::AxesStandard::GlStyle];

	//Replace old IDs with their new IDs; fixing any links that need to be changed.

	//ASSUMPTION: Mesh sub-resources(e.g. index buffers) aren't shared, and as such it doesn't matter that their IDs aren't re-assigned.
	for(auto meshMapPair : oldMeshes)
	{
		avs::AxesStandard mapStandard = meshMapPair.first;
		for(int i = 0; i < meshAmount; i++)
		{
			meshes[mapStandard][reaffirmedMeshes[i].newID] = oldMeshes[mapStandard][reaffirmedMeshes[i].oldID];
		}
	}

	//Lookup to find the new ID from the old ID. For replacing texture IDs in materials. <Old ID, New ID>
	std::map<avs::uid, avs::uid> textureIDLookup;
	for(int i = 0; i < textureAmount; i++)
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

	for(int i = 0; i < materialAmount; i++)
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

	nodes.clear();
	skins.clear();
	animations.clear();
	meshes.clear();
	materials.clear();
	textures.clear();
	shadowMaps.clear();

	texturesToCompress.clear();
	lightNodes.clear();

	//Recreate look-up maps.
	meshes[avs::AxesStandard::EngineeringStyle];
	meshes[avs::AxesStandard::GlStyle];
	animations[avs::AxesStandard::EngineeringStyle];
	animations[avs::AxesStandard::GlStyle];
	skins[avs::AxesStandard::EngineeringStyle];
	skins[avs::AxesStandard::GlStyle];
}

void GeometryStore::setCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
{
	basisCompressorParams.m_quality_level = compressionStrength;
	basisCompressorParams.m_compression_level = compressionQuality;
}

std::vector<avs::uid> GeometryStore::getNodeIDs() const
{
	return getVectorOfIDs(nodes);
}

avs::DataNode* GeometryStore::getNode(avs::uid nodeID)
{
	return getResource(nodes, nodeID);
}
const char *GeometryStore::getNodeName(avs::uid nodeID) const
{
	const char *name="";
	const avs::DataNode* n=getNode(nodeID);
	if(n)
	{
			name=n->name.c_str();
	}
	return name;
}
const avs::DataNode* GeometryStore::getNode(avs::uid nodeID) const
{
	return getResource(nodes, nodeID);
}

const std::map<avs::uid, avs::DataNode>& GeometryStore::getNodes() const
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

const std::vector<avs::LightNodeResources>& GeometryStore::getLightNodes() const
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

const char *stringOf(avs::NodeDataType t)
{
	switch(t)
	{
		case avs::NodeDataType::Mesh:
			return "Mesh";
		break;
		case avs::NodeDataType::Camera:
		case avs::NodeDataType::Scene:
			return "Scene";
		break;
		case avs::NodeDataType::ShadowMap:
		case avs::NodeDataType::Light:
			return "Light";
		break;
		case avs::NodeDataType::Bone:
			return "Bone";
		break;
		default:
		break;
	};

	return "";
}
template<typename T,typename tr> std::basic_ostream<T,tr> & operator << (std::basic_ostream<T,tr> &out, const avs::NodeDataType &c) 
{ 
    out << (int)c; 
    return out; 
} 
void GeometryStore::storeNode(avs::uid id, avs::DataNode& newNode)
{
//	TELEPORT_COUT<<"storeNode "<<newNode.name.c_str()<<" uid "<<id<<", type "<<stringOf(newNode.data_type)<<", data uid "<<(int)newNode.data_uid<<std::endl;
	nodes[id] = newNode;

	if(newNode.data_type == avs::NodeDataType::ShadowMap|| newNode.data_type == avs::NodeDataType::Light)
		lightNodes.emplace_back(avs::LightNodeResources{id, newNode.data_uid});
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

void GeometryStore::storeMesh(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Mesh& newMesh, avs::AxesStandard standard)
{
	meshes[standard][id] = ExtractedMesh{guid, lastModified, newMesh};
}

void GeometryStore::storeMaterial(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Material& newMaterial)
{
	materials[id] = ExtractedMaterial{guid, lastModified, newMaterial};
}

void GeometryStore::storeTexture(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& newTexture, std::string basisFileLocation)
{
	//Compress the texture with Basis Universal if the file location is not blank, and bytes per pixel is equal to 4.
	if(!basisFileLocation.empty() && newTexture.bytesPerPixel == 4)
	{
		newTexture.compression = avs::TextureCompression::BASIS_COMPRESSED;

		bool validBasisFileExists = false;
		filesystem::path filePath = basisFileLocation;
		if(filesystem::exists(filePath))
		{
			//Read last write time.
			filesystem::file_time_type rawBasisTime = filesystem::last_write_time(filePath);

			//Convert to std::time_t; imprecise, but good enough.
			//std::time_t basisLastModified = GetFileWriteTime(filePath);//std::chrono::system_clock::to_time_t(rawBasisTime);
			std::time_t basisLastModified = rawBasisTime.time_since_epoch().count();

			//The file is valid if the basis file is younger than the texture file.
			validBasisFileExists = basisLastModified >= lastModified;
		}

		//Read from disk if the file exists.
		if(validBasisFileExists)
		{
			std::ifstream basisReader(basisFileLocation, std::ifstream::in | std::ifstream::binary);

			newTexture.dataSize = static_cast<uint32_t>(filesystem::file_size(filePath));
			newTexture.data = new unsigned char[newTexture.dataSize];
			basisReader.read(reinterpret_cast<char*>(newTexture.data), newTexture.dataSize);

			basisReader.close();
		}
		//Otherwise, queue the texture for compression.
		else
		{
			//Copy data from source, so it isn't lost.
			unsigned char* dataCopy = new unsigned char[newTexture.dataSize];
			memcpy(dataCopy, newTexture.data, newTexture.dataSize);
			newTexture.data = dataCopy;

			texturesToCompress.emplace(id, PrecompressedTexture{basisFileLocation, newTexture.data, newTexture.dataSize});
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

	textures[id] = ExtractedTexture{guid, lastModified, newTexture};
}

void GeometryStore::storeShadowMap(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& newShadowMap)
{
	shadowMaps[id] = ExtractedTexture{guid, lastModified, newShadowMap};
}

void GeometryStore::removeNode(avs::uid id)
{
	nodes.erase(id);
}

void GeometryStore::updateNode(avs::uid id, avs::Transform& newTransform)
{
	auto nodeIt = nodes.find(id);
	if(nodeIt == nodes.end()) return;

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

	basisCompressorParams.m_source_images.clear();
	basisCompressorParams.m_source_images.push_back(image);

	basisCompressorParams.m_write_output_basis_files = true;
	basisCompressorParams.m_out_filename = compressionData.basisFilePath;

	basisCompressorParams.m_mip_gen = true;
	basisCompressorParams.m_mip_smallest_dimension = 4; // ???
	
	static bool use_uastc=false;
	// use UASTC, which is better for normals.
	basisCompressorParams.m_uastc=use_uastc;

	basisu::basis_compressor basisCompressor;

	if(basisCompressor.init(basisCompressorParams))
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
}

template<typename ExtractedResource>
void GeometryStore::saveResources(const std::string file_name, const std::map<avs::uid, ExtractedResource>& resourceMap) const
{
	bool oldFileExists = filesystem::exists(file_name);

	//Rename old file.
	if(oldFileExists) filesystem::rename(file_name, file_name + ".bak");

	//Save data to new file.
	std::wofstream resourceFile(file_name, std::wofstream::out | std::wofstream::binary);
	for(const auto& resourceData : resourceMap)
	{
		//Don't save to disk if the last modified time is zero, as this means it is a default asset and can't be identified.
		if(resourceData.second.lastModified != 0)
		{
			resourceFile << resourceData.first << std::endl << resourceData.second << std::endl;
		}
	}
	resourceFile.close();

	//Delete old file.
	if(oldFileExists) filesystem::remove(file_name + ".bak");
}

template<typename ExtractedResource>
void GeometryStore::loadResources(const std::string file_name, std::map<avs::uid, ExtractedResource>& resourceMap)
{
	//Load resources if the file exists.
	if(filesystem::exists(file_name))
	{
		std::wifstream resourceFile(file_name, std::wifstream::in | std::wifstream::binary);

		avs::uid oldID;
		//Load resources from the file, while there is still more data in the file.
		while(resourceFile >> oldID)
		{
			ExtractedResource& newResource = resourceMap[oldID];
			resourceFile >> newResource;
		}
	}
}
