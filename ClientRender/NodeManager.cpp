#include "NodeManager.h"

#include "GeometryCache.h"
#include "NodeComponents/AnimationComponent.h"
#include "NodeComponents/SubSceneComponent.h"
#include "TeleportCore/Logging.h"

#include <format>

using namespace teleport;
using namespace clientrender;
using teleport::core::Pose;

using InvisibilityReason = VisibilityComponent::InvisibilityReason;

template<typename T> auto find( std::vector<std::weak_ptr<T>> &v, std::weak_ptr<T> &p)
{
	auto f=std::find_if(v.begin(), v.end(),  [&p](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == p;
			});
	return f;
}
template<typename T> auto find(const std::vector<std::weak_ptr<T>> &v,const std::weak_ptr<T> &p)
{
	auto f=std::find_if(v.begin(), v.end(),  [&p](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == p;
			});
	return f;
}
template<typename T> auto find( std::vector<std::weak_ptr<T>> &v, std::shared_ptr<T> &p)
{
	auto f=std::find_if(v.begin(), v.end(),  [&p](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == p;
			});
	return f;
}

NodeManager::NodeManager()
{
}

std::shared_ptr<Node> NodeManager::CreateNode(std::chrono::microseconds session_time_us,avs::uid id, const avs::Node &avsNode)
{
	std::shared_ptr<Node> node= std::make_shared<Node>(id, avsNode.name);

	//Create MeshNode even if it is missing resources
	AddNode(session_time_us,node, avsNode);
	return node;
}

void NodeManager::AddNode(std::chrono::microseconds session_time_us,std::shared_ptr<Node> node, const avs::Node &avsNode)
{
	using teleport::core::Pose;
	{
		rootNodes_mutex.lock();
		rootNodes.push_back(node);
		rootNodes_mutex.unlock();
		{
			std::scoped_lock lock(distanceSortedRootNodes_mutex);
			distanceSortedRootNodes.push_back(node);
		}
	}
	// Should not do this on the main thread, or any perf-critical thread.
	while(!nodeLookup_mutex.try_lock())
	{
	}
	nodeLookup[node->id] = node;
	avs::uid node_id = node->id;

	nodeLookup_mutex.unlock();
	if(avsNode.parentID)
	{
		parentLookup[node_id]=avsNode.parentID;
		childLookup[avsNode.parentID].insert(node_id);
	}
	//Link new node to parent.
	LinkToParentNode(node);

	// is this a missing parent of a child?
	auto previous_children=childLookup.find(node_id);
	if(previous_children!=childLookup.end())
	{
		for(auto childID : previous_children->second)
		{
			parentLookup[childID] = node_id;
			auto n = nodeLookup.find(childID);
			if(n!= nodeLookup.end())
				LinkToParentNode(n->second);
		}
	}
	childLookup.erase(node_id);

	//Set last movement, if a movement update was received early.
	{
		std::lock_guard<std::mutex> lock(early_mutex);
		auto movementIt = earlyMovements.find(node_id);
		if (movementIt != earlyMovements.end())
		{
			node->SetLastMovement(movementIt->second);
			earlyMovements.erase(movementIt);
		}

		//Set enabled state, if a enabled state update was received early.
		auto enabledIt = earlyEnabledUpdates.find(node_id);
		if (enabledIt != earlyEnabledUpdates.end())
		{
			node->visibility.setVisibility(enabledIt->second.enabled, InvisibilityReason::DISABLED);
			earlyEnabledUpdates.erase(enabledIt);
		}

		//Set correct highlighting for node, if a highlight update was received early.
		auto highlightIt = earlyNodeHighlights.find(node_id);
		if (highlightIt != earlyNodeHighlights.end())
		{
			node->SetHighlighted(highlightIt->second);
			earlyNodeHighlights.erase(highlightIt);
		}
		
		//Set playing animation, if an animation update was received early.
		// Left queued if it still cannot be applied - a node that roots a sub-scene has nothing to
		// animate until that sub-scene has been built. Update() retries it.
		auto animationIt = earlyAnimationUpdates.find(node_id);
		if (animationIt != earlyAnimationUpdates.end())
		{
			if (ApplyAnimationToNode(session_time_us, node, animationIt->second))
			{
				earlyAnimationUpdates.erase(animationIt);
				animationUpdateQueuedUs.erase(node_id);
				animationUpdatesWarned.erase(node_id);
			}
		}

		//Set playing animation, if an animation control update was received early.
		auto animationControlIt = earlyAnimationControlUpdates.find(node_id);
		if (animationControlIt != earlyAnimationControlUpdates.end())
		{
			for (const EarlyAnimationControl& earlyControlUpdate : animationControlIt->second)
			{
				//animC->setAnimationTimeOverride(earlyControlUpdate.animationID, earlyControlUpdate.timeOverride, earlyControlUpdate.overrideMaximum);
			}
			earlyAnimationControlUpdates.erase(animationControlIt);
		}

		//Set animation speed, if an animation speed update was received early.
		auto animationSpeedIt = earlyAnimationSpeedUpdates.find(node_id);
		if (animationSpeedIt != earlyAnimationSpeedUpdates.end())
		{
			for (const EarlyAnimationSpeed& earlySpeedUpdate : animationSpeedIt->second)
			{
				//animC->setAnimationSpeed(earlySpeedUpdate.animationID, earlySpeedUpdate.speed);
			}
			earlyAnimationSpeedUpdates.erase(animationSpeedIt);
		}
	}

	node->SetJointIndices(avsNode.joint_indices);

	node->SetInverseBindMatrices(avsNode.inverseBindMatrices);

	node->SetLocalTransform(static_cast<Transform>(avsNode.localTransform));
	
	// Must do BEFORE SetMaterialListSize because that instantiates the damn mesh for some reason.
	node->SetLightmapScaleOffset(avsNode.renderState.lightmapScaleOffset);
	node->SetMaterialListSize(avsNode.materials.size());
	node->SetStatic(avsNode.stationary);
	
	node->SetHolderClientId(avsNode.holder_client_id);
	node->SetPriority(avsNode.priority);
	node->SetGlobalIlluminationTextureUid(avsNode.renderState.globalIlluminationUid);

	if(avsNode.url.length())
		node->AddLink(avsNode.url);
}

