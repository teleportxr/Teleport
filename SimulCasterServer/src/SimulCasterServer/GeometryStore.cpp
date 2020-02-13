#include "GeometryStore.h"

#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace SCServer;

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

template<class T>
T* getResource(std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	//Assuming an incorrect resource should not happen, or at least not frequently.
	try
	{
		return &resourceMap.at(id);
	}
	catch(std::out_of_range oor)
	{
		return nullptr;
	}
}

//Const version.
template<class T>
const T* getResource(const std::map<avs::uid, T>& resourceMap, avs::uid id)
{
	//Assuming an incorrect resource should not happen, or at least not frequently.
	try
	{
		return &resourceMap.at(id);
	}
	catch(std::out_of_range oor)
	{
		return nullptr;
	}
}

GeometryStore::GeometryStore()
{
	basisCompressorParams.m_tex_type = basist::basis_texture_type::cBASISTexType2D;

	const uint32_t THREAD_AMOUNT = 16;
	basisCompressorParams.m_pJob_pool = new basisu::job_pool(THREAD_AMOUNT);

	basisCompressorParams.m_quality_level = 1;
	basisCompressorParams.m_compression_level = 1;
}

GeometryStore::~GeometryStore()
{
	delete basisCompressorParams.m_pJob_pool;
}

void GeometryStore::clear(bool freeMeshBuffers)
{
	//Free memory for primitive attributes and geometry buffers.
	for(auto& idMeshPair : meshes)
	{
		for(avs::PrimitiveArray& primitive : idMeshPair.second.primitiveArrays)
		{
			delete[] primitive.attributes;
		}

		//Unreal just uses the pointer, but Unity copies them on the native side.
		if(freeMeshBuffers)
		{
			for(auto& idBufferPair : idMeshPair.second.buffers)
			{
				delete[] idBufferPair.second.data;
			}
		}
	}
	
	//Free memory for texture pixel data.
	for(auto& idTexturePair : textures)
	{
		delete[] idTexturePair.second.data;
	}

	//Free memory for shadow map pixel data.
	for(auto& idShadowPair : shadowMaps)
	{
		delete[] idShadowPair.second.data;
	}

	nodes.clear();
	meshes.clear();
	materials.clear();
	textures.clear();
	shadowMaps.clear();

	texturesToCompress.clear();
	lightNodes.clear();
	hands.clear();
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

const avs::DataNode* GeometryStore::getNode(avs::uid nodeID) const
{
	return getResource(nodes, nodeID);
}

const std::map<avs::uid, avs::DataNode>& GeometryStore::getNodes() const
{
	return nodes;
}

std::vector<avs::uid> GeometryStore::getMeshIDs() const
{
	return getVectorOfIDs(meshes);
}

avs::Mesh* GeometryStore::getMesh(avs::uid meshID)
{
	return getResource(meshes, meshID);
}

const avs::Mesh* GeometryStore::getMesh(avs::uid meshID) const
{
	return getResource(meshes, meshID);
}

std::vector<avs::uid> GeometryStore::getTextureIDs() const
{
	return getVectorOfIDs(textures);
}

avs::Texture* GeometryStore::getTexture(avs::uid textureID)
{
	return getResource(textures, textureID);
}

const avs::Texture* GeometryStore::getTexture(avs::uid textureID) const
{
	return getResource(textures, textureID);
}

std::vector<avs::uid> GeometryStore::getMaterialIDs() const
{
	return getVectorOfIDs(materials);
}

avs::Material* GeometryStore::getMaterial(avs::uid materialID)
{
	return getResource(materials, materialID);
}

const avs::Material* GeometryStore::getMaterial(avs::uid materialID) const
{
	return getResource(materials, materialID);
}

std::vector<avs::uid> GeometryStore::getShadowMapIDs() const
{
	return getVectorOfIDs(shadowMaps);
}

avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID)
{
	return getResource(shadowMaps, shadowID);
}

const avs::Texture* GeometryStore::getShadowMap(avs::uid shadowID) const
{
	return getResource(shadowMaps, shadowID);
}

const std::vector<avs::LightNodeResources>& GeometryStore::getLightNodes() const
{
	return lightNodes;
}

const std::vector<std::pair<void*, avs::uid>>& GeometryStore::getHands() const
{
	return hands;
}

