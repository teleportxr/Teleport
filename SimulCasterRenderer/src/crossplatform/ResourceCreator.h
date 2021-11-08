// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <thread>

#include "transcoder/basisu_transcoder.h"

#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/mesh.hpp>

#include "API.h"
#include "api/RenderPlatform.h"
#include "Light.h"
#include "MemoryUtil.h"
#include "NodeManager.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "GeometryCache.h"

namespace scr
{
	class Animation;
	class Material;
}

namespace scr
{
}

/*! A class to receive geometry stream instructions and create meshes. It will then manage them for rendering and destroy them when done.*/
class ResourceCreator final : public avs::GeometryTargetBackendInterface
{
public:
	struct IncompleteResource
	{
		IncompleteResource(avs::uid id, avs::GeometryPayloadType type)
			:id(id), type(type)
		{}

		const avs::uid id;
		const avs::GeometryPayloadType type;
	};

	struct MissingResource
	{
		const avs::uid id; //ID of the missing resource.
		const char* resourceType; //String indicating missing resource's type.

		std::vector<std::shared_ptr<IncompleteResource>> waitingResources; //Resources that can't be completed without this missing resource.

		MissingResource(avs::uid id, const char* resourceType)
			:id(id), resourceType(resourceType)
		{}
	};

	ResourceCreator();
	~ResourceCreator();
	
	void Initialise(scr::RenderPlatform *r, scr::VertexBufferLayout::PackingStyle packingStyle);

	//Returns the resources the ResourceCreator needs, and clears the list.
	std::vector<avs::uid> TakeResourceRequests();
	//Returns a list of resource IDs corresponding to the resources the client has received, and clears the list.
	std::vector<avs::uid> TakeReceivedResources();
	//Returns the nodes that have been finished since the call, and clears the list.
	std::vector<avs::uid> TakeCompletedNodes();

	void Clear();

	//Updates any processes that need to happen on a regular basis; should be called at least once per second.
	//	deltaTime : Milliseconds that has passed since the last call to Update();
	void Update(float deltaTime);

	inline void AssociateResourceManagers(scr::GeometryCache& resourceManagers)
	{
		m_IndexBufferManager = &resourceManagers.mIndexBufferManager;
		m_ShaderManager = &resourceManagers.mShaderManager;
		m_MaterialManager = &resourceManagers.mMaterialManager;
		m_TextureManager = &resourceManagers.mTextureManager;
		m_UniformBufferManager = &resourceManagers.mUniformBufferManager;
		m_VertexBufferManager = &resourceManagers.mVertexBufferManager;
		m_MeshManager = &resourceManagers.mMeshManager;
		m_SkinManager = &resourceManagers.mSkinManager;
		m_LightManager = &resourceManagers.mLightManager;
		m_BoneManager = &resourceManagers.mBoneManager;
		m_AnimationManager = &resourceManagers.mAnimationManager;
		m_pNodeManager = resourceManagers.mNodeManager.get();
	}

	// Inherited via GeometryTargetBackendInterface
	avs::Result Assemble(avs::MeshCreate& meshCreate) override;

	void CreateTexture(avs::uid id, const avs::Texture& texture) override;
	void CreateMaterial(avs::uid id, const avs::Material& material) override;
	void CreateNode(avs::uid id, avs::DataNode& node) override;
	void CreateSkin(avs::uid id, avs::Skin& skin) override;
	void CreateAnimation(avs::uid id, avs::Animation& animation) override;

	std::shared_ptr<scr::Texture> m_DummyWhite;
	std::shared_ptr<scr::Texture> m_DummyNormal;
	std::shared_ptr<scr::Texture> m_DummyCombined;
	std::shared_ptr<scr::Texture> m_DummyBlack;
	std::shared_ptr<scr::Texture> m_DummyGreen;

	std::unordered_map<avs::uid, MissingResource>& GetMissingResources()
	{
		return m_MissingResources;
	}
private:
	struct IncompleteMaterial: IncompleteResource
	{
		IncompleteMaterial(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		scr::Material::MaterialCreateInfo materialInfo;
		std::unordered_map<avs::uid, std::shared_ptr<scr::Texture>&> textureSlots; //<ID of the texture, slot the texture should be placed into>.
	};

	struct IncompleteNode : IncompleteResource
	{
		IncompleteNode(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		std::shared_ptr<scr::Node> node;

		std::unordered_map<avs::uid, std::vector<size_t>> materialSlots; //<ID of the material, list of indexes the material should be placed into node material list>.
		std::unordered_map<avs::uid, size_t> missingAnimations; //<ID of missing animation, index in animation vector>
	};

	struct IncompleteSkin : IncompleteResource
	{
		IncompleteSkin(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		std::shared_ptr<scr::Skin> skin;

		std::unordered_map<avs::uid, size_t> missingBones; //<ID of missing bone, index in vector>
	};