void NodeManager::NotifyModifiedMaterials(std::shared_ptr<Node> node)
{
	std::weak_ptr<Node> n=node;
	nodesWithModifiedMaterials.insert(n);
}

void NodeManager::NotifyModifiedRendering(std::shared_ptr<Node> node)
{
	std::weak_ptr<Node> n = node;
	//nodesWithModifiedRendering.insert(n);
	//updateNodeInRender(node);
}

void NodeManager::RemoveNode(std::shared_ptr<Node> node)
{
	if(!node)
		return;
	removeNodeFromRender(node->id);
	nodeLookup_mutex.lock();
	//Remove node from parent's child list.
	if(!node->GetParent().expired())
	{
		std::shared_ptr<Node> parent = node->GetParent().lock();
		if(parent)
			parent->RemoveChild(node);
	}
	//Remove from root nodes, if the node had no parent.
	else
	{
		rootNodes_mutex.lock();
		auto f = find(rootNodes, node);
		rootNodes.erase(f);
		rootNodes_mutex.unlock();
		std::weak_ptr<clientrender::Node> wn=node;
		//auto d=std::find(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), wn);
		//distanceSortedRootNodes.erase(d);
	}
	// If it's in the transparent list, erase it from there.
	{
		std::scoped_lock l(distanceSortedTransparentNodes_mutex);
		auto f = find(distanceSortedTransparentNodes,  node);
		if (f != distanceSortedTransparentNodes.end())
			distanceSortedTransparentNodes.erase(f);
	}

	//Attach children to world root.
	std::vector<std::weak_ptr<Node>> children = node->GetChildren();
	for (std::weak_ptr<Node> childPtr : children)
	{
		std::shared_ptr<Node> child = childPtr.lock();
		if (child)
		{
			rootNodes_mutex.lock();
			rootNodes.push_back(child);
			rootNodes_mutex.unlock();
			{
				std::scoped_lock lock(distanceSortedRootNodes_mutex);
				distanceSortedRootNodes.push_back(child);
			}
			//Remove parent
			child->SetParent(nullptr);
			parentLookup.erase(child->id);
		}
	}

	//Remove from node lookup table.
	nodeLookup.erase(node->id);
	nodeLookup_mutex.unlock();
}

