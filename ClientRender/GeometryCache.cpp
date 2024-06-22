#include "GeometryCache.h"
#include "InstanceRenderer.h"
#include "Renderer.h"
#include <filesystem>
#include "TeleportCore/ResourceStreams.h"
using namespace teleport;
using namespace clientrender;
#define RESOURCECREATOR_DEBUG_COUT(txt, ...) TELEPORT_INTERNAL_COUT(txt, ##__VA_ARGS__)

platform::crossplatform::RenderPlatform *GeometryCache::renderPlatform=nullptr;

GeometryCache::GeometryCache(avs::uid c_uid, avs::uid parent_c_uid, const std::string &n)
	: cache_uid(c_uid), parent_cache_uid(parent_c_uid),name(n),mNodeManager(flecs_world),
	  mMaterialManager(c_uid),
	  mSubsceneManager(c_uid),
	  mTextureManager(c_uid, &clientrender::Texture::Destroy),
	  mMeshManager(c_uid),
	  mSkeletonManager(c_uid),
	  mLightManager(c_uid),
	  mAnimationManager(c_uid),
	  mTextCanvasManager(c_uid),
	  mFontAtlasManager(c_uid),
	  mIndexBufferManager(c_uid, &clientrender::IndexBuffer::Destroy),
	  mVertexBufferManager(c_uid, &clientrender::VertexBuffer::Destroy)
{
	using avs::Pose;
	flecs_world.set_entity_range(1, 50000000);
	//ECS_COMPONENT(flecs_world, Pose);
	auto addFn = std::bind(&Renderer::AddNodeToRender,Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionAddNodeForRender(addFn);
	auto removeFn = std::bind(&Renderer::RemoveNodeFromRender, Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionRemoveNodeFromRender(removeFn);
	auto updateFn = std::bind(&Renderer::UpdateNodeInRender, Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionUpdateNodeInRender(updateFn);
}

GeometryCache::~GeometryCache()
{
	flecs_world.quit();
	auto uids=mSubsceneManager.GetAllIDs();
	for(auto u:uids)
	{
		auto ss=mSubsceneManager.Get(u);
		GeometryCache::DestroyGeometryCache(ss->subscene_uid);
	}
}

static std::map<avs::uid,std::shared_ptr<GeometryCache>> caches;
static std::vector<avs::uid> cache_uids;

void GeometryCache::CreateGeometryCache(avs::uid cache_uid,avs::uid parent_cache_uid,const std::string &name)
{
	caches[cache_uid] = std::make_shared<GeometryCache>(cache_uid, parent_cache_uid,name);
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
void GeometryCache::DestroyAllCaches()
{
	caches.clear();
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
		if(m_ResourceRequests.size()>4096)
			DebugBreak();
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
	if(m_ResourceRequests.size()>8192)
		DebugBreak();
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
	{
		std::lock_guard g(receivedResourcesMutex);
		std::lock_guard g2(resourceRequestsMutex);
		m_ReceivedResources.push_back(id);
		auto r = std::find(m_ResourceRequests.begin(), m_ResourceRequests.end(), id);
		if (r != m_ResourceRequests.end())
			m_ResourceRequests.erase(r);
	}
}

void GeometryCache::RemoveFromMissingResources(avs::uid id)
{
	std::lock_guard g(missingResourcesMutex);
	auto m = m_MissingResources.find(id);
	if (m != m_MissingResources.end())
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

const std::vector<avs::uid> &GeometryCache::GetCompletedNodes() const
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
	//Add mesh to nodes waiting for mesh.
	MissingResource* missingSubScene = GetMissingResourceIfMissing(subSceneCreate.uid, avs::GeometryPayloadType::Mesh);
	if (missingSubScene)
	{
		for (auto it = missingSubScene->waitingResources.begin(); it != missingSubScene->waitingResources.end(); it++)
		{
			if (it->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_CERR << "Waiting resource is not a node, it's " << int(it->get()->type) << std::endl;
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			RESOURCE_RECEIVES(incompleteNode,subSceneCreate.uid);
			size_t num_remaining = RESOURCES_AWAITED(*it);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got SubScene {2}=cache {3}, missing {4} or {5}", incompleteNode->id, incompleteNode->name, subSceneCreate.uid, subSceneCreate.subscene_uid,num_remaining,incompleteNode->GetMissingResourceCount());
			//If only this mesh and this function are pointing to the node, then it is complete.
			if (RESOURCE_IS_COMPLETE(*it))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(subSceneCreate.uid);
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
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Mesh {2}({3}), now missing {4}", incompleteNode->id, incompleteNode->name, id, meshInfo.name, incompleteNode->GetMissingResourceCount());

			//If only this mesh and this function are pointing to the node, then it is complete.
			if (RESOURCE_IS_COMPLETE(*it))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteSkeleton(avs::uid id, std::shared_ptr<IncompleteSkeleton> completeSkeleton)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteSkeleton {0}({1})",id,completeSkeleton->skeleton->name);
	//Add skeleton to nodes waiting for skeleton.
	mSkeletonManager.Get(id)->InitBones(*this);
	MissingResource *missingSkeleton = GetMissingResourceIfMissing(id,avs::GeometryPayloadType::Skeleton);
	if(missingSkeleton)
	{
		for(auto it = missingSkeleton->waitingResources.begin(); it != missingSkeleton->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetSkeleton(completeSkeleton->skeleton);
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Skeleton {0}({1})",incompleteNode->id,incompleteNode->name,id,completeSkeleton->skeleton->name);
			//If only this skeleton and this function are pointing to the node, then it is complete.
			if (incompleteNode->GetMissingResourceCount()==0)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteTexture(avs::uid id, const clientrender::Texture::TextureCreateInfo& textureInfo)
{
	RESOURCECREATOR_DEBUG_COUT( "CompleteTexture {0}({1})",id,textureInfo.name);
	std::shared_ptr<clientrender::Texture> scrTexture = std::make_shared<clientrender::Texture>(renderPlatform);
	scrTexture->Create(textureInfo);

	mTextureManager.Add(id, scrTexture);

	//Add texture to materials waiting for texture.
	MissingResource * missingTexture = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Texture);
	if(missingTexture)
	{
		for(auto it = missingTexture->waitingResources.begin(); it != missingTexture->waitingResources.end(); it++)
		{
			RESOURCE_RECEIVES(*it,id);
			switch((*it)->type)
			{
				case avs::GeometryPayloadType::FontAtlas:
					{
						std::shared_ptr<IncompleteFontAtlas> incompleteFontAtlas = std::static_pointer_cast<IncompleteFontAtlas>(*it);
						RESOURCECREATOR_DEBUG_COUT("Waiting FontAtlas {0} got Texture {1}({2})", incompleteFontAtlas->id,id,textureInfo.name);
						RemoveFromMissingResources(incompleteFontAtlas->id);
						std::shared_ptr<FontAtlas> fontAtlas = std::static_pointer_cast<FontAtlas>(*it);
						CompleteFontAtlas(incompleteFontAtlas->id, fontAtlas);
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
						TELEPORT_LOG("Waiting Material {0}({1}) got Texture {2}({3})",incompleteMaterial->id,incompleteMaterial->materialInfo.name,id,textureInfo.name);

						//If only this texture and this function are pointing to the material, then it is complete.
						if (RESOURCE_IS_COMPLETE(*it))
						{
							CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
						}
						else
						{
							TELEPORT_LOG(" Still awaiting {0} resources.", RESOURCES_AWAITED(*it));
						}
					}
					break;
				case avs::GeometryPayloadType::Node:
					{
						std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
						size_t num_remaining = RESOURCES_AWAITED(incompleteNode);
						RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Texture {2}({3}), missing {4} or {5}", incompleteNode->id, incompleteNode->name.c_str(), id, textureInfo.name, num_remaining,incompleteNode->GetMissingResourceCount());

						//If only this material and function are pointing to the MeshNode, then it is complete.
						if (RESOURCE_IS_COMPLETE(incompleteNode))
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
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteFontAtlas(avs::uid id, std::shared_ptr<clientrender::FontAtlas> fontAtlas)
{
	fontAtlas->fontTexture = mTextureManager.Get(fontAtlas->font_texture_uid);
	// If the font atlas wasn't sent via a url it may not have one.
	if(fontAtlas->url.length()==0)
	{
		std::string name = std::string(fontAtlas->fontTexture->getName());
		std::replace(name.begin(),name.end(),'.','#');
		fontAtlas->url=name+"_atlas";
	}
	SaveResource(*fontAtlas);
}

std::string GeometryCache::URLToFilePath(std::string url)
{
	if(url.length()==0)
		return "";
	using namespace std::filesystem;
	size_t protocol_end = url.find("://");
	std::string filepath = url.substr(protocol_end + 3, url.length() - protocol_end - 3);
	size_t first_slash = filepath.find("/");
	if (first_slash >= filepath.length())
		first_slash = filepath.length();
	std::string base_url = filepath.substr(0, first_slash);
	filepath = filepath.substr(first_slash, filepath.length() - first_slash);
	size_t colon_pos = base_url.find(":");
	if (colon_pos < base_url.length())
		base_url = base_url.substr(0, colon_pos);
	filepath = base_url + filepath;
	// TODO: check path length is not too long.
	return filepath;
}

bool GeometryCache::SaveResource(const IncompleteResource &res)
{
	std::string filename = URLToFilePath(res.url);
	if(filename=="")
		return false;
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader)
		return false;
	using namespace std::filesystem;
	filename+=".";
	filename+=res.GetFileExtension();
	std::string f = cacheFolder.length() ? (cacheFolder + "/") + filename : filename;
	path fullPath = path(f);
	try
	{
	std::filesystem::create_directories(fullPath.parent_path());
	}
	catch(...)
	{
	}
	std::stringstream s;
	res.Save(s);
	std::string buffer=s.str();
	fileLoader->Save(buffer.c_str(),(uint32_t)buffer.length(),f.c_str(),false);
	return true;
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
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT( "Waiting MeshNode {0}({1}) got Animation {2}({3})",incompleteNode->id,incompleteNode->name,id,animation->name);

			auto animC = incompleteNode->GetOrCreateComponent<AnimationComponent>();
			animC->addAnimation(id, animation);
			//If only this bone, and the loop, are pointing at the skeleton, then it is complete.
			if (RESOURCE_IS_COMPLETE(incompleteNode))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}

	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}
void GeometryCache::CompleteMaterial(avs::uid id, const clientrender::Material::MaterialCreateInfo& materialInfo)
{
	TELEPORT_INTERNAL_COUT("CompleteMaterial {0} ({1})", id, materialInfo.name);
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
			size_t num_remaining = RESOURCES_AWAITED(incompleteNode);
			RESOURCE_RECEIVES(incompleteNode,id);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got Material {2}({3}) - missing {4} or {5}", incompleteNode->id, incompleteNode->name, id, materialInfo.name, num_remaining,incompleteNode->GetMissingResourceCount());

			//If only this material and function are pointing to the MeshNode, then it is complete.
			if (RESOURCE_IS_COMPLETE(incompleteNode))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
			mNodeManager.NotifyModifiedMaterials(incompleteNode);
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteNode(avs::uid id, std::shared_ptr<clientrender::Node> node)
{
//	TELEPORT_INTERNAL_CERR( "CompleteNode {0} {1}",id,node->name);
	///We're using the node ID as the node ID as we are currently generating an node per node/transform anyway; this way the server can tell the client to remove an node.
	m_CompletedNodes.push_back(id);
	RemoveFromMissingResources(id);
	mNodeManager.CompleteNode(id);
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
			if(incompleteMaterial->missingTextureUids.find(accessor.index)==incompleteMaterial->missingTextureUids.end())
			{
				clientrender::MissingResource& missing=GetMissingResource(accessor.index, avs::GeometryPayloadType::Texture);
				missing.waitingResources.insert(incompleteMaterial);
				RESOURCE_AWAITS(incompleteMaterial,accessor.index);
				incompleteMaterial->missingTextureUids.insert(accessor.index);
			}
		}

		vec2 tiling = { accessor.tiling.x, accessor.tiling.y };

		materialParameter.texCoordsScale = tiling;
		materialParameter.texCoordIndex = (int)accessor.texCoord;
	}
	else
	{
		materialParameter.texture = dummyTexture;
		materialParameter.texCoordsScale = vec2(1.0f, 1.0f);
		materialParameter.texCoordIndex = 0;
	}

	materialParameter.textureOutputScalar = *((vec4*)&colourFactor);
}