	struct UntranscodedTexture
	{
		avs::uid texture_uid;
		std::vector<unsigned char> data; //The raw data of the basis file.
		scr::Texture::TextureCreateInfo scrTexture; //Creation information on texture being transcoded.
		std::string name; //For debugging which texture failed.
		avs::TextureCompression fromCompressionFormat;
		float valueScale;	// scale on transcode.
	};
	
	void CreateMeshNode(avs::uid id, avs::DataNode& node);
	void CreateLight(avs::uid id, avs::DataNode& node);
	void CreateBone(avs::uid id, avs::DataNode& node);

	void CompleteMesh(avs::uid id, const scr::Mesh::MeshCreateInfo& meshInfo);
	void CompleteSkin(avs::uid id, std::shared_ptr<IncompleteSkin> completeSkin);
	void CompleteTexture(avs::uid id, const scr::Texture::TextureCreateInfo& textureInfo);
	void CompleteMaterial(avs::uid id, const scr::Material::MaterialCreateInfo& materialInfo);
	void CompleteMeshNode(avs::uid id, std::shared_ptr<scr::Node> node);
	void CompleteBone(avs::uid id, std::shared_ptr<scr::Bone> bone);
	void CompleteAnimation(avs::uid id, std::shared_ptr<scr::Animation> animation);

	//Add texture to material being created.
	//	accessor : Data on texture that was received from server.
	//	colourFactor : Vector factor to multiply texture with to adjust strength.
	//	dummyTexture : Texture to use if there is no texture ID assigned.
	//	incompleteMaterial : IncompleteMaterial we are attempting to add the texture to.
	//	materialParameter : Parameter we are modifying.
	void AddTextureToMaterial(const avs::TextureAccessor& accessor,
							  const avs::vec4& colourFactor,
							  const std::shared_ptr<scr::Texture>& dummyTexture,
							  std::shared_ptr<IncompleteMaterial> incompleteMaterial,
							  scr::Material::MaterialParameter& materialParameter);

	MissingResource& GetMissingResource(avs::uid id, const char* resourceType);

	void BasisThread_TranscodeTextures();

	scr::API m_API;
	scr::RenderPlatform* m_pRenderPlatform = nullptr;
	scr::VertexBufferLayout::PackingStyle m_PackingStyle = scr::VertexBufferLayout::PackingStyle::GROUPED;

	basist::etc1_global_selector_codebook basis_codeBook;
#ifdef _MSC_VER
	basist::transcoder_texture_format basis_transcoder_textureFormat =basist::transcoder_texture_format::cTFBC3;
#else
	basist::transcoder_texture_format basis_transcoder_textureFormat =basist::transcoder_texture_format::cTFETC2;
#endif

	std::vector<UntranscodedTexture> texturesToTranscode;
	std::map<avs::uid, scr::Texture::TextureCreateInfo> texturesToCreate; //Textures that are ready to be created <Texture's UID, Texture's Data>
	
	std::mutex mutex_texturesToTranscode;
	std::mutex mutex_texturesToCreate;
	std::atomic_bool shouldBeTranscoding = true;	//Whether the basis thread should be running, and transcoding textures. Settings this to false causes the thread to end.
	std::thread basisThread;						//Thread where we transcode basis files to mip data.
	
	const uint32_t whiteBGRA = 0xFFFFFFFF;
	//const uint32_t normalBGRA = 0xFF7F7FFF;
	const uint32_t normalRGBA = 0xFFFF7F7F;
	const uint32_t combinedBGRA = 0xFFFFFFFF;
	const uint32_t blackBGRA = 0x0;
	const uint32_t greenBGRA = 0xFF337733;
	
	ResourceManager<scr::IndexBuffer> *m_IndexBufferManager = nullptr;
	ResourceManager<scr::Material> *m_MaterialManager = nullptr;
	ResourceManager<scr::Shader> *m_ShaderManager = nullptr;
	ResourceManager<scr::Texture> *m_TextureManager = nullptr;
	ResourceManager<scr::UniformBuffer> *m_UniformBufferManager = nullptr;
	ResourceManager<scr::VertexBuffer> *m_VertexBufferManager = nullptr;
	ResourceManager<scr::Mesh> *m_MeshManager = nullptr;
	ResourceManager<scr::Skin>* m_SkinManager = nullptr;
	ResourceManager<scr::Light> *m_LightManager = nullptr;
	ResourceManager<scr::Bone>* m_BoneManager = nullptr;
	ResourceManager<scr::Animation>* m_AnimationManager = nullptr;

	scr::NodeManager* m_pNodeManager = nullptr;

	std::vector<avs::uid> m_ResourceRequests; //Resources the client will request from the server.
	std::vector<avs::uid> m_ReceivedResources; //Resources the client will confirm receival of.
	std::vector<avs::uid> m_CompletedNodes; //List of IDs of nodes that have been fully received, and have yet to be confirmed to the server.
	std::unordered_map<avs::uid, MissingResource> m_MissingResources; //<ID of Missing Resource, Missing Resource Info>
};