void NodeManager::RemoveNode(avs::uid nodeID)
{
	std::shared_ptr<Node> node;
	{
		std::lock_guard<std::mutex> lock(nodeLookup_mutex);
		auto nodeIt = nodeLookup.find(nodeID);
		if (nodeIt != nodeLookup.end())
		{
			node=nodeIt->second;
		}
	}
	if(node)
		RemoveNode(node);
}

bool NodeManager::HasNode(avs::uid nodeID) const
{
	std::lock_guard<std::mutex> lock(nodeLookup_mutex);
	return nodeLookup.find(nodeID) != nodeLookup.end();
}

std::shared_ptr<Node> NodeManager::GetNode(avs::uid nodeID) const
{
	std::lock_guard<std::mutex> lock(nodeLookup_mutex);
	auto f = nodeLookup.find(nodeID);
	if (f == nodeLookup.end())
	{
		return nullptr;
	}
	return f->second;
}

std::vector<std::shared_ptr<Node>> NodeManager::GetSkeletonNodes() const
{
	std::vector<std::shared_ptr<Node>> skeletonNodes;
	std::lock_guard<std::mutex> lock(nodeLookup_mutex);
	for (const auto &n : nodeLookup)
	{
		if (n.second && n.second->GetSkeleton())
		{
			skeletonNodes.push_back(n.second);
		}
	}
	return skeletonNodes;
}

size_t NodeManager::GetNodeCount() const
{
	std::lock_guard<std::mutex> lock(nodeLookup_mutex);
	return nodeLookup.size();
}

const std::vector<std::weak_ptr<Node>>& NodeManager::GetRootNodes() const
{
	return rootNodes;
}

const std::vector<std::weak_ptr<Node>>& NodeManager::GetSortedRootNodes()
{
	std::scoped_lock lock(distanceSortedRootNodes_mutex);
	for(size_t i=0;i<distanceSortedRootNodes.size();)
	{
		if(distanceSortedRootNodes[i].expired())
			distanceSortedRootNodes.erase(distanceSortedRootNodes.begin()+i);
		else
			i++;
	}
	std::sort
	(
		distanceSortedRootNodes.begin(),
		distanceSortedRootNodes.end(),
		[](const std::weak_ptr<Node> &a, const std::weak_ptr<Node> &b)
		{
			auto A = a.lock();
			auto B = b.lock();
			if(!A||!B)
				return false;
			return A->distance < B->distance;
		}
	);

	return distanceSortedRootNodes;
}

const std::vector<std::weak_ptr<Node>>& NodeManager::GetSortedTransparentNodes()
{
	std::set<std::weak_ptr<Node>>::iterator n=nodesWithModifiedMaterials.begin();
	while(n!=nodesWithModifiedMaterials.end())
	{
		auto N=n->lock();
		if(!N)
			break;
		const auto &m=N->GetMaterials();
		bool transparent=false;
		bool unknown=false;
		for(const auto &M:m)
		{
			if(!M)
				unknown=true;
			if(M->GetMaterialCreateInfo().materialMode==avs::MaterialMode::TRANSPARENT_MATERIAL)
				transparent=true;
		}
		if(N->GetTextCanvas())
			transparent=true;
		if(!unknown)
		{
			if(transparent)
			{
				bool alreadyPresent=false;
				std::scoped_lock l(distanceSortedTransparentNodes_mutex);
				for(auto i=distanceSortedTransparentNodes.begin();i!=distanceSortedTransparentNodes.end();i++)
				{
					if(i->lock()==N)
					{
						alreadyPresent=true;
						break;
					}
				}
				if(!alreadyPresent)
					distanceSortedTransparentNodes.push_back(*n);
			}
			else
			{
				std::scoped_lock l(distanceSortedTransparentNodes_mutex);
				for(auto i=distanceSortedTransparentNodes.begin();i!=distanceSortedTransparentNodes.end();i++)
				{
					if(i->lock()==N)
					{
						distanceSortedTransparentNodes.erase(i);
						break;
					}
				}
			}
			nodesWithModifiedMaterials.erase(n);
			break;
		}
	}
	{
		std::scoped_lock l(distanceSortedTransparentNodes_mutex);
		std::sort
		(
			distanceSortedTransparentNodes.begin(),
			distanceSortedTransparentNodes.end(),
			[](std::weak_ptr<Node> a, std::weak_ptr<Node> b)
			{
				auto A=a.lock();
				if(!A)
				return false;
				auto B=a.lock();
				if(!B)
					return false;
				return A->distance < B->distance;
			}
		);
	}
	return distanceSortedTransparentNodes;
}

