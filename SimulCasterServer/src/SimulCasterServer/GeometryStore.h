#pragma once

#include <vector>
#include <unordered_map>
#include <ctime>

#include "libavstream/geometry/mesh_interface.hpp"

#include "basisu_comp.h"

namespace SCServer
{
	class GeometryStore: public avs::GeometrySourceBackendInterface
	{
	public:
		GeometryStore();
		~GeometryStore();

		bool willDelayTextureCompression = true; //Causes textures to wait for compression in StoreTexture, rather than calling compress them during the function call, when true.

		void clear(bool freeMeshBuffers);

		void setCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality);

		virtual std::vector<avs::uid> getNodeIDs() const override;
		virtual avs::DataNode* getNode(avs::uid nodeID) override;
		virtual const avs::DataNode* getNode(avs::uid nodeID) const override;
		virtual const std::map<avs::uid, avs::DataNode>& getNodes() const override;

		virtual std::vector<avs::uid> getMeshIDs() const override;
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
		const std::vector<avs::LightNodeResources>& getLightNodes() const;

		//Returns a list of pointer-ID pairs belonging to the nodes that represent the player's hands.
		const std::vector<std::pair<void*, avs::uid>>& getHands() const;
		//Set pointer-ID pairs for each hand.
		void setHands(std::pair<void*, avs::uid> firstHand, std::pair<void*, avs::uid> secondHand);

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

		void storeNode(avs::uid id, avs::DataNode&& node);
		void storeMesh(avs::uid id, avs::AxesStandard standard, avs::Mesh&& mesh);
		void storeMaterial(avs::uid id, avs::Material&& material);
		void storeTexture(avs::uid id, avs::Texture&& texture, std::time_t lastModified, std::string basisFileLocation, bool swapRedBlueChannels);
		void storeShadowMap(avs::uid id, avs::Texture&& shadowMap);

		void removeNode(avs::uid id);

		//Returns amount of textures waiting to be compressed.
		size_t getAmountOfTexturesWaitingForCompression() const;
		//Returns the texture that will be compressed next.
		const avs::Texture* getNextCompressedTexture() const;
		//Compresses the next texture to be compressed; does nothing if there are no more textures to compress.
		void compressNextTexture();
	private:
		//Stores data on a texture that is to be compressed.
		struct PrecompressedTexture
		{
			std::string basisFilePath;

			uint8_t* rawData;
			size_t dataSize;
		};

		basisu::basis_compressor_params basisCompressorParams; //Parameters for basis compressor.

		std::map<avs::uid, avs::DataNode> nodes;
		std::map<avs::uid, std::map<avs::AxesStandard, avs::Mesh>> meshes;
		std::map<avs::uid, avs::Material> materials;
		std::map<avs::uid, avs::Texture> textures;
		std::map<avs::uid, avs::Texture> shadowMaps;

		std::map<avs::uid, PrecompressedTexture> texturesToCompress; //Map of textures that need compressing. <ID of the texture; file path to store the basis file>

		std::vector<avs::LightNodeResources> lightNodes; //List of all light nodes; prevents having to search for them every geometry tick.

		std::vector<std::pair<void*, avs::uid>> hands; //List of pointer-IDs pairs of the nodes that represent the hands that are in use.

		//Swaps the red and blue channels of the passed texture; expects four bytes per pixel (one byte per channel), RGBA.
		//	originalData : Pixel data in BGRA/RGBA format and four bytes per pixel.
		//	dataSize : Byte size of the passed data.
		void swapTextureRedBlue(unsigned char* textureData, uint32_t dataSize);
	};
}