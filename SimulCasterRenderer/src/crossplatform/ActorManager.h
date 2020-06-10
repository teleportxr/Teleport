// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

#include "libavstream/geometry/mesh_interface.hpp"

#include "Actor.h"
#include "ResourceManager.h"

namespace scr
{
	class ActorManager
	{
	public:
		struct LiveActor
		{
            avs::uid actorID = 0; //ID of the actor.
			std::shared_ptr<Actor> actor;

			uint32_t timeSinceLastVisible = 0; //Miliseconds the actor has been invisible.
		};

		typedef std::vector<std::shared_ptr<LiveActor>> actorList_t;

		uint32_t actorLifetime = 2000; //Milliseconds the manager waits before removing invisible actors.

		virtual ~ActorManager() = default;

		virtual void CreateActor(avs::uid actorID, const Actor::ActorCreateInfo& actorCreateInfo)
		{
			actorList.emplace(actorList.begin() + visibleActorAmount, std::make_unique<LiveActor>(LiveActor{actorID, std::make_shared<Actor>(actorCreateInfo)}));
			actorLookup[actorID] = visibleActorAmount;
			++visibleActorAmount;

			//Link new actor to parent.
			LinkToParentActor(actorID);

			//Link actor's children to this actor.
			for(avs::uid childID : actorCreateInfo.childIDs)
			{
				parentActors[childID] = actorID;
				LinkToParentActor(childID);
			}
		}

        virtual void CreateHand(avs::uid handID, const Actor::ActorCreateInfo& handCreateInfo)
        {
            handList.emplace_back(std::make_unique<LiveActor>(LiveActor{handID, std::make_shared<Actor>(handCreateInfo)}));
			actorLookup[handID] = handList.size() - 1;

			if(!rightHand) SetHands(0, handID);
			else if(!leftHand) SetHands(handID);
        }

		void RemoveActor(avs::uid actorID)
		{
			if(!HasActor(actorID)) return;

			if(IsActorVisible(actorID)) --visibleActorAmount;
			actorList.erase(actorList.begin() + actorLookup.at(actorID));
		}

		bool HasActor(avs::uid actorID) const
		{
			return actorLookup.find(actorID) != actorLookup.end();
		}

		std::shared_ptr<Actor> GetActor(avs::uid actorID)
		{
			return HasActor(actorID) ? actorList[actorLookup.at(actorID)]->actor : nullptr;
		}

		bool SetHands(avs::uid leftHandID = 0, avs::uid rightHandID = 0)
        {
            if(leftHandID != 0)
            {
				if(!HasActor(leftHandID)) return false;
				leftHand = handList[actorLookup.at(leftHandID)].get();
            }

            if(rightHandID != 0)
            {
				if(!HasActor(rightHandID)) return false;
				rightHand = handList[actorLookup.at(rightHandID)].get();
            }

            return true;
        }

		void GetHands(LiveActor*& outLeftHand, LiveActor*& outRightHand)
        {
		    outLeftHand = leftHand;
            outRightHand = rightHand;
        }

		//Causes the actor to become visible.
		bool ShowActor(avs::uid actorID)
		{
			if(HasActor(actorID))
			{
				size_t actorIndex = actorLookup.at(actorID);

				if(!IsActorVisible(actorIndex))
				{
					size_t swapIndex = visibleActorAmount;

					auto actorIt = actorList.begin() + actorIndex;
					auto swapIt = actorList.begin() + swapIndex;

					(*actorIt)->timeSinceLastVisible = 0;

					actorLookup.at((*actorIt)->actorID) = swapIndex;
					actorLookup.at((*swapIt)->actorID) = actorIndex;
					std::iter_swap(actorIt, swapIt);
					
					++visibleActorAmount;

					return true;
				}
			}

			return false;
		}

		//Causes the actor to become invisible.
		bool HideActor(avs::uid actorID)
		{
			if(HasActor(actorID))
			{
				size_t actorIndex = actorLookup.at(actorID);

				if(IsActorVisible(actorIndex))
				{
					size_t swapIndex = visibleActorAmount - 1;

					auto actorIt = actorList.begin() + actorIndex;
					auto swapIt = actorList.begin() + swapIndex;

					actorLookup.at((*actorIt)->actorID) = swapIndex;
					actorLookup.at((*swapIt)->actorID) = actorIndex;
					std::iter_swap(actorIt, swapIt);

					--visibleActorAmount;

					return true;
				}
			}

			return false;
		}