bool NodeManager::ShowNode(avs::uid nodeID)
{
	std::lock_guard lock(nodeLookup_mutex);
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		nodeIt->second->SetVisible(true);
		hiddenNodes.erase(nodeID);
		return true;
	}

	return false;
}

bool NodeManager::HideNode(avs::uid nodeID)
{
	std::lock_guard lock(nodeLookup_mutex);
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		TELEPORT_COUT<<"NodeManager::HideNode Hiding node "<<nodeID<<std::endl;
		nodeIt->second->SetVisible(false);
		hiddenNodes.insert(nodeID);
		return true;
	}

	return false;
}

void NodeManager::SetVisibleNodes(const std::vector<avs::uid> visibleNodes)
{
	//Hide all nodes.
	{
		std::lock_guard lock(nodeLookup_mutex);
		for (const auto& it : nodeLookup)
		{
			it.second->SetVisible(false);
			hiddenNodes.insert(it.first);
		}
	}

	//Show visible nodes.
	for(avs::uid id : visibleNodes)
	{
		ShowNode(id);
	}
}

bool NodeManager::UpdateNodeTransform(avs::uid nodeID, const vec3& translation, const quat& rotation, const vec3& scale)
{
	std::lock_guard lock(nodeLookup_mutex);
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		nodeIt->second->UpdateModelMatrix(translation, rotation, scale);
		return true;
	}

	return false;
}

void NodeManager::UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList)
{
	//earlyMovements.clear();

	for(teleport::core::MovementUpdate update : updateList)
	{
		std::shared_ptr<Node> node = GetNode(update.nodeID);
		if(node)
		{
			node->SetLastMovement(update);
		}
		else
		{
			std::lock_guard<std::mutex> lock(early_mutex);
			earlyMovements[update.nodeID] = update;
		}
	}
}

void NodeManager::UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList)
{
	//earlyEnabledUpdates.clear();
	for(teleport::core::NodeUpdateEnabledState update : updateList)
	{
		std::shared_ptr<Node> node = GetNode(update.nodeID);
		if(node)
		{
			node->visibility.setVisibility(update.enabled, InvisibilityReason::DISABLED);
		}
		else
		{
			std::lock_guard<std::mutex> lock(early_mutex);
			earlyEnabledUpdates[update.nodeID] = update;
		}
	}
}

void NodeManager::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	std::shared_ptr<Node> node = GetNode(nodeID);
	if(node)
	{
		node->SetHighlighted(isHighlighted);
	}
	else
	{
		std::lock_guard<std::mutex> lock(early_mutex);
		earlyNodeHighlights[nodeID] = isHighlighted;
	}
}

