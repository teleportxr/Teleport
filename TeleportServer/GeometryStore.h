#pragma once
#include <ctime>
#include <unordered_map>
#include <vector>

#include "libavstream/geometry/mesh_interface.hpp"
#include "TeleportCore/Animation.h"
#include "TeleportCore/TextCanvas.h"
#include "TeleportCore/InputTypes.h"
#include "TeleportServer/Export.h"
#include "ExtractedTypes.h"
struct InteropTextCanvas;

namespace teleport
{
	namespace core
	{
		struct TextCanvas;
	}
	namespace server
	{
		//! Resource state replication. Each resource is either not present or present.
		//! Additionally, we track whether the server is aware of that state.
		struct ResourceReplication
		{
		};

		struct TextureToCompress
		{
			std::string name;
			int width=0;
			int height=0;
		};
		//! Singleton for storing geometry data and managing the geometry file cache.
		//! The definitive geometry store is the file structure pointed to by SetCachePath().
		//! The structure in  GeometryStore is the *session* structure, using uid's for quick reference.
		class TELEPORT_SERVER_API GeometryStore final
		{
		public:
			GeometryStore();
			virtual ~GeometryStore();

			static GeometryStore& GetInstance();
			bool willDelayTextureCompression = true; //Causes textures to wait for compression in StoreTexture, rather than calling compress them during the function call, when true.

			//Checks and sets the global cache path for the project. Returns true if path is valid.
			bool SetCachePath(const char* path);
			std::string GetCachePath() const
			{
				return cachePath;
			}
			/// Sets the root http address this server will use to serve files via http.
			bool SetHttpRoot(const char*);
			std::string GetHttpRoot() const
			{
				return httpRoot;
			}
			bool IsHttpEnabled() const
			{
				return httpRoot.length()>0;
			}
			//Load from disk.
			//Parameters are used to return the meta data of the resources that were loaded back-in, so they can be confirmed.
			void loadFromDisk(size_t& meshCount, LoadedResource*& loadedMeshes, size_t& textureCount, LoadedResource*& loadedTextures, size_t& materialCount, LoadedResource*& loadedMaterials);

			void clear(bool freeMeshBuffers);

			void setCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality);

			const char* getNodeName(avs::uid nodeID) const;

			std::vector<avs::uid> getNodeIDs() const;
			avs::Node* getOrCreateNode(avs::uid nodeId);
			avs::Node* getNode(avs::uid nodeID);
			const avs::Node* getNode(avs::uid nodeID) const;

			avs::Skeleton* getSkeleton(avs::uid skeletonID, avs::AxesStandard standard);
			const avs::Skeleton* getSkeleton(avs::uid skeletonID, avs::AxesStandard standard) const;

			teleport::core::Animation* getAnimation(avs::uid id, avs::AxesStandard standard);
			const teleport::core::Animation* getAnimation(avs::uid id, avs::AxesStandard standard) const;

			std::vector<avs::uid> getMeshIDs() const;

			const ExtractedMesh* getExtractedMesh(avs::uid meshID, avs::AxesStandard standard) const;

			const avs::CompressedMesh* getCompressedMesh(avs::uid meshID, avs::AxesStandard standard) const;
			virtual avs::Mesh* getMesh(avs::uid meshID, avs::AxesStandard standard);
			virtual const avs::Mesh* getMesh(avs::uid meshID, avs::AxesStandard standard) const;

			virtual std::vector<avs::uid> getTextureIDs() const;
			ExtractedTexture* getWrappedTexture(avs::uid textureID);
			virtual avs::Texture* getTexture(avs::uid textureID);
			virtual const avs::Texture* getTexture(avs::uid textureID) const;

			virtual std::vector<avs::uid> getMaterialIDs() const;
			virtual avs::Material* getMaterial(avs::uid materialID);
			virtual const avs::Material* getMaterial(avs::uid materialID) const;

			virtual std::vector<avs::uid> getShadowMapIDs() const;
			virtual avs::Texture* getShadowMap(avs::uid shadowID);
			virtual const avs::Texture* getShadowMap(avs::uid shadowID) const;

			const core::TextCanvas* getTextCanvas(avs::uid u) const;
			const core::FontAtlas* getFontAtlas(avs::uid u) const;

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

