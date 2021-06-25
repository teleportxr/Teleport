#include "NodeManager.h"

using namespace scr;

using InvisibilityReason = scr::VisibilityComponent::InvisibilityReason;

std::shared_ptr<Node> NodeManager::CreateNode(avs::uid id, const std::string& name) const
{
	return std::make_shared<Node>(id, name);
}

void NodeManager::AddNode(std::shared_ptr<Node> node, const avs::DataNode& nodeData)
{
	//Remove any node already using the ID.
	RemoveNode(node->id);

	node->SetStatic(nodeData.stationary);
	if(nodeData.data_subtype == avs::NodeDataSubtype::None)
	{
		rootNodes.push_back(node);
		distanceSortedRootNodes.push_back(node);
	}
	nodeLookup[node->id] = node;

	//Link new node to parent.
	LinkToParentNode(node->id);

	//Link node's children to this node.
	for(avs::uid childID : node->GetChildrenIDs())
	{
		parentLookup[childID] = node->id;
		LinkToParentNode(childID);
	}

	switch(nodeData.data_subtype)
	{
	case avs::NodeDataSubtype::None:
		break;
	case avs::NodeDataSubtype::Body:
		SetBody(node);
		break;
	case avs::NodeDataSubtype::LeftHand:
		SetLeftHand(node);
		break;
	case avs::NodeDataSubtype::RightHand:
		SetRightHand(node);
		break;
	default:
		SCR_CERR << "Unrecognised node data sub-type: " << static_cast<int>(nodeData.data_subtype) << "!\n";
		break;
	}

	//Set last movement, if a movement update was received early.
	auto movementIt = earlyMovements.find(node->id);
	if(movementIt != earlyMovements.end())
	{
		node->SetLastMovement(movementIt->second);
		earlyMovements.erase(movementIt);
	}

	//Set enabled state, if a enabled state update was received early.
	auto enabledIt = earlyEnabledUpdates.find(node->id);
	if(enabledIt != earlyEnabledUpdates.end())
	{
		node->visibility.setVisibility(enabledIt->second.enabled, InvisibilityReason::DISABLED);
		earlyEnabledUpdates.erase(enabledIt);
	}

	//Set correct highlighting for node, if a highlight update was received early.
	auto highlightIt = earlyNodeHighlights.find(node->id);
	if(highlightIt != earlyNodeHighlights.end())
	{
		node->SetHighlighted(highlightIt->second);
		earlyNodeHighlights.erase(highlightIt);
	}

	//Set playing animation, if an animation update was received early.
	auto animationIt = earlyAnimationUpdates.find(node->id);
	if(animationIt != earlyAnimationUpdates.end())
	{
		node->animationComponent.setAnimation(animationIt->second.animationID, animationIt->second.timestamp);
		earlyAnimationUpdates.erase(animationIt);
	}

	//Set playing animation, if an animation control update was received early.
	auto animationControlIt = earlyAnimationControlUpdates.find(node->id);
	if(animationControlIt != earlyAnimationControlUpdates.end())
	{
		for(const EarlyAnimationControl& earlyControlUpdate : animationControlIt->second)
		{
			node->animationComponent.setAnimationTimeOverride(earlyControlUpdate.animationID, earlyControlUpdate.timeOverride, earlyControlUpdate.overrideMaximum);
		}
		earlyAnimationControlUpdates.erase(animationControlIt);
	}

	//Set animation speed, if an animation speed update was received early.
	auto animationSpeedIt = earlyAnimationSpeedUpdates.find(node->id);
	if(animationSpeedIt != earlyAnimationSpeedUpdates.end())
	{
		for(const EarlyAnimationSpeed& earlySpeedUpdate : animationSpeedIt->second)
		{
			node->animationComponent.setAnimationSpeed(earlySpeedUpdate.animationID, earlySpeedUpdate.speed);
		}
		earlyAnimationSpeedUpdates.erase(animationSpeedIt);
	}
}