//! Apply an animation state to whichever node actually owns the skeleton it should drive.
//!
//! The node the server addresses is not always the node that can be animated. An avatar streamed as
//! a MeshPointer - a VRM, say - arrives as a sub-scene in a geometry cache of its own, and the node
//! the server names is only that sub-scene's root. Its skeleton, and the AnimationComponent that
//! goes with it, live on a node inside the sub-cache, which the server has never seen and cannot
//! name. Resolving that is the client's job: the protocol gives cacheID zero the meaning "the cache
//! containing nodeID", and that is the value that lets a command drive a skeleton inside a sub-scene.
//!
//! root_uid is what ties the state to the right instance. The renderer keys a sub-scene's
//! per-instance state on the outer node's uid - InstanceRenderer sets SubSceneNodeStates::root_id to
//! exactly that - so an AnimationInstance created under any other value is one nothing ever reads.
bool NodeManager::ApplyAnimationToNode(std::chrono::microseconds timestampUs, std::shared_ptr<Node> node, const teleport::core::ApplyAnimation &animationUpdate,
									   std::string *reason)
{
	auto fail = [reason](std::string why) -> bool
	{
		if (reason)
		{
			*reason = std::move(why);
		}
		return false;
	};
	if (!node)
	{
		return fail("the node has not arrived");
	}
	// Zero means "the cache containing nodeID", which is this one: that is where the node was found.
	// The clip lives there whether or not the skeleton does, so this stays the clip's cache even when
	// the state is applied to a node in a sub-cache below.
	teleport::core::ApplyAnimation resolved = animationUpdate;
	if (resolved.cacheID == 0)
	{
		resolved.cacheID = cacheUid;
	}
	// A node with its own skeleton is animated directly. root_uid 0 means "this node's own instance".
	if (node->GetSkeleton())
	{
		auto animC = node->GetOrCreateComponent<AnimationComponent>();
		animC->setAnimationState(timestampUs, resolved, 0);
		return true;
	}
	// No skeleton of its own. If it roots a sub-scene, the skeleton is in there.
	auto subSceneComponent = node->GetComponent<SubSceneComponent>();
	if (!subSceneComponent)
	{
		return fail("it has no skeleton and no sub-scene component");
	}
	// Prefer the component's own pointer, but fall back to looking the mesh up by uid, which is what
	// InstanceRenderer does and is therefore the field that is always populated.
	auto mesh = subSceneComponent->mesh;
	if (!mesh && subSceneComponent->mesh_uid)
	{
		auto ownCache = GeometryCache::GetGeometryCache(cacheUid);
		if (ownCache)
		{
			mesh = ownCache->mMeshManager.Get(subSceneComponent->mesh_uid);
		}
	}
	if (!mesh)
	{
		return fail(std::format("its sub-scene component has no mesh (mesh uid {})", subSceneComponent->mesh_uid));
	}
	avs::uid subSceneCacheUid = mesh->GetMeshCreateInfo().subscene_cache_uid;
	if (!subSceneCacheUid)
	{
		return fail(std::format("its mesh {} is not a sub-scene", subSceneComponent->mesh_uid));
	}
	auto subCache = GeometryCache::GetGeometryCache(subSceneCacheUid);
	if (!subCache)
	{
		return fail(std::format("its sub-scene cache {} does not exist", subSceneCacheUid));
	}
	// The sub-scene may still be building, in which case no node in it has a skeleton yet and the
	// caller retries. Once it has, every skeleton in the sub-scene is driven by the one command.
	auto skeletonNodes = subCache->mNodeManager.GetSkeletonNodes();
	if (!skeletonNodes.size())
	{
		return fail(std::format("none of the {} nodes in its sub-scene cache {} has a skeleton",
								subCache->mNodeManager.GetNodeCount(),
								subSceneCacheUid));
	}
	for (auto &skeletonNode : skeletonNodes)
	{
		auto animC = skeletonNode->GetOrCreateComponent<AnimationComponent>();
		animC->setAnimationState(timestampUs, resolved, node->id);
	}
	return true;
}

void NodeManager::UpdateNodeAnimation(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate)
{
	std::shared_ptr<Node> node = GetNode(animationUpdate.nodeID);
	if (ApplyAnimationToNode(timestampUs, node, animationUpdate))
	{
		return;
	}
	// The node is not here yet, or its sub-scene has not finished building. Hold on to the state and
	// retry: nothing else will tell us what to play, because the server only sends on a change.
	std::lock_guard<std::mutex> lock(early_mutex);
	earlyAnimationUpdates[animationUpdate.nodeID] = animationUpdate;
	if (animationUpdateQueuedUs.find(animationUpdate.nodeID) == animationUpdateQueuedUs.end())
	{
		animationUpdateQueuedUs[animationUpdate.nodeID] = timestampUs;
	}
}

