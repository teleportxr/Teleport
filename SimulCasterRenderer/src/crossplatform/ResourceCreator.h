// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include "API.h"
#include "ResourceManager.h"
#include "ActorManager.h"
#include "Light.h"
#include "api/RenderPlatform.h"

#include "transcoder/basisu_transcoder.h"

//Multithreading
#include <thread>
#include <mutex>

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
        ResourceManagers()
            :mIndexBufferManager(&scr::IndexBuffer::Destroy), mShaderManager(nullptr),
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

        void Update(uint32_t timeElapsed)
        {
			mActorManager.Update(timeElapsed);
            mIndexBufferManager.Update(timeElapsed);
            mShaderManager.Update(timeElapsed);
            mMaterialManager.Update(timeElapsed);
            mTextureManager.Update(timeElapsed);
            mUniformBufferManager.Update(timeElapsed);
            mVertexBufferManager.Update(timeElapsed);
			mMeshManager.Update(timeElapsed);
			//mLightManager.Update(timeElapsed);
        }

		void Clear()
		{
			mActorManager.Clear();

			mIndexBufferManager.Clear();
			mShaderManager.Clear();
			mMaterialManager.Clear();
			mTextureManager.Clear();
			mUniformBufferManager.Clear();
			mVertexBufferManager.Clear();
			mMeshManager.Clear(); 
			mLightManager.Clear();
		}

        scr::ActorManager  					mActorManager;
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

	//Updates any processes that need to happen on a regular basis; should be called at least once per second.
	//	deltaTime : Milliseconds that has passed since the last call to Update();
	void Update(uint32_t deltaTime);

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
	avs::Result Assemble(avs::MeshCreate*) override;

	void passTexture(avs::uid texture_uid, const avs::Texture& texture) override;
	void passMaterial(avs::uid material_uid, const avs::Material& material) override;

	void passNode(avs::uid node_uid, avs::DataNode& node) override;

	std::shared_ptr<scr::Texture> m_DummyDiffuse;
	std::shared_ptr<scr::Texture> m_DummyNormal;
	std::shared_ptr<scr::Texture> m_DummyCombined;
private:
	struct IncompleteResource
	{
		avs::uid id;
	};

	struct IncompleteMaterial: IncompleteResource
	{
		scr::Material::MaterialCreateInfo materialInfo;
		std::unordered_map<avs::uid, std::shared_ptr<scr::Texture>&> textureSlots; // <ID of the texture, slot the texture should be placed into.
	};

	struct IncompleteActor : IncompleteResource
	{
		scr::Actor::ActorCreateInfo actorInfo;
		std::unordered_map<avs::uid, std::vector<size_t>> materialSlots; // <ID of the material, list of indexes the material should be placed into actor material list.
	};

	struct UntranscodedTexture
	{
		avs::uid texture_uid;
		uint32_t dataSize; //Size of the basis file.
		unsigned char* data; //The raw data of the basis file.
		scr::Texture::TextureCreateInfo scrTexture; //Creation information on texture being transcoded.

		std::string name; //For debugging which texture failed.
	};

	void CreateActor(avs::uid node_uid, avs::uid mesh_uid, const std::vector<avs::uid>& material_uids, const avs::Transform &transform) override;
	void CreateLight(avs::uid node_uid, avs::DataNode& node);

	void CompleteMesh(avs::uid mesh_uid, const scr::Mesh::MeshCreateInfo& meshInfo);
	void CompleteTexture(avs::uid texture_uid, const scr::Texture::TextureCreateInfo& textureInfo);
	void CompleteMaterial(avs::uid material_uid, const scr::Material::MaterialCreateInfo& materialInfo);
	void CompleteActor(avs::uid node_uid, const scr::Actor::ActorCreateInfo& actorInfo);

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
	const uint32_t normalBGRA = 0xFF7F7FFF;
	const uint32_t combinedBGRA = 0xFFFFFFFF;
	
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

	std::vector<avs::uid> m_ResourceRequests;
	std::unordered_map<avs::uid, std::vector<std::shared_ptr<IncompleteResource>>> m_WaitingForResources; //<ID of Missing Resource, Thing Waiting For Resource>

	void BasisThread_TranscodeTextures();
};

