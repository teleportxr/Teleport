#pragma once

#include <ctime>
#include <unordered_map>
#include <vector>

#include "basisu_comp.h"
#include "libavstream/geometry/mesh_interface.hpp"

#include "ExtractedTypes.h"

namespace teleport
{
	/// Singleton for storing geometry data and managing the geometry file cache.
	class GeometryStore: public avs::GeometrySourceBackendInterface
	{
	public:
		GeometryStore();
		~GeometryStore();

		bool willDelayTextureCompression = true; //Causes textures to wait for compression in StoreTexture, rather than calling compress them during the function call, when true.

		void saveToDisk() const;
		//Load from disk.
		//Parameters are used to return the meta data of the resources that were loaded back-in, so they can be confirmed.
		void loadFromDisk(size_t& meshAmount, LoadedResource*& loadedMeshes, size_t& textureAmount, LoadedResource*& loadedTextures, size_t& materialAmount, LoadedResource*& loadedMaterials);
		//Inform GeometryStore of the resources that still exist, and of their new IDs.
		void reaffirmResources(int32_t meshAmount, ReaffirmedResource* reaffirmedMeshes, int32_t textureAmount, ReaffirmedResource* reaffirmedTextures, int32_t materialAmount, ReaffirmedResource* reaffirmedMaterials);

		void clear(bool freeMeshBuffers);

		void setCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality);
		
		const char* getNodeName(avs::uid nodeID) const override;

		virtual std::vector<avs::uid> getNodeIDs() const override;
		virtual avs::Node* getNode(avs::uid nodeID) override;
		virtual const avs::Node* getNode(avs::uid nodeID) const override;
		virtual const std::map<avs::uid, avs::Node>& getNodes() const override;

		virtual avs::Skin* getSkin(avs::uid skinID, avs::AxesStandard standard) override;
		virtual const avs::Skin* getSkin(avs::uid skinID, avs::AxesStandard standard) const override;

		virtual avs::Animation* getAnimation(avs::uid id, avs::AxesStandard standard) override;
		virtual const avs::Animation* getAnimation(avs::uid id, avs::AxesStandard standard) const override;

		virtual std::vector<avs::uid> getMeshIDs() const override;
		const avs::CompressedMesh* getCompressedMesh(avs::uid meshID, avs::AxesStandard standard) const override;
		virtual avs::Mesh* getMesh(avs::uid meshID, avs::AxesStandard standard) override;
		virtual const avs::Mesh* getMesh(avs::uid meshID, avs::AxesStandard standard) const override;

		virtual std::vector<avs::uid> getTextureIDs() const override;
		virtual avs::Texture* getTexture(avs::uid textureID) override;
		virtual const avs::Texture* getTexture(avs::uid textureID) const override;

		virtual std::vector<avs::uid> getMaterialIDs() const override;
		virtual avs::Material* getMaterial(avs::uid materialID) override;
		virtual const avs::Material* getMaterial(avs::uid materialID) const override;

		virtual std::vector<avs::uid> getShadowMapIDs() const override;
		virtual avs::Texture* getShadowMap(avs::uid shadowID) override;
		virtual const avs::Texture* getShadowMap(avs::uid shadowID) const override;

		//Returns a list of all light nodes that need to be streamed to the client.
		const std::map<avs::uid,avs::LightNodeResources>& getLightNodes() const;

		//Returns whether there is a node stored with the passed id.
		bool hasNode(avs::uid id) const;
		//Returns whether there is a mesh stored with the passed id.
		bool hasMesh(avs::uid id) const;
		//Returns whether there is a material stored with the passed id.
		bool hasMaterial(avs::uid id) const;
		//Returns whether there is a texture stored with the passed id.
		bool hasTexture(avs::uid id) const;
		//Returns whether there is a shadow map stored with the passed id.
		bool hasShadowMap(avs::uid id) const;

		void storeNode(avs::uid id, avs::Node& newNode);
		void storeSkin(avs::uid id, avs::Skin& newSkin, avs::AxesStandard sourceStandard);
		void storeAnimation(avs::uid id, avs::Animation& animation, avs::AxesStandard sourceStandard);
		void storeMesh(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Mesh& newMesh, avs::AxesStandard standard,bool compress=false,bool verify=false);
		void storeMaterial(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Material& newMaterial);
		void storeTexture(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& newTexture, std::string basisFileLocation,  bool genMips, bool highQualityUASTC,bool forceOverwrite);
		void storeShadowMap(avs::uid id, _bstr_t guid, std::time_t lastModified, avs::Texture& shadowMap);

		void removeNode(avs::uid id);

		void updateNode(avs::uid id, avs::Transform& newTransform);

		//Returns amount of textures waiting to be compressed.
		size_t getAmountOfTexturesWaitingForCompression() const;
		//Returns the texture that will be compressed next.
		const avs::Texture* getNextCompressedTexture() const;
		//Compresses the next texture to be compressed; does nothing if there are no more textures to compress.
		void compressNextTexture();
		/// Set the global cache path for the project.
		void SetCachePath(const char *path)
		{
			cachePath=path;
		}
	private:
		std::string cachePath;
		//Stores data on a texture that is to be compressed.
		struct PrecompressedTexture
		{
			std::string basisFilePath;

			uint8_t* rawData;
			size_t dataSize;

			size_t numMips;	
			bool genMips;	// if false, numMips tells how many are in the data already.
			bool highQualityUASTC;
		};

		//Names of the files that store each resource type.
		const std::string TEXTURE_CACHE_PATH		= "textures";
		const std::string MATERIAL_CACHE_PATH		= "materials";
		const std::string MESH_PC_CACHE_PATH		= "meshes/engineering";
		const std::string MESH_ANDROID_CACHE_PATH	= "meshes/gl";
		uint8_t compressionStrength = 1;
		uint8_t compressionQuality = 1;

		std::map<avs::uid, avs::Node> nodes;
		std::map<avs::AxesStandard, std::map<avs::uid, avs::Skin>> skins;
		std::map<avs::AxesStandard, std::map<avs::uid, avs::Animation>> animations;
		std::map<avs::AxesStandard, std::map<avs::uid, ExtractedMesh>> meshes;
		std::map<avs::uid, ExtractedMaterial> materials;
		std::map<avs::uid, ExtractedTexture> textures;
		std::map<avs::uid, ExtractedTexture> shadowMaps;

		std::map<avs::uid, PrecompressedTexture> texturesToCompress; //Map of textures that need compressing. <ID of the texture; file path to store the basis file>

		std::map<avs::uid, avs::LightNodeResources> lightNodes; //List of ALL light nodes; prevents having to search for them every geometry tick.

		template<typename ExtractedResource>
		void saveResources(const std::string file_name, const std::map<avs::uid, ExtractedResource>& resourceMap) const;

		template<typename ExtractedResource>
		void loadResources(const std::string file_name, std::map<avs::uid, ExtractedResource>& resourceMap);

		avs::uid GuidToUid(avs::guid g) const;
		avs::guid UidToGuid(avs::uid u) const;

		std::map<avs::uid,avs::guid> guids;
		std::map<avs::guid,avs::uid> uids;
	};
}