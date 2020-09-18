// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <thread>
#include <mutex>
#include <set>

#include "transcoder/basisu_transcoder.h"

#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/mesh.hpp>

#include "ActorManager.h"
#include "API.h"
#include "api/RenderPlatform.h"
#include "Light.h"
#include "ResourceManager.h"

namespace scr
{
	class Material;
}

namespace basist
{
    class etc1_global_selector_codebook;
    class basisu_transcoder;
    enum transcoder_texture_format;
}

namespace scr
{
    struct ResourceManagers
    {
        ResourceManagers(scr::ActorManager* actorManager)
            :mActorManager(actorManager),
            mIndexBufferManager(&scr::IndexBuffer::Destroy), mShaderManager(nullptr),
            mMaterialManager(nullptr), mTextureManager(&scr::Texture::Destroy),
            mUniformBufferManager(&scr::UniformBuffer::Destroy),
            mVertexBufferManager(&scr::VertexBuffer::Destroy),
			mMeshManager(nullptr),
			mLightManager(nullptr)
        {
        }

        ~ResourceManagers()
        {
        }

		//Clear any resources that have not been used longer than their expiry time.
		//	timeElapsed : Delta time in milliseconds.
		void Update(float timeElapsed)
        {
			mActorManager->Update(timeElapsed);
            mIndexBufferManager.Update(timeElapsed);
            mShaderManager.Update(timeElapsed);
            mMaterialManager.Update(timeElapsed);
            mTextureManager.Update(timeElapsed);
            mUniformBufferManager.Update(timeElapsed);
            mVertexBufferManager.Update(timeElapsed);
			mMeshManager.Update(timeElapsed);
			//mLightManager.Update(timeElapsed);
        }


		std::vector<uid> GetAllResourceIDs()
		{
			std::vector<uid> resourceIDs;
			
			mMaterialManager.GetAllIDs(resourceIDs);
			mTextureManager.GetAllIDs(resourceIDs);
			mMeshManager.GetAllIDs(resourceIDs);
			mLightManager.GetAllIDs(resourceIDs);

			return resourceIDs;

			/*
				//We will resend the actors/objects to update the transform data, as changes in client position (and thus the new invisible actors) aren't stored for the reconnect.
				mActorManager;

				//These IDs aren't stored on the server currently, and thus are ignored.
				mIndexBufferManager.GetAllIDs();
				mShaderManager.GetAllIDs();
				mUniformBufferManager.GetAllIDs();
				mVertexBufferManager.GetAllIDs();
			*/
		}

		//Clear all resources.
		void Clear()
		{
			mActorManager->Clear();

			mIndexBufferManager.Clear();
			mShaderManager.Clear();
			mMaterialManager.Clear();
			mTextureManager.Clear();
			mUniformBufferManager.Clear();
			mVertexBufferManager.Clear();
			mMeshManager.Clear(); 
			mLightManager.Clear();
		}

		//Clear all resources that aren't in the exclude list.
		//	excludeList : List of resources that should be spared from clearing of resource managers.
		//	outExistingActors : List of actors in the excludeList that existed on the client.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingActors)
		{
			mMaterialManager.ClearCareful(excludeList);
			mTextureManager.ClearCareful(excludeList);
			mMeshManager.ClearCareful(excludeList);
			mLightManager.ClearCareful(excludeList);

			//Last as it will likely be the largest.
			mActorManager->ClearCareful(excludeList, outExistingActors);

			///As the UIDs of these aren't(?) stored on the server; the server can't confirm their existence.
			///If the mesh is cleared, then these will be cleared.
			//mIndexBufferManager.ClearCareful(excludeList);
			//mShaderManager.ClearCareful(excludeList);
			//mUniformBufferManager.ClearCareful(excludeList);
			//mVertexBufferManager.ClearCareful(excludeList);
		}