bool NodeManager::ReparentNode(const teleport::core::UpdateNodeStructureCommand& updateNodeStructureCommand)
{
	nodeLookup_mutex.lock();
	auto c = nodeLookup.find(updateNodeStructureCommand.nodeID);
	auto p = nodeLookup.find(updateNodeStructureCommand.parentID);
	std::shared_ptr<Node> node = c != nodeLookup.end() ? c->second : nullptr;
	if (!node)
	{
		TELEPORT_CERR << "Failed to reparent node " << updateNodeStructureCommand.nodeID << " as it was not found locally.\n";
		return false;
	}
	std::shared_ptr<Node> parent = p != nodeLookup.end() ? p->second : nullptr;
	nodeLookup_mutex.unlock();
	if (updateNodeStructureCommand.parentID != 0 && !parent)
	{
		TELEPORT_CERR << "Failed to reparent node " << updateNodeStructureCommand.nodeID << " as its new parent " << updateNodeStructureCommand.parentID << " was not found locally.\n";
		return false;
	}

	if (updateNodeStructureCommand.parentID != 0)
		parentLookup[updateNodeStructureCommand.nodeID] = updateNodeStructureCommand.parentID;
	else
	{
		auto p = parentLookup.find(updateNodeStructureCommand.nodeID);
		if (p != parentLookup.end())
			parentLookup.erase(p);
	}
	std::weak_ptr<Node> oldParent = node->GetParent();
	auto oldp = oldParent.lock();
	if (oldp)
		oldp->RemoveChild(node);
	node->SetLocalPosition(unpacked(updateNodeStructureCommand.relativePose.position));
	node->SetLocalRotation(*((quat*)&updateNodeStructureCommand.relativePose.orientation));
	// TODO: Force an update. SHOULD NOT be necessary.
	node->GetGlobalTransform();
	
	LinkToParentNode(node);
	return true;
}
void NodeManager::UpdateExtrapolatedPositions(double serverTimeS)
{
	for (const std::weak_ptr<Node> node : rootNodes)
	{
		auto n=node.lock();
		if(n)
			n->UpdateExtrapolatedPositions(serverTimeS);
	}
}

void NodeManager::Update( std::chrono::microseconds timestamp_us)
{
	rootNodes_mutex.lock();
	nodeList_t expiredNodes;
	for(const std::weak_ptr<Node> node : rootNodes)
	{
		auto n=node.lock();
		if(n)
			n->Update(timestamp_us);
	}
	rootNodes_mutex.unlock();
	for(const avs::uid u : hiddenNodes)
	{
		auto n=nodeLookup.find(u);
		if(n!=nodeLookup.end())
		{
			std::shared_ptr<Node> node =n->second;
			if (node->GetTimeSinceLastVisibleS(timestamp_us) >= nodeLifetime && node->visibility.getInvisibilityReason() == InvisibilityReason::OUT_OF_BOUNDS)
			{
				expiredNodes.push_back(node);
			}
		}
	}
	removed_node_uids.clear();
	//Delete nodes that have been invisible for too long.
	for(const std::shared_ptr<Node> node : expiredNodes)
	{
		RemoveNode(node);
		removed_node_uids.insert(node->id);
	}
	RetryAnimationUpdates(timestamp_us);
}

//! Retry the animation states that could not be applied when they arrived.
//! A sub-scene avatar takes time to download and build, and the first ApplyAnimation for it routinely
//! lands before there is a skeleton to drive. The server will not repeat itself - it sends only on a
//! change of locomotion state - so without this the avatar stays in whatever it was doing until the
//! user next changes gait, and a client that missed the opening state never animates at all.
void NodeManager::RetryAnimationUpdates(std::chrono::microseconds timestamp_us)
{
	std::lock_guard<std::mutex> lock(early_mutex);
	if (!earlyAnimationUpdates.size())
	{
		return;
	}
	// Long enough that a sub-scene has had a fair chance to download and build, short enough to see
	// while the session that caused it is still running.
	static const std::chrono::microseconds pendingWarnAfter(5000000);
	for (auto it = earlyAnimationUpdates.begin(); it != earlyAnimationUpdates.end();)
	{
		std::shared_ptr<Node> node = GetNode(it->first);
		std::string			  reason;
		if (ApplyAnimationToNode(timestamp_us, node, it->second, &reason))
		{
			animationUpdateQueuedUs.erase(it->first);
			animationUpdatesWarned.erase(it->first);
			it = earlyAnimationUpdates.erase(it);
			continue;
		}
		auto queued = animationUpdateQueuedUs.find(it->first);
		if (queued != animationUpdateQueuedUs.end() && timestamp_us - queued->second > pendingWarnAfter &&
			animationUpdatesWarned.find(it->first) == animationUpdatesWarned.end())
		{
			animationUpdatesWarned.insert(it->first);
			TELEPORT_WARN("Animation {} for node {} still cannot be applied after {}s: {}.",
						  (unsigned)it->second.animationID,
						  (unsigned)it->first,
						  (timestamp_us - queued->second).count() / 1000000,
						  reason);
		}
		it++;
	}
}