void GeometryStore::setHands(std::pair<void*, avs::uid> firstHand, std::pair<void*, avs::uid> secondHand)
{
	hands = {firstHand, secondHand};
}

bool GeometryStore::hasNode(avs::uid id) const
{
	return nodes.find(id) != nodes.end();
}

bool GeometryStore::hasMesh(avs::uid id) const
{
	return meshes.find(id) != meshes.end();
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

void GeometryStore::storeNode(avs::uid id, avs::DataNode&& newNode)
{
	nodes[id] = newNode;

	if(newNode.data_type == avs::NodeDataType::ShadowMap) lightNodes.emplace_back(avs::LightNodeResources{id, newNode.data_uid});
}

void GeometryStore::storeMesh(avs::uid id, avs::Mesh&& newMesh)
{
	meshes[id] = newMesh;
}

void GeometryStore::storeMaterial(avs::uid id, avs::Material&& newMaterial)
{
	materials[id] = newMaterial;
}
#if defined ( _WIN32 )
#include <sys/stat.h>
#endif

std::time_t GetFileWriteTime ( const std::filesystem::path& filename )
{
    #if defined ( _WIN32 )
    {
        struct _stat64 fileInfo;
        if ( _wstati64 ( filename.wstring ().c_str (), &fileInfo ) != 0 )
        {
            throw std::runtime_error ( "Failed to get last write time." );
        }
        return fileInfo.st_mtime;
    }
    #else
    {
        auto fsTime = std::filesystem::last_write_time ( filename );
        return decltype ( fsTime )::clock::to_time_t ( fsTime );
    }
    #endif
}
void GeometryStore::storeTexture(avs::uid id, avs::Texture&& newTexture, std::time_t lastModified, std::string basisFileLocation)
{
	//We need to use the experimental namespace if we are using MSVC 2017, but not for 2019+.
#if _MSC_VER < 1920
	namespace filesystem = std::experimental::filesystem;
#else
	namespace filesystem = std::filesystem;
#endif

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
			std::cout << "Texture \"" << newTexture.name << "\" was stored UNCOMPRESSED with a data size larger than 1MB! Size: " << newTexture.dataSize << "B(" << newTexture.dataSize / 1048576.0f << "MB).\n";
		}
	}

	textures[id] = newTexture;
}

void GeometryStore::storeShadowMap(avs::uid id, avs::Texture&& newShadowMap)
{
	shadowMaps[id] = newShadowMap;
}

void SCServer::GeometryStore::removeNode(avs::uid id)
{
	nodes.erase(id);
}

size_t GeometryStore::getAmountOfTexturesWaitingForCompression() const
{
	return texturesToCompress.size();
}

const avs::Texture* GeometryStore::getNextCompressedTexture() const
{
	//No textures to compress.
	if(texturesToCompress.size() == 0) return nullptr;

	auto compressionPair = texturesToCompress.begin();
	auto foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	return &foundTexture->second;
}

void GeometryStore::compressNextTexture()
{
	//No textures to compress.
	if(texturesToCompress.size() == 0) return;

	auto compressionPair = texturesToCompress.begin();
	auto& foundTexture = textures.find(compressionPair->first);
	assert(foundTexture != textures.end());

	avs::Texture& newTexture = foundTexture->second;
	PrecompressedTexture& compressionData = compressionPair->second;

	basisu::image image(newTexture.width, newTexture.height);
	basisu::color_rgba_vec& imageData = image.get_pixels();
	memcpy(imageData.data(), compressionData.rawData, compressionData.dataSize);

	basisCompressorParams.m_source_images.clear();
	basisCompressorParams.m_source_images.push_back(image);

	basisCompressorParams.m_write_output_basis_files = true;
	basisCompressorParams.m_out_filename = compressionData.basisFilePath;

	basisCompressorParams.m_mip_gen = true;
	basisCompressorParams.m_mip_smallest_dimension = 4; //Appears to be the smallest texture size that SimulFX handles.

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
			std::cout << "Failed to compress texture: " << newTexture.name << ".\n";
		}
	}
	else
	{
		std::cout << "Failed to initialise basis compressor for texture: " << newTexture.name << ".\n";
	}

	texturesToCompress.erase(texturesToCompress.begin());
}