			void setNodeParent(avs::uid id, avs::uid parent_id, teleport::core::Pose relPose);
			void storeSkeleton(avs::uid id, avs::Skeleton& newSkeleton, avs::AxesStandard sourceStandard);
			bool storeAnimation(avs::uid id, const std::string &path, teleport::core::Animation &animation, avs::AxesStandard sourceStandard);
			bool storeMesh(avs::uid id, const std::string & path, std::time_t lastModified, const InteropMesh *iMesh, avs::AxesStandard standard, bool verify=false);
			bool storeMesh(avs::uid id, const std::string & path, std::time_t lastModified, const avs::Mesh &newMesh, avs::AxesStandard standard, bool verify = false);
			bool storeMaterial(avs::uid id,  const std::string & path, std::time_t lastModified, avs::Material& newMaterial);
			bool storeTexture(avs::uid id,  const std::string & path, std::time_t lastModified, const avs::Texture &newTexture, bool genMips, bool highQualityUASTC, bool forceOverwrite);
			avs::uid storeFont(const std::string & ttf_path_utf8, const std::string & relative_asset_path_utf8, std::time_t lastModified, int size = 32);
			avs::uid storeTextCanvas(const std::string & relative_asset_path, const InteropTextCanvas* interopTextCanvas);
			void storeShadowMap(avs::uid id,  const std::string & path, std::time_t lastModified, avs::Texture& shadowMap);

			void removeNode(avs::uid id);

			void updateNodeTransform(avs::uid id, avs::Transform& newLTransform);

			//Returns amount of textures waiting to be compressed.
			size_t getNumberOfTexturesWaitingForCompression() const;
			//Returns the texture that will be compressed next.
			TextureToCompress getNextTextureToCompress() const;
			//Compresses the next texture to be compressed; does nothing if there are no more textures to compress.
			void compressNextTexture();

			/// Check for errors - these should be resolved before using this store in a server.
			bool CheckForErrors(avs::uid uid=0) ;
			//! Get or generate a uid. If the path already corresponds to an id, that will be returned. Otherwise a new one will be added.
			avs::uid GetOrGenerateUid(const std::string& path);
			//! Generate a uid. Used mainly for nodes, which don't have persistent paths.
			avs::uid GenerateUid();
			//! Get the current session uid corresponding to the given resource/asset path.
			avs::uid PathToUid(std::string p) const;
			//! Get the resource/asset path corresponding to the current session uid.
			std::string UidToPath(avs::uid u) const;
			//! Load the resource if possible from the file cache into memory.
			bool EnsureResourceIsLoaded(avs::uid u);
			template <typename ExtractedResource>
			bool saveResource(const std::string file_name, const ExtractedResource &resource) const;
			template <typename ExtractedResource>
			bool loadResourceFromPath(const std::string &resource_path, const std::string &filepath_root, ExtractedResource &esource);
			template <typename ExtractedResource>
			bool loadResourceBinary(const std::string file_name, const std::string &path_root, ExtractedResource &esource);
			template <typename ExtractedResource>
			avs::uid loadResourceBinary(const std::string file_name, const std::string &path_root, std::map<avs::uid, ExtractedResource> &resourceMap);

			template <typename ExtractedResource>
			bool saveResourcesBinary(const std::string file_name, const std::map<avs::uid, ExtractedResource> &resourceMap) const;

			template <typename ExtractedResource>
			void loadResourcesBinary(std::string file_name, std::map<avs::uid, ExtractedResource> &resourceMap);

		private:
			std::string cachePath;
			std::string httpRoot;

			uint8_t compressionStrength = 1;
			uint8_t compressionQuality = 1;

			// Mutable, non-resource assets.
			std::map<avs::uid, std::unique_ptr<avs::Node>> nodes;
			std::map<avs::uid, core::TextCanvas> textCanvases;

			// Static, resource assets.
			std::map<avs::AxesStandard, std::map<avs::uid, avs::Skeleton>> skeletons;
			std::map<avs::AxesStandard, std::map<avs::uid, teleport::core::Animation>> animations;
			std::map<avs::AxesStandard, std::map<avs::uid, ExtractedMesh>> meshes;
			std::map<avs::uid, ExtractedMaterial> materials;
			std::map<avs::uid, ExtractedTexture> textures;
			std::map<avs::uid, ExtractedTexture> shadowMaps;
			std::map<avs::uid, ExtractedFontAtlas> fontAtlases;

			std::map<avs::uid, std::shared_ptr<PrecompressedTexture>> texturesToCompress; //Map of textures that need compressing. <ID of the texture; file path to store the basis file>

			std::map<avs::uid, std::string> uid_to_path;
			std::map<std::string, avs::uid> path_to_uid;
			bool LoadResourceAtPath(std::string p, avs::uid u);
			avs::uid LoadResourceFromFile(std::string p, avs::uid u);
			
			void CompressTexturesAsync();
			std::thread compressTexturesThread;
			bool kill=false;
		};
	}
}