// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <atomic>
#include <mutex>
#include <set>
#include <thread>

#include <libavstream/geometry/mesh_interface.hpp>
#include <libavstream/mesh.hpp>

#include "NodeManager.h"
#include "ResourceManager.h"

namespace clientrender
{
	class Animation;
	class Material;
	class Light;
}

namespace clientrender
{
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
		avs::GeometryPayloadType resourceType; //String indicating missing resource's type.
		//Resources that can't be completed without this missing resource.
		std::vector<std::shared_ptr<IncompleteResource>> waitingResources;

		MissingResource(avs::uid id, avs::GeometryPayloadType r)
			:id(id), resourceType(r)
		{}
	};
	struct IncompleteMaterial : IncompleteResource
	{
		IncompleteMaterial(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		clientrender::Material::MaterialCreateInfo materialInfo;
		std::unordered_map<avs::uid, std::shared_ptr<clientrender::Texture>&> textureSlots; //<ID of the texture, slot the texture should be placed into>.
	};

	struct IncompleteNode : IncompleteResource
	{
		IncompleteNode(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		std::shared_ptr<clientrender::Node> node;

		std::unordered_map<avs::uid, std::vector<size_t>> materialSlots; //<ID of the material, list of indexes the material should be placed into node material list>.
		std::unordered_map<avs::uid, size_t> missingAnimations; //<ID of missing animation, index in animation vector>
	};

	struct IncompleteSkin : IncompleteResource
	{
		IncompleteSkin(avs::uid id, avs::GeometryPayloadType type)
			:IncompleteResource(id, type)
		{}

		std::shared_ptr<clientrender::Skin> skin;

		std::unordered_map<avs::uid, size_t> missingBones; //<ID of missing bone, index in vector>
	};

	struct UntranscodedTexture
	{
		avs::uid texture_uid;
		std::vector<unsigned char> data; //The raw data of the basis file.
		clientrender::Texture::TextureCreateInfo scrTexture; //Creation information on texture being transcoded.
		std::string name; //For debugging which texture failed.
		avs::TextureCompression fromCompressionFormat;
		float valueScale;	// scale on transcode.
	};
	//! A container for geometry sent from servers and cached locally.
	//! There is one instance of GeometryCache for each connected server, and a local GeometryCache for the client's own objects.
	class GeometryCache : public avs::GeometryCacheBackendInterface
	{
	public:
		GeometryCache(NodeManager *);

		~GeometryCache();

		//Clear any resources that have not been used longer than their expiry time.
		//	timeElapsed_s : Delta time in seconds.
		void Update(float timeElapsed_s)
		{
			mNodeManager->Update(timeElapsed_s);
			mIndexBufferManager.Update(timeElapsed_s);
			mMaterialManager.Update(timeElapsed_s);
			mTextureManager.Update(timeElapsed_s);
			mUniformBufferManager.Update(timeElapsed_s);
			mVertexBufferManager.Update(timeElapsed_s);
			mMeshManager.Update(timeElapsed_s);
			mSkinManager.Update(timeElapsed_s);
			//mLightManager.Update(timeElapsed_s);
			mBoneManager.Update(timeElapsed_s);
			mAnimationManager.Update(timeElapsed_s);
		}


		std::vector<uid> GetAllResourceIDs()
		{
			std::vector<uid> resourceIDs;

			const auto& m = mMaterialManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), m.begin(), m.end());
			const auto& t = mTextureManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), t.begin(), t.end());
			const auto& h = mMeshManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), h.begin(), h.end());
			const auto& s = mSkinManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), s.begin(), s.end());
			const auto& l = mLightManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), l.begin(), l.end());
			const auto& b = mBoneManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), b.begin(), b.end());
			const auto& a = mAnimationManager.GetAllIDs();
			resourceIDs.insert(resourceIDs.end(), a.begin(), a.end());

			return resourceIDs;

			/*
				//We will resend the nodes/objects to update the transform data, as changes in client position (and thus the new invisible nodes) aren't stored for the reconnect.
				mNodeManager;

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
			mNodeManager->Clear();

			mIndexBufferManager.Clear();
			mMaterialManager.Clear();
			mTextureManager.Clear();
			mUniformBufferManager.Clear();
			mVertexBufferManager.Clear();
			mMeshManager.Clear();
			mSkinManager.Clear();
			mLightManager.Clear();
			mBoneManager.Clear();
			mAnimationManager.Clear();
		}

		//Clear all resources that aren't in the exclude list.
		//	excludeList : List of resources that should be spared from clearing of resource managers.
		//	outExistingNodes : List of nodes in the excludeList that existed on the client.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes)
		{
			mMaterialManager.ClearCareful(excludeList);
			mTextureManager.ClearCareful(excludeList);
			mMeshManager.ClearCareful(excludeList);
			mSkinManager.ClearCareful(excludeList);
			mLightManager.ClearCareful(excludeList);
			mBoneManager.ClearCareful(excludeList);
			mAnimationManager.ClearCareful(excludeList);

			//Last as it will likely be the largest.
			mNodeManager->ClearCareful(excludeList, outExistingNodes);

			///As the UIDs of these aren't(?) stored on the server; the server can't confirm their existence.
			///If the mesh is cleared, then these will be cleared.
			//mIndexBufferManager.ClearCareful(excludeList);
			//mShaderManager.ClearCareful(excludeList);
			//mUniformBufferManager.ClearCareful(excludeList);
			//mVertexBufferManager.ClearCareful(excludeList);
		}
		//Returns the resources the ResourceCreator needs, and clears the list.
		std::vector<avs::uid> GetResourceRequests() const override;
		void ClearResourceRequests() override;
		//Returns a list of resource IDs corresponding to the resources the client has received, and clears the list.
		std::vector<avs::uid> GetReceivedResources() const override;
		void ClearReceivedResources() override;
		//Returns the nodes that have been finished since the call, and clears the list.
		std::vector<avs::uid> GetCompletedNodes() const override;
		void ClearCompletedNodes() override;

		std::unique_ptr<clientrender::NodeManager>	mNodeManager;
		ResourceManager<clientrender::IndexBuffer>   mIndexBufferManager;
		ResourceManager<clientrender::Material>		mMaterialManager;
		ResourceManager<clientrender::Texture>       mTextureManager;
		ResourceManager<clientrender::UniformBuffer> mUniformBufferManager;
		ResourceManager<clientrender::VertexBuffer>  mVertexBufferManager;
		ResourceManager<clientrender::Mesh>			mMeshManager;
		ResourceManager<clientrender::Skin>			mSkinManager;
		ResourceManager<clientrender::Light>			mLightManager;
		ResourceManager<clientrender::Bone>			mBoneManager;
		ResourceManager<clientrender::Animation>		mAnimationManager;

		std::vector<avs::uid> m_ResourceRequests; //Resources the client will request from the server.
		std::vector<avs::uid> m_ReceivedResources; //Resources the client will confirm receival of.
		std::vector<avs::uid> m_CompletedNodes; //List of IDs of nodes that have been fully received, and have yet to be confirmed to the server.
		std::unordered_map<avs::uid, MissingResource> m_MissingResources; //<ID of Missing Resource, Missing Resource Info>
	};
}