        std::unique_ptr<scr::ActorManager>	mActorManager;
        ResourceManager<scr::IndexBuffer>   mIndexBufferManager;
        ResourceManager<scr::Shader>        mShaderManager;
        ResourceManager<scr::Material>		mMaterialManager;
        ResourceManager<scr::Texture>       mTextureManager;
        ResourceManager<scr::UniformBuffer> mUniformBufferManager;
        ResourceManager<scr::VertexBuffer>  mVertexBufferManager;
		ResourceManager<scr::Mesh>			mMeshManager;
		ResourceManager<scr::Light>			mLightManager;
    };
}

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.*/
class ResourceCreator final : public avs::GeometryTargetBackendInterface
{
public:
	ResourceCreator(basist::transcoder_texture_format transcoderTextureFormat);
	~ResourceCreator();
	
	void Initialise(scr::RenderPlatform *r, scr::VertexBufferLayout::PackingStyle packingStyle);
	//Returns the resources the ResourceCreator needs, and clears the list.
	std::vector<avs::uid> TakeResourceRequests();
	//Returns a list of resource IDs corresponding to the resources the client has received, and clears the list.
	std::vector<avs::uid> TakeReceivedResources();
	//Returns the actors that have been finished since the call, and clears the list.
	std::vector<avs::uid> TakeCompletedActors();

	//Updates any processes that need to happen on a regular basis; should be called at least once per second.
	//	deltaTime : Milliseconds that has passed since the last call to Update();
	void Update(float deltaTime);

	inline void AssociateActorManager(scr::ActorManager* actorManager)
	{
		m_pActorManager = actorManager;
	}

	inline void AssociateResourceManagers(
		ResourceManager<scr::IndexBuffer> *indexBufferManager,
		ResourceManager<scr::Shader> *shaderManager,
		ResourceManager<scr::Material> *materialManager,
		ResourceManager<scr::Texture> *textureManager,
		ResourceManager<scr::UniformBuffer> *uniformBufferManager,
		ResourceManager<scr::VertexBuffer> *vertexBufferManager,
		ResourceManager<scr::Mesh> *meshManager,
		ResourceManager<scr::Light> *lightManager)
	{
		m_IndexBufferManager = indexBufferManager;
		m_ShaderManager = shaderManager;
		m_MaterialManager = materialManager;
		m_TextureManager = textureManager;
		m_UniformBufferManager = uniformBufferManager;
		m_VertexBufferManager = vertexBufferManager;
		m_MeshManager = meshManager;
		m_LightManager = lightManager;
	}

	// Inherited via GeometryTargetBackendInterface
	avs::Result Assemble(const avs::MeshCreate& meshCreate) override;

	void CreateTexture(avs::uid texture_uid, const avs::Texture& texture) override;
	void CreateMaterial(avs::uid material_uid, const avs::Material& material) override;
	void CreateNode(avs::uid node_uid, avs::DataNode& node) override;

	std::shared_ptr<scr::Texture> m_DummyDiffuse;
	std::shared_ptr<scr::Texture> m_DummyNormal;
	std::shared_ptr<scr::Texture> m_DummyCombined;
	std::shared_ptr<scr::Texture> m_DummyEmissive;
	
	struct IncompleteResource
	{
		avs::uid id;
	};
	
	struct MissingResource
	{
		std::vector<std::shared_ptr<IncompleteResource>> incompleteResources;
	};
	
	std::unordered_map<avs::uid, MissingResource> &GetMissingResources()
	{
		return m_WaitingForResources;
	}
private:
	struct IncompleteMaterial: IncompleteResource
	{
		scr::Material::MaterialCreateInfo materialInfo;
		std::unordered_map<avs::uid, std::shared_ptr<scr::Texture>&> textureSlots; // <ID of the texture, slot the texture should be placed into.
	};

	struct IncompleteActor : IncompleteResource
	{
		scr::Actor::ActorCreateInfo actorInfo;
		std::unordered_map<avs::uid, std::vector<size_t>> materialSlots; // <ID of the material, list of indexes the material should be placed into actor material list.
		bool isHand;
	};

	struct UntranscodedTexture
	{
		avs::uid texture_uid;
		uint32_t dataSize; //Size of the basis file.
		unsigned char* data; //The raw data of the basis file.
		scr::Texture::TextureCreateInfo scrTexture; //Creation information on texture being transcoded.

