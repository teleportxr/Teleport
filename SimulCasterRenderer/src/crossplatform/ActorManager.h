// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <map>
#include <memory>

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
		
		void CreateActor(avs::uid actor_uid, const Actor::ActorCreateInfo& pActorCreateInfo)
		{
			m_Actors[actor_uid] = {std::make_shared<Actor>(pActorCreateInfo), 0};
		}

		void RemoveActor(avs::uid actor_uid)
		{
			m_Actors.erase(actor_uid);
		}

		bool HasActor(avs::uid actor_uid) const
		{
			return m_Actors.find(actor_uid) != m_Actors.end();
		}

		std::shared_ptr<Actor> GetActor(avs::uid actor_uid) const
		{
			if(HasActor(actor_uid))
			{
				return m_Actors.at(actor_uid).actor;
			}

				return nullptr;
		}

		//Causes the actor to become visible.
		bool ShowActor(avs::uid actor_uid)
		{
			if(HasActor(actor_uid))
			{
				m_Actors[actor_uid].actor->isVisible = true;
				return true;
			}

			return false;
		}

		//Causes the actor to become invisible.
		bool HideActor(avs::uid actor_uid)
		{
			if(HasActor(actor_uid))
			{
				m_Actors[actor_uid].actor->isVisible = false;
				return true;
		}

			return false;
		}

		//Tick the actor manager along, and remove any actors that have been invisible for too long.
		void Update(uint32_t deltaTimestamp)
		{
			for(auto it = m_Actors.begin(); it != m_Actors.end();)
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
						it = m_Actors.erase(it);
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
			m_Actors.clear();
		}

		const std::map<avs::uid, LiveActor>& GetActorList()
		{
			return m_Actors;
		}
	private:
		std::map<avs::uid, LiveActor> m_Actors;
	};
}