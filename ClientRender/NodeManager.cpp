#include "NodeManager.h"

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
	//ECS_COMPONENT_DEFINE(flecs_world, flecs_pos);
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
		distanceSortedRootNodes.push_back(node);
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
		auto animationIt = earlyAnimationUpdates.find(node_id);
		if (animationIt != earlyAnimationUpdates.end())
		{
			auto animC=node->GetOrCreateComponent<AnimationComponent>();
			//animC->setAnimationState(session_time_us, animationIt->second);
			earlyAnimationUpdates.erase(animationIt);
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
			distanceSortedRootNodes.push_back(child);
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
	for(size_t i=0;i<distanceSortedRootNodes.size();i++)
	{
		if(distanceSortedRootNodes[i].expired())
			distanceSortedRootNodes.erase(distanceSortedRootNodes.begin()+i);
	}
	std::sort
	(
		distanceSortedRootNodes.begin(),
		distanceSortedRootNodes.end(),
		[](std::weak_ptr<Node> a, std::weak_ptr<Node> b)
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

void NodeManager::UpdateNodeAnimation(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate)
{
	std::shared_ptr<Node> node = GetNode(animationUpdate.nodeID);
	if(node)
	{
		auto animC=node->GetOrCreateComponent<AnimationComponent>();
		animC->setAnimationState(timestampUs,animationUpdate, 0);
	}
	else
	{
		std::lock_guard<std::mutex> lock(early_mutex);
		earlyAnimationUpdates[animationUpdate.nodeID] = animationUpdate;
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
	node->SetLocalPosition(updateNodeStructureCommand.relativePose.position);
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