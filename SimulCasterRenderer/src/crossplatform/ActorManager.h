// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>
#include <memory>
#include <functional>

#include <libavstream/geometry/mesh_interface.hpp>
#include "ResourceManager.h"
#include "Actor.h"

namespace scr
{
	class ActorManager
	{
	public:
		struct LiveActor
		{
			std::shared_ptr<Actor> actor = nullptr;
			uint32_t timeSinceLastVisible = 0; //Miliseconds the actor has been invisible.
		};

		uint32_t actorLifetime = 2000; //Milliseconds the manager waits before removing invisible actors.
		std::vector<avs::uid> handUIDs; //UIDs of the actors that are used as hands.

		std::function<void(avs::uid actorID, std::shared_ptr<Actor> actorInfo)> nativeActorCreator; //Function that creates a native actor.
		std::function<void(avs::uid actorID)> nativeActorDestroyer; //Function that destroys a native actor.
		std::function<void(void)> nativeActorClearer; //Function that clears the list of native actors.

		void CreateActor(avs::uid actor_uid, const Actor::ActorCreateInfo& pActorCreateInfo)
		{
			actorMap[actor_uid] = {std::make_shared<Actor>(pActorCreateInfo), 0};
			if(nativeActorCreator) nativeActorCreator(actor_uid, actorMap[actor_uid].actor);
		}

		void RemoveActor(avs::uid actor_uid)
		{
			RemoveActor_Internal(actor_uid);
		}

		bool HasActor(avs::uid actor_uid) const
		{
			return actorMap.find(actor_uid) != actorMap.end();
		}

		std::shared_ptr<Actor> GetActor(avs::uid actor_uid) const
		{
			if(HasActor(actor_uid))
			{
				return actorMap.at(actor_uid).actor;
			}

			return nullptr;
		}

		//Causes the actor to become visible.
		bool ShowActor(avs::uid actor_uid)
		{
			if(HasActor(actor_uid))
			{
				actorMap[actor_uid].actor->isVisible = true;
				return true;
			}

			return false;
		}

		//Causes the actor to become invisible.
		bool HideActor(avs::uid actor_uid)
		{
			if(HasActor(actor_uid))
			{
				actorMap[actor_uid].actor->isVisible = false;
				return true;
			}

			return false;
		}

		//Tick the actor manager along, and remove any actors that have been invisible for too long.
		void Update(uint32_t deltaTimestamp)
		{
			for(auto it = actorMap.begin(); it != actorMap.end();)
			{
				if(it->second.actor->isVisible)
				{
					//If the actor is visible, then reset their timer and continue.
					it->second.timeSinceLastVisible = 0;
					it++;
				}
				else
				{
					it->second.timeSinceLastVisible += deltaTimestamp;

					if(it->second.timeSinceLastVisible >= actorLifetime)
					{
						//Erase an actor if they have been invisible for too long.
						it = RemoveActor_Internal(it);
					}
					else
					{
						it++;
					}
				}
			}
		}

		//Clear actor manager of all actors.
		void Clear()
		{
			actorMap.clear();
			handUIDs.clear();
			if(nativeActorClearer) nativeActorClearer();
		}

		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingActors : List of actors in the exclude list that were actually in the actor manager.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingActors)
		{
			for(auto it = actorMap.begin(); it != actorMap.end();)
			{
				bool isExcluded = false; //We don't remove the resource if it is excluded.
				unsigned int i = 0;
				while(i < excludeList.size() && !isExcluded)
				{
					//The resource is excluded if its uid appears in the exclude list.
					if(excludeList[i] == it->first)
					{
						outExistingActors.push_back(excludeList[i]);
						isExcluded = true;
					}

					++i;
				}

				//Increment the iterator if it is excluded.
				if(isExcluded)
				{
					++it;
					excludeList.erase(std::remove(excludeList.begin(), excludeList.end(), excludeList[i - 1]), excludeList.end());
				}
				//Remove the resource if it is not.
				else
				{
					it = RemoveActor_Internal(it);
				}
			}
		}

		const std::map<avs::uid, LiveActor>& GetActorList()
		{
			return actorMap;
		}
	private:
		std::map<avs::uid, LiveActor> actorMap; //<ID of the actor, struct of the information on the living actor>.

		//Removes actor with passed ID; use the version with the iterator parameter where possible.
		//	actorID : ID of the actor to be removed.
		void RemoveActor_Internal(avs::uid actorID)
		{
			RemoveActor_Internal(actorMap.find(actorID));
		}

		//Removes actor at iterator, and returns iterator to next item in the list.
		//	currentItem : Iterator pointing to the item in the list that is to be removed.
		std::map<avs::uid, LiveActor>::iterator RemoveActor_Internal(std::map<avs::uid, LiveActor>::iterator currentItem)
		{
			avs::uid actorID = currentItem->first;

			std::map<avs::uid, LiveActor>::iterator nextItem = actorMap.erase(currentItem);
			if(nativeActorDestroyer) nativeActorDestroyer(actorID);

			return nextItem;
		}
	};
}