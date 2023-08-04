#include "GeometryCache.h"

using namespace clientrender;
#define RESOURCECREATOR_DEBUG_COUT(txt, ...)

platform::crossplatform::RenderPlatform *GeometryCache::renderPlatform=nullptr;

GeometryCache::GeometryCache()
		:mNodeManager(new clientrender::NodeManager),
		  mTextureManager(&clientrender::Texture::Destroy),
		  mIndexBufferManager(&clientrender::IndexBuffer::Destroy),
		  mVertexBufferManager(&clientrender::VertexBuffer::Destroy)
{
}

GeometryCache::~GeometryCache()
{
	auto uids=mSubsceneManager.GetAllIDs();
	for(auto u:uids)
	{
		auto ss=mSubsceneManager.Get(u);
		GeometryCache::DestroyGeometryCache(ss->subscene_uid);
	}
}

static std::map<avs::uid,std::shared_ptr<GeometryCache>> caches;
static std::vector<avs::uid> cache_uids;

void GeometryCache::CreateGeometryCache(avs::uid cache_uid)
{
	caches[cache_uid]=std::make_shared<GeometryCache>();
	cache_uids.push_back(cache_uid);
}

void GeometryCache::DestroyGeometryCache(avs::uid cache_uid)
{
	if(caches.find(cache_uid)!=caches.end())
		caches.erase(cache_uid);
}

std::shared_ptr<GeometryCache> GeometryCache::GetGeometryCache(avs::uid cache_uid)
{
	if(caches.find(cache_uid)==caches.end())
		return nullptr;
	return caches[cache_uid];
}

const std::vector<avs::uid> &GeometryCache::GetCacheUids()
{
	return cache_uids;
}

clientrender::MissingResource* GeometryCache::GetMissingResourceIfMissing(avs::uid id, avs::GeometryPayloadType resourceType)
{
	std::lock_guard g(missingResourcesMutex);
	auto missingPair = m_MissingResources.find(id);
	if (missingPair == m_MissingResources.end())
	{
		return nullptr;
	}
	return &missingPair->second;
}

clientrender::MissingResource& GeometryCache::GetMissingResource(avs::uid id, avs::GeometryPayloadType resourceType)
{
	std::lock_guard g(resourceRequestsMutex);
	std::lock_guard g2(missingResourcesMutex);
	auto missingPair = m_MissingResources.find(id);
	if (missingPair == m_MissingResources.end())
	{
		missingPair = m_MissingResources.emplace(id, MissingResource(id, resourceType)).first;
		m_ResourceRequests.push_back(id);
		TELEPORT_INTERNAL_COUT("Resource {0} of type {1} is missing so far.",id, stringOf(resourceType));
	}
	if(resourceType!=missingPair->second.resourceType)
	{
		TELEPORT_CERR<<"Resource type mismatch"<<std::endl;
	}
	return missingPair->second;
}

const std::vector<avs::uid> &GeometryCache::GetResourceRequests() 
{
	return m_ResourceRequests;
}
std::vector<avs::uid> GeometryCache::GetResourceRequests() const
{
	std::lock_guard g(resourceRequestsMutex);
	std::vector<avs::uid> resourceRequests = m_ResourceRequests;
	//Remove duplicates.
	std::sort(resourceRequests.begin(), resourceRequests.end());
	resourceRequests.erase(std::unique(resourceRequests.begin(), resourceRequests.end()), resourceRequests.end());

	return resourceRequests;
}

void GeometryCache::ClearResourceRequests()
{
	std::lock_guard g(resourceRequestsMutex);
	m_ResourceRequests.clear();
}

void GeometryCache::ReceivedResource(avs::uid id)
{
	std::lock_guard g(receivedResourcesMutex);
	std::lock_guard g2(resourceRequestsMutex);
	m_ReceivedResources.push_back(id);
	auto r = std::find(m_ResourceRequests.begin(), m_ResourceRequests.end(), id);
	if (r != m_ResourceRequests.end())
		m_ResourceRequests.erase(r);
}

void GeometryCache::CompleteResource(avs::uid id)
{
	std::lock_guard g(missingResourcesMutex);
	auto m = m_MissingResources.find(id);
	if(m!= m_MissingResources.end())
		m_MissingResources.erase(m);
}

