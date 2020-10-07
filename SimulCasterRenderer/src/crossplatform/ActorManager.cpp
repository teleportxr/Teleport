#include "ActorManager.h"

using namespace scr;

std::shared_ptr<Node> ActorManager::CreateActor(avs::uid id) const
{
	return std::make_shared<Node>(id);
}

void ActorManager::AddActor(std::shared_ptr<Node> actor, bool isHand)
{
	//Remove any actor already using the ID.
	RemoveActor(actor->id);

	isHand ? LinkHand(actor) : LinkActor(actor);
}

void ActorManager::RemoveActor(std::shared_ptr<Node> actor)
{
	//Remove actor from parent's child list.
	std::shared_ptr<Node> parent = actor->GetParent().lock();
	if(parent)
	{
		parent->RemoveChild(actor);
	}
	//Remove from root actors, if the actor had no parent.
	else
	{
		rootActors.erase(std::find(rootActors.begin(), rootActors.end(), actor));
	}

	//Attach children to world root.
	std::vector<std::weak_ptr<Node>> children = actor->GetChildren();
	for(std::weak_ptr<Node> childPtr : children)
	{
		std::shared_ptr<Node> child = childPtr.lock();
		if(child)
		{
			rootActors.push_back(child);

			//Remove parent
			child->SetParent(std::weak_ptr<Node>());
			parentLookup.erase(child->id);
		}
	}

	//Remove from actor lookup table.
	actorLookup.erase(actor->id);
}

void ActorManager::RemoveActor(avs::uid actorID)
{
	auto actorIt = actorLookup.find(actorID);
	if(actorIt != actorLookup.end())
	{
		RemoveActor(actorIt->second);
	}
}

bool ActorManager::HasActor(avs::uid actorID) const
{
	return actorLookup.find(actorID) != actorLookup.end();
}

std::shared_ptr<Node> ActorManager::GetActor(avs::uid actorID)
{
	return HasActor(actorID) ? actorLookup.at(actorID) : nullptr;
}

size_t ActorManager::GetActorAmount()
{
	return actorLookup.size();
}

void ActorManager::SetHands(avs::uid leftHandID, avs::uid rightHandID)
{
	if(leftHandID != 0)
	{
		this->leftHandID = leftHandID;
	}

	if(rightHandID != 0)
	{
		this->rightHandID = rightHandID;
	}
}

void ActorManager::GetHands(std::shared_ptr<Node>& outLeftHand, std::shared_ptr<Node>& outRightHand)
{
	auto leftHandIt = actorLookup.find(leftHandID);
	if(leftHandIt != actorLookup.end())
	{
		outLeftHand = leftHandIt->second;
	}
	else
	{
		leftHandID = 0;
	}

	auto rightHandIt = actorLookup.find(rightHandID);
	if(rightHandIt != actorLookup.end())
	{
		outRightHand = rightHandIt->second;
	}
	else
	{
		rightHandID = 0;
	}
}

bool ActorManager::ShowActor(avs::uid actorID)
{
	auto actorIt = actorLookup.find(actorID);
	if(actorIt != actorLookup.end())
	{
		actorIt->second->SetVisible(true);
		return true;
	}

	return false;
}

bool ActorManager::HideActor(avs::uid actorID)
{
	auto actorIt = actorLookup.find(actorID);
	if(actorIt != actorLookup.end())
	{
		actorIt->second->SetVisible(false);
		return true;
	}

	return false;
}

void ActorManager::SetVisibleActors(const std::vector<avs::uid> visibleActors)
{
	//Hide all actors.
	for(auto it : actorLookup)
	{
		it.second->SetVisible(false);
	}

	//Show visible actors.
	for(avs::uid id : visibleActors)
	{
		ShowActor(id);
	}
}

bool ActorManager::UpdateActorTransform(avs::uid actorID, const avs::vec3& translation, const quat& rotation, const avs::vec3& scale)
{
	auto actorIt = actorLookup.find(actorID);
	if(actorIt != actorLookup.end())
	{
		actorIt->second->UpdateModelMatrix(translation, rotation, scale);
		return true;
	}

	return false;
}

void ActorManager::UpdateActorMovement(std::vector<avs::MovementUpdate> updateList)
{
	earlyMovements.clear();

	for(avs::MovementUpdate update : updateList)
	{
		auto actorIt = actorLookup.find(update.nodeID);
		if(actorIt != actorLookup.end())
		{
			actorIt->second->SetLastMovement(update);
		}
		else
		{
			earlyMovements[update.nodeID] = update;
		}
	}
}

void ActorManager::Update(float deltaTime)
{
	actorList_t expiredActors;
	for(auto actor : rootActors)
	{
		actor->Update(deltaTime);

		if(actor->GetTimeSinceLastVisible() >= actorLifetime)
		{
			expiredActors.push_back(actor);
		}
	}

	//Delete actors that have been invisible for too long.
	for(auto actor : expiredActors)
	{
		RemoveActor(actor);
	}
}

void ActorManager::Clear()
{
	rootActors.clear();
	handList.clear();
	actorLookup.clear();

	leftHandID = 0;
	rightHandID = 0;

	parentLookup.clear();
}

void ActorManager::ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingActors)
{
	for(auto it = actorLookup.begin(); it != actorLookup.end();)
	{
		auto exclusionIt = std::find(excludeList.begin(), excludeList.end(), it->first);

		//Keep actor in manager, if it is in the exclusion list.
		if(exclusionIt != excludeList.end())
		{
			excludeList.erase(exclusionIt);
			outExistingActors.push_back(it->first);
			++it;
		}
		else
		{
			RemoveActor(it->second);
		}
	}
}

const ActorManager::actorList_t& ActorManager::GetRootActors() const
{
	return rootActors;
}

void ActorManager::LinkActor(std::shared_ptr<Node> newActor)
{
	rootActors.push_back(newActor);
	actorLookup[newActor->id] = newActor;

	//Update movement based on movement data that was received before the actor was complete.
	auto movementIt = earlyMovements.find(newActor->id);
	if(movementIt != earlyMovements.end()) newActor->SetLastMovement(movementIt->second);

	//Link new actor to parent.
	LinkToParentActor(newActor->id);

	//Link actor's children to this actor.
	for(avs::uid childID : newActor->GetChildrenIDs())
	{
		parentLookup[childID] = newActor->id;
		LinkToParentActor(childID);
	}
}

void ActorManager::LinkHand(std::shared_ptr<Node> newHand)
{
	handList.push_back(newHand);
	actorLookup[newHand->id] = newHand;

	if(!rightHandID) rightHandID = newHand->id;
	else if(!leftHandID) leftHandID = newHand->id;
}

bool ActorManager::IsActorVisible(avs::uid actorID) const
{
	return HasActor(actorID) && actorLookup.at(actorID)->IsVisible();
}

void ActorManager::LinkToParentActor(avs::uid childID)
{
	auto parentIt = parentLookup.find(childID);
	if(parentIt == parentLookup.end()) return;

	std::shared_ptr<Node> parent = GetActor(parentIt->second);
	std::shared_ptr<Node> child = GetActor(childID);

	if(parent == nullptr || child == nullptr) return;

	child->SetParent(parent);
	parent->AddChild(child);

	rootActors.erase(std::find(rootActors.begin(), rootActors.end(), actorLookup[childID]));
}