const std::set<avs::uid> &NodeManager::GetRemovedNodeUids() const
{
	return removed_node_uids;
}

void NodeManager::Clear()
{
	for(auto n:nodeLookup)
		removeNodeFromRender(n.first);
	rootNodes_mutex.lock();
	rootNodes.clear();
	rootNodes_mutex.unlock();
	{
		std::scoped_lock lock(distanceSortedRootNodes_mutex);
		distanceSortedRootNodes.clear();
	}
	{
		std::scoped_lock lock(distanceSortedTransparentNodes_mutex);
		distanceSortedTransparentNodes.clear();
	}
	nodeLookup.clear();

	parentLookup.clear();

	std::lock_guard<std::mutex> lock(early_mutex);
	earlyMovements.clear();
	earlyEnabledUpdates.clear();
	earlyNodeHighlights.clear();
	earlyAnimationUpdates.clear();
	animationUpdateQueuedUs.clear();
	animationUpdatesWarned.clear();
	earlyAnimationControlUpdates.clear();
	earlyAnimationSpeedUpdates.clear();
	hiddenNodes.clear();
	nodesWithModifiedMaterials.clear();
	nodesWithModifiedRendering.clear();
}

void NodeManager::ClearAllButExcluded(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes)
{
	for (auto it = nodeLookup.begin(); it != nodeLookup.end();)
	{
		auto exclusionIt = std::find(excludeList.begin(), excludeList.end(), it->first);

		//Keep node in manager, if it is in the exclusion list.
		if (exclusionIt != excludeList.end())
		{
			excludeList.erase(exclusionIt);
			outExistingNodes.push_back(it->first);
			++it;
		}
		else
		{
			RemoveNode(it->second);
		}
	}
}

bool NodeManager::IsNodeVisible(avs::uid nodeID) const
{
	std::shared_ptr<Node> node = GetNode(nodeID);
	return node != nullptr && node->IsVisible();
}

void NodeManager::LinkToParentNode(std::shared_ptr<Node> child)
{
	//Do nothing if the child doesn't appear in the parent lookup; i.e. we have not received a parent for the node.
	auto parentIt = parentLookup.find(child->id);
	std::shared_ptr<Node> parent;
	if(parentIt != parentLookup.end())
	{
		parent = GetNode(parentIt->second);
	}

	//Connect up hierarchy.
	if (child != nullptr)
	{
		child->SetParent(parent);
		if (parent == nullptr)
		{
			rootNodes_mutex.lock();
			// put in root nodes list.
			auto r = std::find_if(rootNodes.begin(), rootNodes.end(),  [&child](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == child;
			});
			if (r == rootNodes.end())
				rootNodes.push_back(child);
			distanceSortedRootNodes_mutex.lock();
			
			auto f = std::find_if(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), [&child](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == child;
			});
			//auto f = std::find(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), child);
			if (f == distanceSortedRootNodes.end())
				distanceSortedRootNodes.push_back(child);
			distanceSortedRootNodes_mutex.unlock();
	rootNodes_mutex.unlock();
		}
	}
	//Do nothing if we couldn't find one of the nodes; likely due to the parent being removed before the child was received.
	if(parent == nullptr || child == nullptr)
	{
		return;
	}

	parent->AddChild(child);
	{
		std::scoped_lock l(distanceSortedRootNodes_mutex);
		auto f = std::find_if(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), [&child](const std::weak_ptr<Node>& ptr1) {
				return ptr1.lock() == child;
			});
		if (f != distanceSortedRootNodes.end())
			distanceSortedRootNodes.erase(f);
	}
	//Erase child from the root nodes list, as they now have a parent.
	// TODO: ONLY do this if it was unparented before.....
	
	rootNodes_mutex.lock();
	auto r=find(rootNodes, child);
	if(r!=rootNodes.end())
		rootNodes.erase(r);
	rootNodes_mutex.unlock();
}

void NodeManager::CompleteNode(avs::uid id)
{
//.	TELEPORT_COUT<<"CompleteNode "<<id<<"\n";
	addNodeForRender(id);
}