std::vector<avs::uid> GeometryCache::GetReceivedResources() const
{
	std::lock_guard g(receivedResourcesMutex);
	return m_ReceivedResources;
}

void GeometryCache::ClearReceivedResources()
{
	std::lock_guard g(receivedResourcesMutex);
	m_ReceivedResources.clear();
}

std::vector<avs::uid> GeometryCache::GetCompletedNodes() const
{
	return m_CompletedNodes;
}

void GeometryCache::ClearCompletedNodes()
{
	m_CompletedNodes.clear();
}
#include "Platform/Core/FileLoader.h"


void GeometryCache::setCacheFolder(const std::string &f)
{
	cacheFolder=f;
}

template<typename T> void put(std::vector<uint8_t> &buffer, T t)
{
	size_t sz=buffer.size();
	buffer.resize(sz+sizeof(T));
	T* bt=(T*)(buffer.data()+sz);
	*bt=t;
}

void SaveNodeTree(const std::weak_ptr<clientrender::Node>& n,std::vector<uint8_t> &buffer) 
{
	auto N=n.lock();
	put(buffer,N->GetChildren().size());
	auto T=N->GetLocalTransform();
	put(buffer,T.m_Rotation);
	put(buffer,T.m_Translation);
	put(buffer,T.m_Scale);
	for(size_t i=0;i<N->GetChildren().size();i++)
	{
		SaveNodeTree(N->GetChildren()[i],buffer);
	}
}

void GeometryCache::SaveNodeTree(const std::shared_ptr<clientrender::Node>& n) const
{
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	std::string filename=n->name+".node";
	std::string f=cacheFolder.length()?(cacheFolder+"/")+filename:filename;
	std::vector<uint8_t> buffer;
	::SaveNodeTree(n,buffer);
	fileLoader->Save((const void*)buffer.data(),(unsigned)buffer.size(),f.c_str(),false);
}

avs::Result GeometryCache::CreateSubScene(const SubSceneCreate& subSceneCreate)
{
	std::shared_ptr<SubSceneCreate> s = std::make_shared<SubSceneCreate>(subSceneCreate);
	mSubsceneManager.Add(subSceneCreate.uid, s);
	return avs::Result::OK;
}