		//Make all actors in the passed list visible, while hiding the actors that are absent.
		void SetVisibleActors(const std::vector<avs::uid> visibleActors)
		{
			//Hide all actors.
			visibleActorAmount = 0;

			//Show actors that should be visible.
			for(avs::uid id : visibleActors)
			{
				ShowActor(id);
			}
		}

        bool UpdateActorTransform(avs::uid actorID, const scr::vec3& translation, const scr::quat& rotation, const scr::vec3& scale)
        {
			if(!HasActor(actorID)) return false;
			
			actorList[actorLookup.at(actorID)]->actor->UpdateModelMatrix(translation, rotation, scale);
			return true;
        }

		bool UpdateHandTransform(avs::uid handID, const scr::vec3& translation, const scr::quat& rotation, const scr::vec3& scale)
		{
			if(!HasActor(handID)) return false;

			handList[actorLookup.at(handID)]->actor->UpdateModelMatrix(translation, rotation, scale);
			return true;
		}

		void UpdateActorMovement(std::vector<avs::MovementUpdate> updateList)
		{
			for(avs::MovementUpdate update : updateList)
			{
				std::shared_ptr<scr::Actor> actor = GetActor(update.nodeID);
				if(actor == nullptr) continue;

				actor->SetLastMovement(update);
			}
		}

		//Tick the actor manager along, and remove any actors that have been invisible for too long.
		//	deltaTime : Milliseconds since last update.
		void Update(float deltaTime)
		{
			{
				//Increment timeSinceLastVisible for all invisible actors.
				auto actorIt = actorList.begin() + visibleActorAmount;
				for(; actorIt != actorList.end(); actorIt++)
				{
					(*actorIt)->timeSinceLastVisible += deltaTime;

					if((*actorIt)->timeSinceLastVisible >= actorLifetime) break;
				}

				//Delete actors that have been invisible for too long.
				for(; actorIt != actorList.end();)
				{
					actorLookup.erase((*actorIt)->actorID);
					actorIt = actorList.erase(actorIt);
				}
			}

			{
				for(auto actorIt = actorList.begin(); actorIt != actorList.begin() + visibleActorAmount; actorIt++)
				{
					
					(*actorIt)->actor->TickExtrapolatedTransform(deltaTime);
				}
			}
		}

		//Clear actor manager of all actors.
		void Clear()
		{
			actorList.clear();
			handList.clear();
            visibleActorAmount = 0;

			actorLookup.clear();
			leftHand = nullptr;
			rightHand = nullptr;
		}

		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingActors : List of actors in the exclude list that were actually in the actor manager.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingActors)
		{
			for(auto it = actorList.begin(); it != actorList.end();)
			{
				//Increment the iterator if it is excluded.
				if(std::find(excludeList.begin(), excludeList.end(), (*it)->actorID) != excludeList.end())
				{
					excludeList.erase(std::remove(excludeList.begin(), excludeList.end(), (*it)->actorID), excludeList.end());
					outExistingActors.push_back((*it)->actorID);

					++it;
				}
				//Remove the resource if it is not.
				else
				{
					actorLookup.erase((*it)->actorID);
					it = actorList.erase(it);
					--visibleActorAmount;
				}
			}
		}

		const actorList_t& GetActorList() const
		{
			return actorList;
		}

		size_t getVisibleActorAmount() const
		{
			return visibleActorAmount;
		}

	protected:
		actorList_t actorList; //Actors/geometry that are drawn to the screen.
        size_t visibleActorAmount = 0; //Amount of actors in the actorList currently being rendered.

		actorList_t handList; //List of hand actors; handled differently as we don't want them cleaned-up.
        std::unordered_map<avs::uid, size_t> actorLookup; //<ActorID, Index of actor in actorList/handList>

        LiveActor* leftHand = nullptr;
        LiveActor* rightHand = nullptr;
	private:
		std::map<avs::uid, avs::uid> parentActors;

		//Uses the index of the actor in the actorList to determine if it is visible.
	    bool IsActorVisible(size_t actorIndex) const
		{
			return actorIndex < visibleActorAmount;
		}

		//Links the actor with the passed ID to it's parent. If the actor doesn't exist, then it doesn't do anything.
		void LinkToParentActor(avs::uid actorID)
		{
			auto parentIt = parentActors.find(actorID);
			if(parentIt == parentActors.end()) return;

			std::shared_ptr<Actor> parent = GetActor(parentIt->second);
			std::shared_ptr<Actor> child = GetActor(actorID);

			child->SetParent(parent);
			parent->AddChild(child);
		}
	};
}