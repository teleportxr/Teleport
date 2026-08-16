#include "GeometryCache.h"
#include "GeometryDecoder.h"
#include "InstanceRenderer.h"
#include "Renderer.h"
#include "TeleportCore/ResourceStreams.h"
#include "Platform/Core/FileLoader.h"
#include "ClientRender/NodeComponents/AnimationComponent.h"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>

using namespace teleport;
using namespace clientrender;
#define RESOURCECREATOR_DEBUG_COUT(txt, ...) TELEPORT_INTERNAL_COUT(Resource, txt, ##__VA_ARGS__)

platform::crossplatform::RenderPlatform *GeometryCache::renderPlatform = nullptr;

GeometryCache::GeometryCache(avs::uid c_uid, avs::uid parent_c_uid, const std::string &n)
	: cache_uid(c_uid), parent_cache_uid(parent_c_uid), name(n), mNodeManager(), mMaterialManager(c_uid),
	  mTextureManager(c_uid, &clientrender::Texture::Destroy), mMeshManager(c_uid), mSkeletonManager(c_uid), mLightManager(c_uid), mAnimationManager(c_uid),
	  mTextCanvasManager(c_uid), mFontAtlasManager(c_uid), mIndexBufferManager(c_uid, &clientrender::IndexBuffer::Destroy),
	  mVertexBufferManager(c_uid, &clientrender::VertexBuffer::Destroy)
{
	// So the node manager can resolve an ApplyAnimation whose cacheID is zero, which the protocol
	// defines as "the cache containing nodeID".
	mNodeManager.SetCacheUid(c_uid);
	auto addFn = std::bind(&Renderer::UpdateNodeInRender, Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionAddNodeForRender(addFn);
	auto removeFn = std::bind(&Renderer::RemoveNodeFromRender, Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionRemoveNodeFromRender(removeFn);
	auto updateFn = std::bind(&Renderer::UpdateNodeInRender, Renderer::GetRenderer(), c_uid, std::placeholders::_1);
	mNodeManager.SetFunctionUpdateNodeInRender(updateFn);
}

GeometryCache::~GeometryCache()
{
	auto uids = mMeshManager.GetAllIDs();
	for (auto u : uids)
	{
		auto ss = mMeshManager.Get(u);
		if (!ss) continue;
		auto cache_uid = ss->GetMeshCreateInfo().subscene_cache_uid;
		if (cache_uid != 0) GeometryCache::DestroyGeometryCache(cache_uid);
	}
}

static std::map<avs::uid, std::shared_ptr<GeometryCache>> caches;
static std::vector<avs::uid> cache_uids;
//! Ids for resources the client mints for itself, rather than being given by a server.
//!
//! These share a cache - and its missing-resource table, which is keyed by id alone - with resources
//! the server numbered, so the two ranges must not meet. avs::GenerateUid counts up from 1, exactly
//! as a server's own numbering does, so it cannot be used here: a texture that happened to be given
//! the id of a mesh some node was waiting for would be handed to that node as its mesh, and the
//! record of what the node actually wanted would then be erased. Hence a range no server reaches.
//!
//! Only the top bit is claimed, so an id from here is recognisable on sight in a log.
static std::atomic<avs::uid> nextLocalResourceUid{0x8000000000000000ULL};

//! Guards both of the above. A sub-scene cache is created on the decode thread while the render
//! thread walks the caches and the transcode thread resolves one to complete a texture, so the
//! map is reached from three threads at once. Never held while a cache is destroyed: ~GeometryCache
//! destroys the sub-scene caches its meshes own, which re-enters DestroyGeometryCache.
static std::mutex cachesMutex;

void GeometryCache::CreateGeometryCache(avs::uid cache_uid, avs::uid parent_cache_uid, const std::string &name)
{
	if (cache_uid == 0xFFFFFFFFFFFFFFFF)
	{
		TELEPORT_WARN("Trying to create invalid cache -1");
		return;
	}
	auto cache = std::make_shared<GeometryCache>(cache_uid, parent_cache_uid, name);
	cache->SetDefaultURLRoot(name);
	std::lock_guard g(cachesMutex);
	caches[cache_uid] = cache;
	if (std::find(cache_uids.begin(), cache_uids.end(), cache_uid) == cache_uids.end())
	{
		cache_uids.push_back(cache_uid);
	}
}

void GeometryCache::DestroyGeometryCache(avs::uid cache_uid)
{
	// Taken out of the map under the lock and released after it: ~GeometryCache calls back into
	// this function for each sub-scene it owns, which would deadlock on a non-recursive lock, and
	// the waiter purge below takes the root's texture-url lock.
	std::shared_ptr<GeometryCache> dying;
	{
		std::lock_guard g(cachesMutex);
		auto			i = caches.find(cache_uid);
		if (i == caches.end())
		{
			return;
		}
		dying = std::move(i->second);
		caches.erase(i);
		cache_uids.erase(std::remove(cache_uids.begin(), cache_uids.end(), cache_uid), cache_uids.end());
	}
	// Nothing in this cache can be given a texture now, so release its claims on urls still in
	// flight; otherwise the root holds waiters that can never be satisfied.
	if (dying)
	{
		dying->AbandonTextureUrlWaiters();
	}
}

std::shared_ptr<GeometryCache> GeometryCache::GetGeometryCache(avs::uid cache_uid)
{
	std::lock_guard g(cachesMutex);
	auto			i = caches.find(cache_uid);
	if (i == caches.end())
	{
		return nullptr;
	}
	return i->second;
}
void GeometryCache::DestroyAllCaches()
{
	// As DestroyGeometryCache: emptied under the lock, destroyed outside it.
	std::map<avs::uid, std::shared_ptr<GeometryCache>> dying;
	{
		std::lock_guard g(cachesMutex);
		dying.swap(caches);
		cache_uids.clear();
	}
	dying.clear();
}

std::vector<avs::uid> GeometryCache::GetCacheUids()
{
	std::lock_guard g(cachesMutex);
	return cache_uids;
}

clientrender::MissingResource *GeometryCache::GetMissingResourceIfMissing(avs::uid id, avs::GeometryPayloadType resourceType)
{
	auto missingPair = m_MissingResources.find(id);
	if (missingPair == m_MissingResources.end())
	{
		return nullptr;
	}
	// The table is keyed by uid alone, but a uid only identifies a resource together with its type.
	// Answering a query about one type with an entry recorded for another hands a resource to things
	// that were waiting for something else entirely, and then erases the record of what they really
	// wanted - so an id used twice does not merely warn, it silently loses a resource.
	if (missingPair->second.resourceType != resourceType)
	{
		TELEPORT_WARN_NOSPAM("Cache {0} was asked for missing {1} {2}, but {2} is a missing {3}.",
							 cache_uid,
							 stringOf(resourceType),
							 id,
							 stringOf(missingPair->second.resourceType));
		return nullptr;
	}
	return &missingPair->second;
}

clientrender::MissingResource &GeometryCache::GetMissingResource(avs::uid id, avs::GeometryPayloadType resourceType)
{
	std::lock_guard g(resourceRequestsMutex);
	auto missingPair = m_MissingResources.find(id);
	if (missingPair == m_MissingResources.end())
	{
		missingPair = m_MissingResources.emplace(id, MissingResource(id, resourceType)).first;
		m_ResourceRequests.push_back(id);
		TELEPORT_INTERNAL_COUT(Resource, "Resource {0} of type {1} is missing so far.", id, stringOf(resourceType));
		if (m_ResourceRequests.size() > 4096) DebugBreak();
	}
	if (resourceType != missingPair->second.resourceType)
	{
		TELEPORT_INTERNAL_CERR("Resource type mismatch for resource {0}: expected {1} but got {2}.", id, stringOf(missingPair->second.resourceType), stringOf(resourceType));
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
	if (m_ResourceRequests.size() > 8192) DebugBreak();
	std::vector<avs::uid> resourceRequests = m_ResourceRequests;
	// Remove duplicates.
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
	if (r != m_ResourceRequests.end()) m_ResourceRequests.erase(r);
}

void GeometryCache::RemoveFromMissingResources(avs::uid id)
{
	if (missingResourcesMutex.try_lock())
	{
		TELEPORT_WARN("missingResourcesMutex was not locked.");
		missingResourcesMutex.unlock();
	}
	auto m = m_MissingResources.find(id);
	if (m != m_MissingResources.end()) m_MissingResources.erase(m);
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

void GeometryCache::setSaveFolder(const std::string &f)
{
	saveFolder = f;
}

template <typename T> void put(std::vector<uint8_t> &buffer, T t)
{
	size_t sz = buffer.size();
	buffer.resize(sz + sizeof(T));
	T *bt = (T *)(buffer.data() + sz);
	*bt	  = t;
}

void SaveNodeTree(const std::weak_ptr<clientrender::Node> &n, std::vector<uint8_t> &buffer)
{
	auto N = n.lock();
	put(buffer, N->GetChildren().size());
	auto T = N->GetLocalTransform();
	put(buffer, T.m_Rotation);
	put(buffer, T.m_Translation);
	put(buffer, T.m_Scale);
	for (size_t i = 0; i < N->GetChildren().size(); i++)
	{
		SaveNodeTree(N->GetChildren()[i], buffer);
	}
}

void GeometryCache::SaveNodeTree(const std::shared_ptr<clientrender::Node> &n) const
{
	auto *fileLoader	 = platform::core::FileLoader::GetFileLoader();
	std::string filename = n->name + ".node";
	std::string f		 = saveFolder.length() ? (saveFolder + "/") + filename : filename;
	std::vector<uint8_t> buffer;
	::SaveNodeTree(n, buffer);
	fileLoader->Save((const void *)buffer.data(), (unsigned)buffer.size(), f.c_str(), false);
}

avs::Result GeometryCache::CreateSubScene(const SubSceneCreate &subSceneCreate)
{
	std::shared_ptr<clientrender::Mesh> s = std::make_shared<clientrender::Mesh>(subSceneCreate);
	mMeshManager.Add(subSceneCreate.uid, s);
	// Add mesh to nodes waiting for mesh.
	std::lock_guard g(missingResourcesMutex);
	MissingResource *missingSubScene = GetMissingResourceIfMissing(subSceneCreate.uid, avs::GeometryPayloadType::Mesh);
	if (missingSubScene)
	{
		for (auto it = missingSubScene->waitingResources.begin(); it != missingSubScene->waitingResources.end(); it++)
		{
			if (it->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_INTERNAL_CERR("Waiting resource is not a node, it's {}" , int(it->get()->type));
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			// As CompleteMesh does for an ordinary mesh. Without this a node that was created before
			// its sub-scene arrived keeps a SubSceneComponent whose mesh_uid is set but whose mesh
			// pointer is null, forever. It still renders, because InstanceRenderer looks the mesh up
			// by uid from the manager rather than through this pointer - which is exactly why the
			// omission went unnoticed - but anything reading the pointer gets nothing.
			incompleteNode->SetMesh(s);
			RESOURCE_RECEIVES(incompleteNode, subSceneCreate.uid);
			size_t num_remaining = RESOURCES_AWAITED(*it);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got SubScene {2}=cache {3}, missing {4} or {5}",
									   incompleteNode->id,
									   incompleteNode->name,
									   subSceneCreate.uid,
									   subSceneCreate.subscene_cache_uid,
									   num_remaining,
									   incompleteNode->GetMissingResourceCount());
			// If only this mesh and this function are pointing to the node, then it is complete.
			if (RESOURCE_IS_COMPLETE(*it))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(subSceneCreate.uid);
	return avs::Result::OK;
}

void GeometryCache::CompleteMesh(avs::uid id, const clientrender::Mesh::MeshCreateInfo &meshInfo)
{
	std::shared_ptr<clientrender::Mesh> mesh = std::make_shared<clientrender::Mesh>(meshInfo);
	mMeshManager.Add(id, mesh);

	std::lock_guard g(missingResourcesMutex);
	// Add mesh to nodes waiting for mesh.
	MissingResource *missingMesh = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Mesh);
	if (missingMesh)
	{
		for (auto it = missingMesh->waitingResources.begin(); it != missingMesh->waitingResources.end(); it++)
		{
			if (it->get()->type != avs::GeometryPayloadType::Node)
			{
				TELEPORT_INTERNAL_CERR("Waiting resource is not a node, it's {}" , int(it->get()->type));
				continue;
			}
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetMesh(mesh);
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Mesh {2}({3}), now missing {4}",
									   incompleteNode->id,
									   incompleteNode->name,
									   id,
									   meshInfo.name,
									   incompleteNode->GetMissingResourceCount());

			// If only this mesh and this function are pointing to the node, then it is complete.
			if (RESOURCE_IS_COMPLETE(*it))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteSkeleton(avs::uid id, std::shared_ptr<IncompleteSkeleton> completeSkeleton)
{
	RESOURCECREATOR_DEBUG_COUT("CompleteSkeleton {0}({1})", id, completeSkeleton->skeleton->name);
	// Add skeleton to nodes waiting for skeleton.
	mSkeletonManager.Get(id)->InitBones(*this);
	MissingResource *missingSkeleton = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Skeleton);
	if (missingSkeleton)
	{
		for (auto it = missingSkeleton->waitingResources.begin(); it != missingSkeleton->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			incompleteNode->SetSkeleton(completeSkeleton->skeleton);
			incompleteNode->GetOrCreateComponent<AnimationComponent>();
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT(
				"Waiting MeshNode {0}({1}) got Skeleton {0}({1})", incompleteNode->id, incompleteNode->name, id, completeSkeleton->skeleton->name);
			// If only this skeleton and this function are pointing to the node, then it is complete.
			if (incompleteNode->GetMissingResourceCount() == 0)
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteTexture(avs::uid id, const clientrender::Texture::TextureCreateInfo &textureInfo)
{
	RESOURCECREATOR_DEBUG_COUT("CompleteTexture {0}({1})", id, textureInfo.name);
	// The texture carries the cache that holds it, because a uid alone cannot find it: a material
	// in a sub-scene may sample a texture the session's cache holds, and anything navigating from
	// the one to the other needs to know where to look. See Texture::TextureCreateInfo::cache_uid.
	clientrender::Texture::TextureCreateInfo createInfo = textureInfo;
	createInfo.cache_uid							   = cache_uid;
	std::shared_ptr<clientrender::Texture> scrTexture   = std::make_shared<clientrender::Texture>(renderPlatform);
	scrTexture->Create(createInfo);

	const std::string textureName = std::string(scrTexture->getName());
	mTextureManager.Add(id, scrTexture);

	CompleteResourcesWaitingForTexture(id, scrTexture, textureName);

	// Separately, and outside that lock: materials elsewhere in the tree that named this texture's
	// url rather than its uid. They live in other caches, and completing one takes that cache's
	// own missing-resource lock.
	PublishTextureToUrlWaiters(id, scrTexture);
}

//! The uid-keyed half of completion: everything in *this* cache that was waiting for this id.
void GeometryCache::CompleteResourcesWaitingForTexture(avs::uid id, std::shared_ptr<clientrender::Texture> scrTexture, const std::string &textureName)
{
	std::lock_guard g(missingResourcesMutex);
	// Add texture to materials waiting for texture.
	// GetMissingResourceIfMissing checks the type itself, so an id recorded as a missing something
	// else comes back null rather than being handed this texture.
	MissingResource *missingTexture = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Texture);
	if (missingTexture)
	{
		for (auto it = missingTexture->waitingResources.begin(); it != missingTexture->waitingResources.end(); it++)
		{
			RESOURCE_RECEIVES(*it, id);
			switch ((*it)->type)
			{
			case avs::GeometryPayloadType::FontAtlas:
			{
				std::shared_ptr<IncompleteFontAtlas> incompleteFontAtlas = std::static_pointer_cast<IncompleteFontAtlas>(*it);
				RESOURCECREATOR_DEBUG_COUT("Waiting FontAtlas {0} got Texture {1}({2})", incompleteFontAtlas->id, id, textureName);
				std::shared_ptr<FontAtlas> fontAtlas = std::static_pointer_cast<FontAtlas>(*it);
				CompleteFontAtlas(incompleteFontAtlas->id, fontAtlas);
			}
			break;
			case avs::GeometryPayloadType::Material:
			{
				std::shared_ptr<IncompleteMaterial> incompleteMaterial = std::static_pointer_cast<IncompleteMaterial>(*it);
				// Replacing this nonsense:
				// incompleteMaterial->textureSlots.at(id) = scrTexture;
				// with the correct:
				if (incompleteMaterial->materialInfo.diffuse.texture_uid == id) incompleteMaterial->materialInfo.diffuse.texture = scrTexture;
				if (incompleteMaterial->materialInfo.normal.texture_uid == id) incompleteMaterial->materialInfo.normal.texture = scrTexture;
				if (incompleteMaterial->materialInfo.combined.texture_uid == id) incompleteMaterial->materialInfo.combined.texture = scrTexture;
				if (incompleteMaterial->materialInfo.emissive.texture_uid == id) incompleteMaterial->materialInfo.emissive.texture = scrTexture;
				TELEPORT_INTERNAL_COUT(Resource, 
					"Waiting Material {0}({1}) got Texture {2}({3})", incompleteMaterial->id, incompleteMaterial->materialInfo.name, id, textureName);

				// If only this texture and this function are pointing to the material, then it is complete.
				if (RESOURCE_IS_COMPLETE(*it))
				{
					CompleteMaterial(incompleteMaterial->id, incompleteMaterial->materialInfo);
				}
				else
				{
					TELEPORT_INTERNAL_COUT(Resource, " Still awaiting {0} resources.", RESOURCES_AWAITED(*it));
				}
			}
			break;
			case avs::GeometryPayloadType::Node:
			{
				std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
				size_t num_remaining				 = RESOURCES_AWAITED(incompleteNode);
				RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Texture {2}({3}), missing {4} or {5}",
										   incompleteNode->id,
										   incompleteNode->name.c_str(),
										   id,
										   textureName,
										   num_remaining,
										   incompleteNode->GetMissingResourceCount());

				// If only this material and function are pointing to the MeshNode, then it is complete.
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
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteFontAtlas(avs::uid id, std::shared_ptr<clientrender::FontAtlas> fontAtlas)
{
	fontAtlas->fontTexture = mTextureManager.Get(fontAtlas->font_texture_uid);
	// If the font atlas wasn't sent via a url it may not have one.
	if (fontAtlas->url.length() == 0)
	{
		std::string name = std::string(fontAtlas->fontTexture->getName());
		std::replace(name.begin(), name.end(), '.', '#');
		fontAtlas->url = name + "_atlas";
	}
	SaveResource(*fontAtlas);
	// Was this resource being awaited?
	MissingResource* missingResource = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::FontAtlas);
	if(missingResource)
	for(auto it = missingResource->waitingResources.begin(); it != missingResource->waitingResources.end(); it++)
	{
		if(it->get()->type!=avs::GeometryPayloadType::TextCanvas)
		{
			TELEPORT_CERR<<"Waiting resource is not a TextCanvas, it's "<<int(it->get()->type)<<std::endl;
			continue;
		}
		std::shared_ptr<IncompleteTextCanvas> incompleteTextCanvas = std::static_pointer_cast<IncompleteTextCanvas>(*it);
		incompleteTextCanvas->missingFontAtlasUid=0;
		std::shared_ptr<TextCanvas> textCanvas = std::static_pointer_cast<TextCanvas>(*it);
		textCanvas->SetFontAtlas(fontAtlas);
		CompleteTextCanvas(textCanvas->id);
		RESOURCECREATOR_DEBUG_COUT( "Waiting TextCanvas {0}({1}) got FontAtlas {2}({3})" , incompleteTextCanvas->id,"",id,"");
		// The TextCanvas is complete
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteTextCanvas(avs::uid id)
{
	std::shared_ptr<clientrender::TextCanvas> textCanvas=mTextCanvasManager.Get(id);
	// Was this resource being awaited?
	MissingResource* missingResource = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::TextCanvas);
	if(missingResource)
		for (auto waiting = missingResource->waitingResources.begin(); waiting != missingResource->waitingResources.end(); waiting++)
	{
		if (waiting->get()->type != avs::GeometryPayloadType::Node)
		{
			TELEPORT_INTERNAL_CERR("Waiting resource is not a node, it's {}" , int(waiting->get()->type));
			continue;
		}
		std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*waiting);
		incompleteNode->SetTextCanvas(textCanvas);
		RESOURCE_RECEIVES(incompleteNode, textCanvas->textCanvasCreateInfo.uid);
		// modified "material" - add to transparent list.
		mNodeManager.NotifyModifiedMaterials(incompleteNode);

		size_t num_remaining = RESOURCES_AWAITED(incompleteNode);
		RESOURCECREATOR_DEBUG_COUT("Waiting Node {0}({1}) got Canvas {2}, still awaiting {3}", incompleteNode->id, incompleteNode->name, textCanvas->textCanvasCreateInfo.uid, num_remaining);
		//If the waiting resource has no incomplete resources, it is now itself complete.
		if (RESOURCE_IS_COMPLETE((*waiting)))
		{
			CompleteNode(incompleteNode->id, incompleteNode);
		}
	}
	//Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(textCanvas->textCanvasCreateInfo.uid);
}

avs::uid GeometryCache::GetRootCacheUid() const
{
	// -1 is the "no parent" value Renderer.cpp tests for; 0 and self-parent are the others.
	// The depth bound is belt and braces against a malformed tree: a cycle here would hang
	// every texture registration.
	const GeometryCache *c = this;
	for (int depth = 0; depth < 32; depth++)
	{
		avs::uid p = c->parent_cache_uid;
		if (!p || int64_t(p) == int64_t(-1) || p == c->cache_uid)
		{
			break;
		}
		std::shared_ptr<GeometryCache> parent = GetGeometryCache(p);
		if (!parent)
		{
			break;
		}
		c = parent.get();
	}
	return c->cache_uid;
}

//! The last path element of a url, without any query string: the filename the server was asked for.
static std::string TextureFilenameOfUrl(const std::string &url)
{
	size_t		end	  = url.find_first_of("?#");
	std::string path  = (end == std::string::npos) ? url : url.substr(0, end);
	size_t		slash = path.rfind('/');
	return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

GeometryCache::UrlTexture GeometryCache::RequestTextureFromUrl(const std::string &url, GeometryDecoder *decoder, ResourceCreator *target, avs::uid preferred_uid,
															   platform::crossplatform::AxesStandard sourceAxesStandard)
{
	UrlTexture result;
	if (url.empty())
	{
		return result;
	}
	// The registry is the root's, and the root is the only cache that ever holds a texture named
	// by a url: a sub-scene neither outlives the session nor is visible to its siblings, so a
	// texture kept there could not be shared and could not be found again.
	std::shared_ptr<GeometryCache> root = GetGeometryCache(GetRootCacheUid());
	if (!root)
	{
		return result;
	}

	bool	 issueFetch = false;
	avs::uid aliasUid	= 0;
	{
		std::lock_guard	 g	   = std::lock_guard(root->textureUrlsMutex);
		TextureUrlEntry &entry = root->texturesByUrl[url];
		if (!entry.texture_uid)
		{
			// The server's id where it named the file, and one of our own where it has not: this url
			// came out of an asset, and nothing has numbered it.
			entry.texture_uid						 = preferred_uid ? preferred_uid : nextLocalResourceUid++;
			root->textureUrlsByUid[entry.texture_uid] = url;
			root->NoteTextureFilename(url);
		}
		else if (preferred_uid && preferred_uid != entry.texture_uid)
		{
			// An asset reached this file first and gave it an id of our own making; the server has
			// now named it too, and its resources refer to it by an id of the server's making. Both
			// must find the one texture, so the second id becomes another name for it.
			if (std::find(entry.aliasUids.begin(), entry.aliasUids.end(), preferred_uid) == entry.aliasUids.end())
			{
				entry.aliasUids.push_back(preferred_uid);
				root->textureUrlsByUid[preferred_uid] = url;
				aliasUid							 = preferred_uid;
			}
		}
		result.uid = entry.texture_uid;
		// One fetch per url, however many assets name it and whether the url arrived as a
		// TexturePointer or was resolved out of a glb's images.
		if (!entry.fetchIssued && decoder && target)
		{
			entry.fetchIssued = true;
			issueFetch		  = true;
		}
	}
	result.texture = root->mTextureManager.Get(result.uid);
	if (result.texture)
	{
		if (aliasUid)
		{
			TELEPORT_INTERNAL_COUT(Resource, "Texture {0} from {1} is also resource {2}", result.uid, url, aliasUid);
			root->mTextureManager.Add(aliasUid, result.texture);
		}
		return result;
	}
	if (issueFetch)
	{
		TELEPORT_INTERNAL_COUT(Resource, "Texture {0} fetched from {1}", result.uid, url);
		// Against the root, so the texture is created there. The url is already absolute -
		// AbsoluteResourceUrl and ResolveUrl both guarantee it - which matters because a sub-scene
		// cache's default url root is the whole url of the asset itself.
		decoder->decodeFromWeb(root->cache_uid, url, avs::GeometryPayloadType::Texture, target, result.uid, sourceAxesStandard);
	}
	return result;
}

//! One file offered at several urls is fetched, decoded and uploaded once per url, because the url
//! is the texture's identity here and in the http cache. Nothing can be shared in that case, so say
//! so: it is a server-side duplication, and this is what makes it visible. Once per filename, not
//! once per url - the point is to count the files affected, and a scene may name the same file from
//! dozens of assets. Called with textureUrlsMutex held.
void GeometryCache::NoteTextureFilename(const std::string &url)
{
	const std::string filename = TextureFilenameOfUrl(url);
	if (filename.empty())
	{
		return;
	}
	TextureFilenameEntry &seen = textureFilenames[filename];
	if (seen.firstUrl.empty())
	{
		seen.firstUrl = url;
	}
	else if (seen.firstUrl != url && !seen.warned)
	{
		seen.warned = true;
		TELEPORT_WARN("Texture {0} is being fetched from a second url {1}; it was already fetched from {2}. "
					  "The server should serve one url per file.",
					  filename, url, seen.firstUrl);
	}
}

std::shared_ptr<clientrender::Texture> GeometryCache::BindTextureUrlToMaterial(const std::string &url, std::shared_ptr<IncompleteMaterial> material, MaterialSlot slot)
{
	if (url.empty() || !material)
	{
		return nullptr;
	}
	std::shared_ptr<GeometryCache> root = GetGeometryCache(GetRootCacheUid());
	if (!root)
	{
		return nullptr;
	}
	// Lock order throughout is missing-resources then texture-urls, never the reverse:
	// PublishTextureToUrlWaiters releases the url lock before it completes any material.
	std::lock_guard missingLock(missingResourcesMutex);
	std::lock_guard urlLock(root->textureUrlsMutex);
	auto			e = root->texturesByUrl.find(url);
	if (e == root->texturesByUrl.end())
	{
		// RequestTextureFromUrl always creates the entry, so this is a caller that never asked.
		TELEPORT_WARN_NOSPAM("Material {0} named texture url {1}, which was never requested.", material->id, url);
		return nullptr;
	}
	// Taken while holding the url lock, which is what makes this atomic against arrival: the
	// publisher adds the texture to the manager first and takes this lock afterwards, so either we
	// see the texture here, or we are in the waiting list before it is drained.
	std::shared_ptr<clientrender::Texture> texture = root->mTextureManager.Get(e->second.texture_uid);
	if (texture)
	{
		return texture;
	}
	std::vector<MaterialSlot> &slots = material->missingTextureUrls[url];
	const bool				   first = slots.empty();
	slots.push_back(slot);
	if (first)
	{
		RESOURCE_AWAITS(material, 0);
		e->second.waiting.push_back({cache_uid, material});
	}
	return nullptr;
}

void GeometryCache::PublishTextureToUrlWaiters(avs::uid id, std::shared_ptr<clientrender::Texture> texture)
{
	// Only the root holds the registry, and only a texture whose identity is a url has waiters.
	std::string					  url;
	std::vector<TextureUrlWaiter> waiting;
	std::vector<avs::uid>		  aliasUids;
	{
		std::lock_guard g(textureUrlsMutex);
		auto			u = textureUrlsByUid.find(id);
		if (u == textureUrlsByUid.end())
		{
			return;
		}
		url	   = u->second;
		auto e = texturesByUrl.find(url);
		if (e == texturesByUrl.end())
		{
			return;
		}
		// Taken and cleared under the lock: each waiter is satisfied exactly once, and a material
		// created after this point finds the texture directly rather than joining a stale list.
		waiting = std::move(e->second.waiting);
		e->second.waiting.clear();
		aliasUids = e->second.aliasUids;
	}
	// The other ids this one file must answer to; see RequestTextureFromUrl. All within this cache,
	// so this is one texture under several names, not a copy.
	for (avs::uid alias : aliasUids)
	{
		if (alias != id && !mTextureManager.Has(alias))
		{
			mTextureManager.Add(alias, texture);
			CompleteResourcesWaitingForTexture(alias, texture, std::string(texture->getName()));
		}
	}
	for (const TextureUrlWaiter &w : waiting)
	{
		std::shared_ptr<GeometryCache> waitingCache = GetGeometryCache(w.cache_uid);
		if (!waitingCache)
		{
			// Its cache went away while the bytes were in flight; there is nothing to complete.
			continue;
		}
		waitingCache->GiveTextureToMaterial(w.material, url, texture);
	}
}

void GeometryCache::GiveTextureToMaterial(std::shared_ptr<IncompleteMaterial> material, const std::string &url, std::shared_ptr<clientrender::Texture> texture)
{
	if (!material)
	{
		return;
	}
	std::lock_guard g(missingResourcesMutex);
	auto			slots = material->missingTextureUrls.find(url);
	if (slots == material->missingTextureUrls.end())
	{
		// Already satisfied - two caches both publishing, or a retry after a failure.
		return;
	}
	if (texture)
	{
		for (MaterialSlot slot : slots->second)
		{
			switch (slot)
			{
			case MaterialSlot::Diffuse:
				material->materialInfo.diffuse.texture = texture;
				break;
			case MaterialSlot::Normal:
				material->materialInfo.normal.texture = texture;
				break;
			case MaterialSlot::Combined:
				material->materialInfo.combined.texture = texture;
				break;
			case MaterialSlot::Emissive:
				material->materialInfo.emissive.texture = texture;
				break;
			}
		}
		TELEPORT_INTERNAL_COUT(Resource, "Waiting Material {0}({1}) got Texture {2} from {3}",
							   material->id, material->materialInfo.name, std::string(texture->getName()), url);
	}
	else
	{
		// The fetch failed. The slots keep the dummy textures AddTextureToMaterial gave them, which
		// is a visibly wrong material rather than one that never appears at all.
		TELEPORT_WARN_NOSPAM("Material {0}({1}) will render without the texture from {2}, which could not be fetched.",
							 material->id, material->materialInfo.name, url);
	}
	material->missingTextureUrls.erase(slots);
	// Awaited once per distinct url, so released once per distinct url.
	RESOURCE_RECEIVES(material, 0);
	if (RESOURCE_IS_COMPLETE(material))
	{
		CompleteMaterial(material->id, material->materialInfo);
	}
	else
	{
		TELEPORT_INTERNAL_COUT(Resource, "Material {} still awaiting {} resources.", material->materialInfo.name, RESOURCES_AWAITED(material));
	}
}

void GeometryCache::AbandonTextureUrlWaiters()
{
	std::shared_ptr<GeometryCache> root = GetGeometryCache(GetRootCacheUid());
	// A cache being destroyed may itself be the root, in which case the registry dies with it.
	if (!root || root.get() == this)
	{
		return;
	}
	std::lock_guard g(root->textureUrlsMutex);
	for (auto &entry : root->texturesByUrl)
	{
		auto &waiting = entry.second.waiting;
		waiting.erase(std::remove_if(waiting.begin(), waiting.end(), [this](const TextureUrlWaiter &w) { return w.cache_uid == cache_uid; }), waiting.end());
	}
}

void GeometryCache::FailTextureUrl(const std::string &url)
{
	if (url.empty())
	{
		return;
	}
	std::shared_ptr<GeometryCache> root = GetGeometryCache(GetRootCacheUid());
	if (!root)
	{
		return;
	}
	std::vector<TextureUrlWaiter> waiting;
	{
		std::lock_guard g(root->textureUrlsMutex);
		auto			e = root->texturesByUrl.find(url);
		if (e == root->texturesByUrl.end())
		{
			return;
		}
		waiting = std::move(e->second.waiting);
		e->second.waiting.clear();
		// Left unclaimed, so a later reference to this url tries again rather than waiting on a
		// fetch that already failed. The uid is kept, so anything already holding it stays valid.
		e->second.fetchIssued = false;
	}
	TELEPORT_WARN_NOSPAM("Texture url {0} could not be fetched; releasing {1} waiting material(s).", url, waiting.size());
	for (const TextureUrlWaiter &w : waiting)
	{
		std::shared_ptr<GeometryCache> waitingCache = GetGeometryCache(w.cache_uid);
		if (waitingCache)
		{
			waitingCache->GiveTextureToMaterial(w.material, url, nullptr);
		}
	}
}

void GeometryCache::SetExternalTextureUrls(const std::map<avs::uid, std::string> &urls)
{
	std::lock_guard g(textureUrlsMutex);
	for (const auto &u : urls)
	{
		externalTextureUrls[u.first] = u.second;
	}
}

std::string GeometryCache::GetExternalTextureUrl(avs::uid placeholder_id) const
{
	if (!placeholder_id)
	{
		return std::string();
	}
	std::lock_guard g(textureUrlsMutex);
	auto			i = externalTextureUrls.find(placeholder_id);
	return (i == externalTextureUrls.end()) ? std::string() : i->second;
}


std::string GeometryCache::URLToFilePath(std::string url)
{
	if (url.length() == 0) return "";
	using namespace std::filesystem;
	size_t protocol_end	 = url.find("://");
	std::string filepath = url.substr(protocol_end + 3, url.length() - protocol_end - 3);
	size_t first_slash	 = filepath.find("/");
	if (first_slash >= filepath.length()) first_slash = filepath.length();
	std::string base_url = filepath.substr(0, first_slash);
	filepath			 = filepath.substr(first_slash, filepath.length() - first_slash);
	size_t colon_pos	 = base_url.find(":");
	if (colon_pos < base_url.length()) base_url = base_url.substr(0, colon_pos);
	filepath = base_url + filepath;
	// TODO: check path length is not too long.
	return filepath;
}

bool GeometryCache::SaveResource(const IncompleteResource &res)
{
	std::string filename = URLToFilePath(res.url);
	if (filename == "") return false;
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader) return false;
	using namespace std::filesystem;
	filename += ".";
	filename += res.GetFileExtension();
	std::string f = saveFolder.length() ? (saveFolder + "/") + filename : filename;
	path fullPath = path(f);
	try
	{
		std::filesystem::create_directories(fullPath.parent_path());
	}
	catch (...)
	{
	}
	std::stringstream s;
	res.Save(s);
	std::string buffer = s.str();
	fileLoader->Save(buffer.c_str(), (uint32_t)buffer.length(), f.c_str(), false);
	return true;
}

void GeometryCache::CompleteAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation)
{
	RESOURCECREATOR_DEBUG_COUT("CompleteAnimation {0}({1})", id, animation->name);

	// Update animation length before adding to the animation manager.
	animation->updateAnimationLength();
	mAnimationManager.Add(id, animation);

	std::lock_guard g(missingResourcesMutex);
	// Add animation to waiting nodes.
	MissingResource *missingAnimation = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Animation);
	if (missingAnimation)
	{
		for (auto it = missingAnimation->waitingResources.begin(); it != missingAnimation->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got Animation {2}({3})", incompleteNode->id, incompleteNode->name, id, animation->name);

			auto animC = incompleteNode->GetOrCreateComponent<AnimationComponent>();
			//animC->addAnimation(id, animation);
			// If it is complete...
			if (RESOURCE_IS_COMPLETE(incompleteNode))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
		}
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteMaterial(avs::uid id, const clientrender::Material::MaterialCreateInfo &materialInfo)
{
	//TELEPORT_INTERNAL_COUT("CompleteMaterial {0} ({1})", id, materialInfo.name);
	// RESOURCECREATOR_DEBUG_COUT( "CompleteMaterial {0}({1})",id,materialInfo.name);
	std::shared_ptr<clientrender::Material> material = mMaterialManager.Get(id);
	if (!material)
	{
		TELEPORT_INTERNAL_CERR("Trying to complete material {0}, but it hasn't been created.\n", id);
		return;
	}
	// Update its properties:
	material->SetMaterialCreateInfo(materialInfo);
	// Add material to nodes waiting for material.
	MissingResource *missingMaterial = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Material);
	if (missingMaterial)
	{
		for (auto it = missingMaterial->waitingResources.begin(); it != missingMaterial->waitingResources.end(); it++)
		{
			std::shared_ptr<Node> incompleteNode = std::static_pointer_cast<Node>(*it);

			const auto &indexesPair = incompleteNode->materialSlots.find(id);
			if (indexesPair == incompleteNode->materialSlots.end())
			{
				TELEPORT_INTERNAL_CERR("Material {} not found in incomplete node {}", id, incompleteNode->id);
				continue;
			}
			for (size_t materialIndex : indexesPair->second)
			{
				incompleteNode->SetMaterial(materialIndex, material);
			}
			size_t num_remaining = RESOURCES_AWAITED(incompleteNode);
			RESOURCE_RECEIVES(incompleteNode, id);
			RESOURCECREATOR_DEBUG_COUT("Waiting MeshNode {0}({1}) got Material {2}({3}) - missing {4} or {5}",
									   incompleteNode->id,
									   incompleteNode->name,
									   id,
									   materialInfo.name,
									   num_remaining,
									   incompleteNode->GetMissingResourceCount());

			// If only this material and function are pointing to the MeshNode, then it is complete.
			if (RESOURCE_IS_COMPLETE(incompleteNode))
			{
				CompleteNode(incompleteNode->id, incompleteNode);
			}
			mNodeManager.NotifyModifiedMaterials(incompleteNode);
		}
	}
	// Resource has arrived, so we are no longer waiting for it.
	RemoveFromMissingResources(id);
}

void GeometryCache::CompleteNode(avs::uid id, std::shared_ptr<clientrender::Node> node)
{
	//	TELEPORT_INTERNAL_CERR( "CompleteNode {0} {1}",id,node->name);
	/// We're using the node ID as the node ID as we are currently generating an node per node/transform anyway; this way the server can tell the client to
	/// remove an node.
	m_CompletedNodes.push_back(id);
	MissingResource *missingNode = GetMissingResourceIfMissing(id, avs::GeometryPayloadType::Node);
	if (missingNode)
	{
		for (auto waiting = missingNode->waitingResources.begin(); waiting != missingNode->waitingResources.end(); waiting++)
		{
			if (waiting->get()->type == avs::GeometryPayloadType::Node)
			{
				std::shared_ptr<Node> waitingNode = std::static_pointer_cast<Node>(*waiting);
				if (waitingNode->id == id)
				{
					TELEPORT_INTERNAL_CERR("Node {0} is waiting for itself", id, id);
					break;
				}
				TELEPORT_INTERNAL_CERR("Waiting Mesh Node {0} got Skeleton Node {1}", waitingNode->id, id);
				if (waitingNode)
				{
					RESOURCE_RECEIVES(waitingNode, id);
					waitingNode->SetSkeletonNode(node);
					// If the waiting resource has no incomplete resources, it is now itself complete.
					if (waitingNode->GetMissingResourceCount() == 0)
					{
						CompleteNode(waitingNode->id, waitingNode);
					}
				}
			}
		}
	}
	RemoveFromMissingResources(id);
	mNodeManager.CompleteNode(id);
}

void GeometryCache::AddTextureToMaterial(const avs::TextureAccessor &accessor,
										 const vec4 &colourFactor,
										 const std::shared_ptr<clientrender::Texture> &dummyTexture,
										 std::shared_ptr<IncompleteMaterial> incompleteMaterial,
										 MaterialSlot slot,
										 clientrender::Material::MaterialParameter &materialParameter)
{
	materialParameter.texture_uid = 0;
	materialParameter.hasTexture  = (accessor.index != 0);
	if (accessor.index != 0)
	{
		// A texture the asset referenced as a separate file is identified by the url it comes from,
		// and is held by the session's cache rather than this one - accessor.index is then only the
		// decoder's label for it, not a resource here. Everything else is an ordinary resource of
		// this cache, found by uid. See GeometryCache::RequestTextureFromUrl.
		const std::string url = GetExternalTextureUrl(accessor.index);
		if (!url.empty())
		{
			// The dummy stands in until the real texture arrives, and stays if it never does.
			materialParameter.texture						  = dummyTexture;
			std::shared_ptr<clientrender::Texture> urlTexture = BindTextureUrlToMaterial(url, incompleteMaterial, slot);
			if (urlTexture)
			{
				materialParameter.texture = urlTexture;
			}
		}
		else
		{
			materialParameter.texture_uid						= accessor.index;
			const std::shared_ptr<clientrender::Texture> texture = mTextureManager.Get(accessor.index);

			if (texture)
			{
				materialParameter.texture = texture;
			}
			else
			{
				if (incompleteMaterial->missingTextureUids.find(accessor.index) == incompleteMaterial->missingTextureUids.end())
				{
					clientrender::MissingResource &missing = GetMissingResource(accessor.index, avs::GeometryPayloadType::Texture);
					missing.waitingResources.insert(incompleteMaterial);
					RESOURCE_AWAITS(incompleteMaterial, accessor.index);
					incompleteMaterial->missingTextureUids.insert(accessor.index);
				}
			}
		}

		vec2 tiling = {accessor.tiling.x, accessor.tiling.y};

		materialParameter.texCoordsScale = tiling;
		materialParameter.texCoordIndex	 = (int)accessor.texCoord;
	}
	else
	{
		materialParameter.texture		 = dummyTexture;
		materialParameter.texCoordsScale = vec2(1.0f, 1.0f);
		materialParameter.texCoordIndex	 = 0;
	}

	materialParameter.textureOutputScalar = *((vec4 *)&colourFactor);
}