void GeometryCache::CompleteMesh(avs::uid id, const clientrender::Mesh::MeshCreateInfo& meshInfo)
{
	//RESOURCECREATOR_DEBUG_COUT( "CompleteMesh(" << id << ", " << meshInfo.name << ")\n";

	std::shared_ptr<clientrender::Mesh> mesh = std::make_shared<clientrender::Mesh>(meshInfo);
	mMeshManager.Add(id, mesh);

	//Add mesh to nodes waiting for mesh.
	MissingResource* missingMesh = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Mesh);
	if (missingMesh)
	{
		for (auto it = missingMesh->waitingResources.begin(); it != missingMesh->waitingResources.end(); it++)
		{
			if (it->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_CERR << "Waiting resource is not a node, it's " << int(it->get()->type) << std::endl;
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetMesh(mesh);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got Mesh {2}({3})", incompleteNode->id, incompleteNode->name, id, meshInfo.name);

			//If only this mesh and this function are pointing to the node, then it is complete.
			if (it->use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}

void GeometryCache::CompleteSkin(avs::uid id, std::shared_ptr<IncompleteSkin> completeSkin)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteSkin {0}({1})",id,completeSkin->skin->name);

	mSkinManager.Add(id, completeSkin->skin);

	//Add skin to nodes waiting for skin.
	MissingResource *missingSkin = GetMissingResourceIfMissing(id,avs::GeometryPayloadType::Skin);
	if(missingSkin)
	{
		for(auto it = missingSkin->waitingResources.begin(); it != missingSkin->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetSkin(completeSkin->skin);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Skin {0}({1})",incompleteNode->id,incompleteNode->name,id,completeSkin->skin->name);

			//If only this resource and this skin are pointing to the node, then it is complete.
			if(it->use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}

void GeometryCache::CompleteTexture(avs::uid id, const clientrender::Texture::TextureCreateInfo& textureInfo)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteTexture {0}()",id,textureInfo.name,magic_enum::enum_name<clientrender::Texture::CompressionFormat>(textureInfo.compression));
	std::shared_ptr<clientrender::Texture> scrTexture = std::make_shared<clientrender::Texture>(renderPlatform);
	scrTexture->Create(textureInfo);

	mTextureManager.Add(id, scrTexture);

	//Add texture to materials waiting for texture.
	MissingResource * missingTexture = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Texture);
	if(missingTexture)
	{
		for(auto it = missingTexture->waitingResources.begin(); it != missingTexture->waitingResources.end(); it++)
		{
			switch((*it)->type)
			{
				case avs::GeometryPayloadType::FontAtlas:
					{
						std::shared_ptr<IncompleteFontAtlas> incompleteFontAtlas = std::static_pointer_cast<IncompleteFontAtlas>(*it);
						RESOURCECREATOR_DEBUG_COUT("Waiting FontAtlas {0} got Texture {1}({2})", incompleteFontAtlas->id,id,textureInfo.name);

						CompleteResource(incompleteFontAtlas->id);
					}
					break;
				case avs::GeometryPayloadType::Material:
					{
						std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::static_pointer_cast<IncompleteMaterial>(*it);
						// Replacing this nonsense:
						//incompleteMaterial->textureSlots.at(id) = scrTexture;
						// with the correct:
						if(incompleteMaterial->materialInfo.diffuse.texture_uid==id)
							incompleteMaterial->materialInfo.diffuse.texture=scrTexture;
						if(incompleteMaterial->materialInfo.normal.texture_uid==id)
							incompleteMaterial->materialInfo.normal.texture=scrTexture;
						if(incompleteMaterial->materialInfo.combined.texture_uid==id)
							incompleteMaterial->materialInfo.combined.texture=scrTexture;
						if(incompleteMaterial->materialInfo.emissive.texture_uid==id)
							incompleteMaterial->materialInfo.emissive.texture=scrTexture;
						RESOURCECREATOR_DEBUG_COUT( "Waiting Material ",") got Texture ",incompleteMaterial->id,incompleteMaterial->materialInfo.name,id,textureInfo.name);

						//If only this texture and this function are pointing to the material, then it is complete.
						if (it->use_count() == 2)
						{
							CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
						}
						else
						{
							RESOURCECREATOR_DEBUG_COUT(" Still awaiting {0} resources.",(it->use_count()-2));
						}
					}
					break;
				case avs::GeometryPayloadType::Node:
					{
						std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
						RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Texture {2}({3})",incompleteNode->id,incompleteNode->name.c_str(),id,textureInfo.name);

						//If only this material and function are pointing to the MeshNode, then it is complete.
						if(incompleteNode.use_count() == 2)
						{
							CompleteNode(incompleteNode->id, incompleteNode);
						}
					}
					break;
				default:
					break;
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}
void GeometryCache::CompleteBone(avs::uid id, std::shared_ptr<clientrender::Bone> bone)
{
	//RESOURCECREATOR_DEBUG_COUT( "CompleteBone(",id,", ",bone->name,")\n";

	mBoneManager.Add(id, bone);

	//Add bone to skin waiting for bone.
	MissingResource *missingBone = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Bone);
	if(missingBone)
	{
		for(auto it = missingBone->waitingResources.begin(); it != missingBone->waitingResources.end(); it++)
		{
			if((*it)->type == avs::GeometryPayloadType::Skin)
			{
				std::shared_ptr<IncompleteSkin> incompleteSkin = std::static_pointer_cast<IncompleteSkin>(*it);
				incompleteSkin->skin->SetBone(incompleteSkin->missingBones[id], bone);
				RESOURCECREATOR_DEBUG_COUT( "Waiting Skin {0}({1}) got Bone {2}({3})",incompleteSkin->id,incompleteSkin->skin->name,id,bone->name);

				//If only this bone, and the loop, are pointing at the skin, then it is complete.
				if(it->use_count() == 2)
				{
					CompleteSkin(incompleteSkin->id, incompleteSkin);
				}
			}
		}
	}

	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}

void GeometryCache::CompleteAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteAnimation {0}({1})",id,animation->name);

	//Update animation length before adding to the animation manager.
	animation->updateAnimationLength();
	mAnimationManager.Add(id, animation);

	//Add animation to waiting nodes.
	MissingResource *missingAnimation = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Animation);
	if(missingAnimation)
	{
		for(auto it = missingAnimation->waitingResources.begin(); it != missingAnimation->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->animationComponent.addAnimation(id, animation);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Animation {2}({3})",incompleteNode->id,incompleteNode->name,id,animation->name);

			//If only this bone, and the loop, are pointing at the skin, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}

	}
	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}
void GeometryCache::CompleteMaterial(avs::uid id, const clientrender::Material::MaterialCreateInfo& materialInfo)
{
	//RESOURCECREATOR_DEBUG_COUT( "CompleteMaterial {0}({1})",id,materialInfo.name);
	std::shared_ptr<clientrender::Material> material = mMaterialManager.Get(id);
	if (!material)
	{
		TELEPORT_INTERNAL_CERR("Trying to complete material {0}, but it hasn't been created.\n", id);
		return;
	}
	// Update its properties:
	material->SetMaterialCreateInfo(materialInfo);
	//Add material to nodes waiting for material.
	MissingResource *missingMaterial = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Material);
	if(missingMaterial)
	{
		for(auto it = missingMaterial->waitingResources.begin(); it != missingMaterial->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);

			const auto &indexesPair = incompleteNode->materialSlots.find(id);
			if (indexesPair == incompleteNode->materialSlots.end())
			{
				TELEPORT_CERR<<"Material "<<id<<" not found in incomplete node "<<incompleteNode->id<<std::endl;
				continue;
			}
			for(size_t materialIndex : indexesPair->second)
			{
				incompleteNode->SetMaterial(materialIndex, material);
			}
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Material {2}({3})",incompleteNode->id,incompleteNode->name,id,materialInfo.name);

			//If only this material and function are pointing to the MeshNode, then it is complete.
			if(incompleteNode.use_count() == 2)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
			mNodeManager->NotifyModifiedMaterials(incompleteNode);
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	CompleteResource(id);
}

void GeometryCache::CompleteNode(avs::uid id, std::shared_ptr<clientrender::Node> node)
{
//	RESOURCECREATOR_DEBUG_COUT( "CompleteMeshNode(ID: ",id,", node: ",node->name,")\n";

	///We're using the node ID as the node ID as we are currently generating an node per node/transform anyway; this way the server can tell the client to remove an node.
	m_CompletedNodes.push_back(id);
}

void GeometryCache::AddTextureToMaterial(const avs::TextureAccessor& accessor, const vec4& colourFactor, const std::shared_ptr<clientrender::Texture>& dummyTexture,
										   std::shared_ptr<IncompleteMaterial> incompleteMaterial, clientrender::Material::MaterialParameter& materialParameter)
{
	materialParameter.texture_uid=accessor.index;
	if (accessor.index != 0)
	{
		const std::shared_ptr<clientrender::Texture> texture = mTextureManager.Get(accessor.index);

		if (texture)
		{
			materialParameter.texture = texture;
		}
		else
		{
			//TELEPORT_DEBUG_COUT( "Material {0}({1}) missing Texture ",incompleteMaterial->id,"(",incompleteMaterial->id,accessor.index);
			clientrender::MissingResource& missing=GetMissingResource(accessor.index, avs::GeometryPayloadType::Texture);
			missing.waitingResources.insert(incompleteMaterial);
			incompleteMaterial->missingTextureUids.insert(accessor.index);
		}

		vec2 tiling = { accessor.tiling.x, accessor.tiling.y };

		materialParameter.texCoordsScalar[0] = tiling;
		materialParameter.texCoordsScalar[1] = tiling;
		materialParameter.texCoordsScalar[2] = tiling;
		materialParameter.texCoordsScalar[3] = tiling;
		materialParameter.texCoordIndex = static_cast<float>(accessor.texCoord);
	}
	else
	{
		materialParameter.texture = dummyTexture;
		materialParameter.texCoordsScalar[0] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[1] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[2] = vec2(1.0f, 1.0f);
		materialParameter.texCoordsScalar[3] = vec2(1.0f, 1.0f);
		materialParameter.texCoordIndex = 0.0f;
	}

	materialParameter.textureOutputScalar = *((vec4*)&colourFactor);
}