void NodeManager::RemoveNode(std::shared_ptr<Node> node)
{
	//Remove node from parent's child list.
	std::shared_ptr<Node> parent = node->GetParent().lock();
	if(parent)
	{
		parent->RemoveChild(node);
	}
	//Remove from root nodes, if the node had no parent.
	else
	{
		rootNodes.erase(std::find(rootNodes.begin(), rootNodes.end(), node));
		distanceSortedRootNodes.erase(std::find(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), node));
	}

	//Attach children to world root.
	std::vector<std::weak_ptr<Node>> children = node->GetChildren();
	for (std::weak_ptr<Node> childPtr : children)
	{
		std::shared_ptr<Node> child = childPtr.lock();
		if (child)
		{
			rootNodes.push_back(child);
			distanceSortedRootNodes.push_back(child);
			//Remove parent
			child->SetParent(nullptr);
			parentLookup.erase(child->id);
		}
	}

	//Remove from node lookup table.
	nodeLookup.erase(node->id);
}

void NodeManager::RemoveNode(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		RemoveNode(nodeIt->second);
	}
}

bool NodeManager::HasNode(avs::uid nodeID) const
{
	return nodeLookup.find(nodeID) != nodeLookup.end();
}

std::shared_ptr<Node> NodeManager::GetNode(avs::uid nodeID) const
{
	return HasNode(nodeID) ? nodeLookup.at(nodeID) : nullptr;
}

size_t NodeManager::GetNodeAmount() const
{
	return nodeLookup.size();
}

const NodeManager::nodeList_t& NodeManager::GetRootNodes() const
{
	return rootNodes;
}

const std::vector<std::shared_ptr<Node>>& NodeManager::GetSortedRootNodes()
{
	std::sort
	(
		distanceSortedRootNodes.begin(),
		distanceSortedRootNodes.end(),
		[](std::shared_ptr<Node> a, std::shared_ptr<Node> b)
		{
			return a->distance < b->distance;
		}
	);

	return distanceSortedRootNodes;
}

void NodeManager::SetBody(std::shared_ptr<Node> node)
{
	body = node;
}

bool NodeManager::SetBody(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if(nodeIt != nodeLookup.end())
	{
		SetBody(nodeIt->second);
		return true;
	}

	return false;
}

std::shared_ptr<Node> NodeManager::GetBody() const
{
	return body;
}

void NodeManager::SetLeftHand(std::shared_ptr<Node> node)
{
	leftHand = node;
}

bool NodeManager::SetLeftHand(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if(nodeIt != nodeLookup.end())
	{
		SetLeftHand(nodeIt->second);
		return true;
	}

	return false;
}

std::shared_ptr<Node> NodeManager::GetLeftHand() const
{
	return leftHand;
}

void NodeManager::SetRightHand(std::shared_ptr<Node> node)
{
	rightHand = node;
}

bool NodeManager::SetRightHand(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if(nodeIt != nodeLookup.end())
	{
		SetRightHand(nodeIt->second);
		return true;
	}

	return false;
}

std::shared_ptr<Node> NodeManager::GetRightHand() const
{
	return rightHand;
}

bool NodeManager::ShowNode(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		nodeIt->second->SetVisible(true);
		return true;
	}

	return false;
}

bool NodeManager::HideNode(avs::uid nodeID)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		nodeIt->second->SetVisible(false);
		return true;
	}

	return false;
}

void NodeManager::SetVisibleNodes(const std::vector<avs::uid> visibleNodes)
{
	//Hide all nodes.
	for(const auto& it : nodeLookup)
	{
		it.second->SetVisible(false);
	}

	//Show visible nodes.
	for(avs::uid id : visibleNodes)
	{
		ShowNode(id);
	}
}

bool NodeManager::UpdateNodeTransform(avs::uid nodeID, const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	auto nodeIt = nodeLookup.find(nodeID);
	if (nodeIt != nodeLookup.end())
	{
		nodeIt->second->UpdateModelMatrix(translation, rotation, scale);
		return true;
	}

	return false;
}

void NodeManager::UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList)
{
	earlyMovements.clear();

	for(avs::MovementUpdate update : updateList)
	{
		std::shared_ptr<scr::Node> node = GetNode(update.nodeID);
		if(node)
		{
			node->SetLastMovement(update);
		}
		else
		{
			earlyMovements[update.nodeID] = update;
		}
	}
}

void NodeManager::UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList)
{
	earlyEnabledUpdates.clear();

	for(avs::NodeUpdateEnabledState update : updateList)
	{
		std::shared_ptr<scr::Node> node = GetNode(update.nodeID);
		if(node)
		{
			node->visibility.setVisibility(update.enabled, InvisibilityReason::DISABLED);
		}
		else
		{
			earlyEnabledUpdates[update.nodeID] = update;
		}
	}
}

