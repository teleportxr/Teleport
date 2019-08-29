// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <libavstream/mesh.hpp>
#include <libavstream/geometry/mesh_interface.hpp>

#include "API.h"
#include "ResourceManager.h"
#include "ActorManager.h"
#include "api/RenderPlatform.h"

#include "transcoder/basisu_transcoder.h"

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
        ResourceManagers() :
                mIndexBufferManager(&scr::IndexBuffer::Destroy), mShaderManager(nullptr),
                mMaterialManager(nullptr), mTextureManager(&scr::Texture::Destroy),
                mUniformBufferManager(&scr::UniformBuffer::Destroy),
                mVertexBufferManager(&scr::VertexBuffer::Destroy)
        {
        }

        ~ResourceManagers()
        {
        }

        void Update(uint32_t timeElapsed)
        {
            mIndexBufferManager.Update(timeElapsed);
            mShaderManager.Update(timeElapsed);
            mMaterialManager.Update(timeElapsed);
            mTextureManager.Update(timeElapsed);
            mUniformBufferManager.Update(timeElapsed);
            mVertexBufferManager.Update(timeElapsed);
        }

        scr::ActorManager                                    mActorManager;
        ResourceManager<scr::IndexBuffer>   mIndexBufferManager;
        ResourceManager<scr::Shader>        mShaderManager;
        ResourceManager<scr::Material>		mMaterialManager;
        ResourceManager<scr::Texture>       mTextureManager;
        ResourceManager<scr::UniformBuffer> mUniformBufferManager;
        ResourceManager<scr::VertexBuffer>  mVertexBufferManager;
    };
}

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.*/
class ResourceCreator final : public avs::GeometryTargetBackendInterface
{
public:
	ResourceCreator();
	~ResourceCreator();
	
	void SetRenderPlatform(scr::RenderPlatform *r);

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
		ResourceManager<scr::VertexBuffer> *vertexBufferManager)
	{
		m_IndexBufferManager = indexBufferManager;
		m_ShaderManager = shaderManager;
		m_MaterialManager = materialManager;
		m_TextureManager = textureManager;
		m_UniformBufferManager = uniformBufferManager;
		m_VertexBufferManager = vertexBufferManager;
	}

private:
	// Inherited via GeometryTargetBackendInterface
	avs::Result Assemble(avs::ResourceCreate *) override;

	//Material and Texture
	void passTexture(avs::uid texture_uid, const avs::Texture& texture) override;
	void passMaterial(avs::uid material_uid, const avs::Material& material) override;

	//Transforms
	void passNode(avs::uid node_uid, avs::DataNode& node) override;

	//Actor
	void CreateActor(avs::uid mesh_uid, const std::vector<avs::uid>& material_uids, avs::uid transform_uid) override;


#define CHECK_SHAPE_UID(x) if (!SetAndCheckShapeUID(x)) { SCR_CERR("Invalid shape_uid.\n"); return; }

public:
	ResourceManager<scr::Texture>* GetTextureManager()
	{
		return m_TextureManager;
	}

private:
	scr::API m_API;
	scr::RenderPlatform* m_pRenderPlatform;

	basist::etc1_global_selector_codebook basis_codeBook;
	basist::transcoder_texture_format basis_textureFormat;
	
	uint32_t m_PostUseLifetime = 30000; //30,000ms = 30s
	ResourceManager<scr::IndexBuffer> *m_IndexBufferManager;
	ResourceManager<scr::Material> *m_MaterialManager;
	ResourceManager<scr::Shader> *m_ShaderManager;
	ResourceManager<scr::Texture> *m_TextureManager;
	ResourceManager<scr::UniformBuffer> *m_UniformBufferManager;
	ResourceManager<scr::VertexBuffer> *m_VertexBufferManager;

	scr::ActorManager* m_pActorManager;

	static std::vector<std::pair<avs::uid, avs::uid>> m_MeshMaterialUIDPairs;

	std::map<avs::uid, std::shared_ptr<avs::DataNode>> nodes;
};

