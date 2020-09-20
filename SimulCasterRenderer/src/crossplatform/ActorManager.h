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
		typedef std::vector<std::shared_ptr<Actor>> actorList_t;

		uint32_t actorLifetime = 2000; //Milliseconds the manager waits before removing invisible actors.

		virtual ~ActorManager() = default;

		virtual std::shared_ptr<Actor> CreateActor(avs::uid actorID);
		virtual std::shared_ptr<Actor> CreateHand(avs::uid handID);

		void RemoveActor(std::shared_ptr<Actor> actor);
		void RemoveActor(avs::uid actorID);

		bool HasActor(avs::uid actorID) const;

		std::shared_ptr<Actor> GetActor(avs::uid actorID);
		size_t GetActorAmount();

		void SetHands(avs::uid leftHandID = 0, avs::uid rightHandID = 0);
		void GetHands(std::shared_ptr<Actor>& outLeftHand, std::shared_ptr<Actor>& outRightHand);

		//Get list of actors parented to the world root.
		const actorList_t& GetRootActors() const;

		//Causes the actor to become visible.
		bool ShowActor(avs::uid actorID);
		//Causes the actor to become invisible.
		bool HideActor(avs::uid actorID);

		//Make all actors in the passed list visible, while hiding the actors that are absent.
		void SetVisibleActors(const std::vector<avs::uid> visibleActors);

		bool UpdateActorTransform(avs::uid actorID, const avs::vec3& translation, const scr::quat& rotation, const avs::vec3& scale);

		void UpdateActorMovement(std::vector<avs::MovementUpdate> updateList);

		//Tick the actor manager along, and remove any actors that have been invisible for too long.
		//	deltaTime : Milliseconds since last update.
		void Update(float deltaTime);

		//Clear actor manager of all actors.
		void Clear();
		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingActors : List of actors in the exclude list that were actually in the actor manager.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingActors);

	protected:
		actorList_t rootActors; //Actors that are parented to the world root.
		/// List of hand actors; handled differently as we don't want them cleaned-up.
		actorList_t handList; 
        std::unordered_map<avs::uid, std::shared_ptr<Actor>> actorLookup;

		avs::uid leftHandID = 0;
		avs::uid rightHandID = 0;

		//Add actor to manager after being created.
		void AddActor(std::shared_ptr<Actor> newActor);
		//Add hand to manager after being created.
		void AddHand(std::shared_ptr<Actor> newHand);
	private:
		std::map<avs::uid, avs::uid> parentLookup; //Lookup for the parent of an actor, so they can be linked when received. <ChildID, ParentID>
		std::map<avs::uid, avs::MovementUpdate> earlyMovements; //Movements that have arrived before the actor was received.

		//Uses the index of the actor in the actorList to determine if it is visible.
		bool IsActorVisible(avs::uid actorID) const;

		//Links the actor with the passed ID to it's parent. If the actor doesn't exist, then it doesn't do anything.
		void LinkToParentActor(avs::uid actorID);
	};
}