void scr::NodeManager::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	std::shared_ptr<scr::Node> node = GetNode(nodeID);
	if(node)
	{
		node->SetHighlighted(isHighlighted);
	}
	else
	{
		earlyNodeHighlights[nodeID] = isHighlighted;
	}
}

void NodeManager::UpdateNodeAnimation(const avs::NodeUpdateAnimation& animationUpdate)
{
	std::shared_ptr<scr::Node> node = GetNode(animationUpdate.nodeID);
	if(node)
	{
		node->animationComponent.setAnimation(animationUpdate.animationID, animationUpdate.timestamp);
	}
	else
	{
		earlyAnimationUpdates[animationUpdate.nodeID] = animationUpdate;
	}
}

void scr::NodeManager::UpdateNodeAnimationControl(avs::uid nodeID, avs::uid animationID, const float* animationTimeOverride, float overrideMaximum)
{
	std::shared_ptr<scr::Node> node = GetNode(nodeID);
	if(node)
	{
		node->animationComponent.setAnimationTimeOverride(animationID, animationTimeOverride, overrideMaximum);
	}
	else
	{
		std::vector<EarlyAnimationControl>& earlyControlUpdates = earlyAnimationControlUpdates[nodeID];
		earlyControlUpdates.emplace_back(EarlyAnimationControl{animationID, animationTimeOverride, overrideMaximum});
	}
}

void scr::NodeManager::SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	std::shared_ptr<scr::Node> node = GetNode(nodeID);
	if(node)
	{
		node->animationComponent.setAnimationSpeed(animationID, speed);
	}
	else
	{
		std::vector<EarlyAnimationSpeed>& earlySpeedUpdates = earlyAnimationSpeedUpdates[nodeID];
		earlySpeedUpdates.emplace_back(EarlyAnimationSpeed{animationID, speed});
	}
}

void NodeManager::Update(float deltaTime)
{
	nodeList_t expiredNodes;
	for(const std::shared_ptr<scr::Node>& node : rootNodes)
	{
		node->Update(deltaTime);

		if(node->GetTimeSinceLastVisible() >= nodeLifetime && node->visibility.getInvisibilityReason() == InvisibilityReason::OUT_OF_BOUNDS)
		{
			expiredNodes.push_back(node);
		}
	}

	//Delete nodes that have been invisible for too long.
	for(const std::shared_ptr<scr::Node>& node : expiredNodes)
	{
		RemoveNode(node);
	}

	if(body)
	{
		body->Update(deltaTime);
	}
	if(leftHand)
	{
		leftHand->Update(deltaTime);
	}
	if(rightHand)
	{
		rightHand->Update(deltaTime);
	}
}

void NodeManager::Clear()
{
	rootNodes.clear();
	distanceSortedRootNodes.clear();
	nodeLookup.clear();

	body = nullptr;
	leftHand = nullptr;
	rightHand = nullptr;

	parentLookup.clear();

	earlyMovements.clear();
	earlyEnabledUpdates.clear();
	earlyNodeHighlights.clear();
	earlyAnimationUpdates.clear();
	earlyAnimationControlUpdates.clear();
	earlyAnimationSpeedUpdates.clear();
}

void NodeManager::ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes)
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
	std::shared_ptr<scr::Node> node = GetNode(nodeID);
	return node != nullptr && node->IsVisible();
}

void NodeManager::LinkToParentNode(avs::uid childID)
{
	//Do nothing if the child doesn't appear in the parent lookup; i.e. we have not received a parent for the node.
	auto parentIt = parentLookup.find(childID);
	if(parentIt == parentLookup.end())
	{
		return;
	}

	std::shared_ptr<Node> parent = GetNode(parentIt->second);
	std::shared_ptr<Node> child = GetNode(childID);

	//Do nothing if we couldn't find one of the nodes; likely due to the parent being removed before the child was received.
	if(parent == nullptr || child == nullptr)
	{
		return;
	}

	//Connect up hierarchy.
	child->SetParent(parent);
	parent->AddChild(child);
	distanceSortedRootNodes.erase(std::find(distanceSortedRootNodes.begin(), distanceSortedRootNodes.end(), child));
	//Erase child from the root nodes list, as they now have a parent.
	rootNodes.erase(std::find(rootNodes.begin(), rootNodes.end(), child));
}