		std::string name; //For debugging which texture failed.
	};
	
	void CreateActor(avs::uid node_uid, avs::DataNode& node, bool isHand);
	void CreateLight(avs::uid node_uid, avs::DataNode& node);

	void CompleteMesh(avs::uid mesh_uid, const scr::Mesh::MeshCreateInfo& meshInfo);
	void CompleteTexture(avs::uid texture_uid, const scr::Texture::TextureCreateInfo& textureInfo);
	void CompleteMaterial(avs::uid material_uid, const scr::Material::MaterialCreateInfo& materialInfo);
	void CompleteActor(avs::uid node_uid, const scr::Actor::ActorCreateInfo& actorInfo, bool isHand);

	//Add texture to material being created.
	//	accessor : Data on texture that was received from server.
	//	colourFactor : Vector factor to multiply texture with to adjust strength.
	//	dummyTexture : Texture to use if there is no texture ID assigned.
	//	materialParameter : Material's data for this texture.
	//	textureSlots : Mapping list of texture IDs to the texture slot(e.g. diffuse texture).
	//	missingResources : Set containing IDs of textures that the client doesn't have.
	void AddTextureToMaterial(const avs::TextureAccessor& accessor,
							  const avs::vec4& colourFactor,
							  const std::shared_ptr<scr::Texture>& dummyTexture,
							  scr::Material::MaterialParameter& materialParameter,
							  std::unordered_map<avs::uid, std::shared_ptr<scr::Texture>&>& textureSlots,
							  std::set<avs::uid>& missingResources) const;

	scr::API m_API;
	const scr::RenderPlatform* m_pRenderPlatform = nullptr;
	scr::VertexBufferLayout::PackingStyle m_PackingStyle;

	basist::etc1_global_selector_codebook basis_codeBook;
	basist::transcoder_texture_format basis_textureFormat;

	std::vector<UntranscodedTexture> texturesToTranscode;
	std::map<avs::uid, scr::Texture::TextureCreateInfo> texturesToCreate; //Textures that are ready to be created <Texture's UID, Texture's Data>
	
	std::mutex mutex_texturesToTranscode;
	std::mutex mutex_texturesToCreate;
	bool shouldBeTranscoding = true; //Whether the basis thread should be running, and transcoding textures. Settings this to false causes the thread to end.
	std::thread basisThread; //Thread where we transcode basis files to mip data.
	
	const uint32_t diffuseBGRA = 0xFFFFFFFF;
	//const uint32_t normalBGRA = 0xFF7F7FFF;
	const uint32_t normalRGBA = 0xFFFF7F7F;
	const uint32_t combinedBGRA = 0xFFFFFFFF;
	const uint32_t emissiveBGRA = 0x00000000;
	
//s	uint32_t m_PostUseLifetime = 1000; //30,000ms = 30s
	ResourceManager<scr::IndexBuffer> *m_IndexBufferManager;
	ResourceManager<scr::Material> *m_MaterialManager;
	ResourceManager<scr::Shader> *m_ShaderManager;
	ResourceManager<scr::Texture> *m_TextureManager;
	ResourceManager<scr::UniformBuffer> *m_UniformBufferManager;
	ResourceManager<scr::VertexBuffer> *m_VertexBufferManager;
	ResourceManager<scr::Mesh> *m_MeshManager;
	ResourceManager<scr::Light> *m_LightManager;

	scr::ActorManager* m_pActorManager;

	std::vector<avs::uid> m_ResourceRequests; //Resources the client will request from the server.
	std::vector<avs::uid> m_ReceivedResources; //Resources the client will confirm receival of.
	std::vector<avs::uid> m_CompletedActors; //List of IDs of actors that have been fully received, and have yet to be confirmed to the server.
	std::unordered_map<avs::uid, MissingResource> m_WaitingForResources; //<ID of Missing Resource, List Of Things Waiting For Resource>

	void BasisThread_TranscodeTextures();
};

