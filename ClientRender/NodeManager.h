// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <parallel_hashmap/phmap.h>

#include "libavstream/geometry/mesh_interface.hpp"

#include "Node.h"
#include "ResourceManager.h"
#include "flecs.h"
#include "Platform/CrossPlatform/Quaterniond.h"

struct flecs_local_pos
{
	vec3 value;
};
struct flecs_local_orientation
{
	platform::crossplatform::Quaternionf value;
};


namespace clientrender
{
	template<typename T>
	struct weak_ptr_compare {
		bool operator() (const std::weak_ptr<T> &lhs, const std::weak_ptr<T> &rhs) const
		{
			auto lptr = lhs.lock(), rptr = rhs.lock();
			if (!rptr) return false; // nothing after expired pointer 
			if (!lptr) return true;  // every not expired after expired pointer
			return lptr < rptr;
		}
	};
	//! Manages nodes for a specific server context.
	class NodeManager
	{
		flecs::world &flecs_world;
		ecs_entity_t FlecsEntity(avs::uid node_id);

		//ECS_COMPONENT_DECLARE(flecs_pos);
	/*	phmap::flat_hash_map<avs::uid, ecs_entity_t> flecs_entity_map;
		inline phmap::flat_hash_map<avs::uid, ecs_entity_t> &GetFlecsEntityMap()
		{
			return flecs_entity_map;
		}*/
	public:
		NodeManager(flecs::world &flecs_w);
		typedef std::vector<std::shared_ptr<Node>> nodeList_t;

		uint32_t nodeLifetime = 30000; //Milliseconds the manager waits before removing invisible nodes.

		virtual ~NodeManager() = default;

		virtual std::shared_ptr<Node> CreateNode(avs::uid id, const avs::Node &avsNode) ;

		void RemoveNode(std::shared_ptr<Node> node);
		void RemoveNode(avs::uid nodeID);

		bool HasNode(avs::uid nodeID) const;

		std::shared_ptr<Node> GetNode(avs::uid nodeID) const;
		size_t GetNodeCount() const;

		//Get list of nodes parented to the world root.
		const std::vector<std::weak_ptr<Node>>& GetRootNodes() const;
		const std::vector<std::weak_ptr<Node>> & GetSortedRootNodes();
		const std::vector<std::weak_ptr<Node>>& GetSortedTransparentNodes();

		//Causes the node to become visible.
		bool ShowNode(avs::uid nodeID);
		//Causes the node to become invisible.
		bool HideNode(avs::uid nodeID);

		//Make all nodes in the passed list visible, while hiding the nodes that are absent.
		void SetVisibleNodes(const std::vector<avs::uid> visibleNodes);

		bool UpdateNodeTransform(avs::uid nodeID, const vec3& translation, const clientrender::quat& rotation, const vec3& scale);

		void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList);
		void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList);
		void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted);
		void UpdateNodeAnimation(const teleport::core::ApplyAnimation& animationUpdate);
		void UpdateNodeAnimationControl(avs::uid nodeID, avs::uid animationID,  float  animationTimeOverride=0.0f, float overrideMaximum = 0.0f);
		void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

		//! Returns true if successful, or false if not e.g. if either the node or the parent is not present.
		bool ReparentNode(const teleport::core::UpdateNodeStructureCommand& updateNodeStructureCommand);

		void NotifyModifiedMaterials(std::shared_ptr<clientrender::Node> node);
		//Tick the node manager along, and remove any nodes that have been invisible for too long.
		//	deltaTime : Milliseconds since last update.
		void Update( float deltaTime);
		// For rendering
		void UpdateExtrapolatedPositions(double serverTimeS);
		//Clear node manager of all nodes.
		void Clear();
		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingNodes : List of nodes in the exclude list that were actually in the node manager.
		void ClearAllButExcluded(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes);

		//! Get the nodes that have been removed since the last update.
		const std::set<avs::uid> &GetRemovedNodeUids() const;
	protected:
		std::vector<std::weak_ptr<Node>> rootNodes; //Nodes that are parented to the world root.
		std::vector<std::weak_ptr<Node>> distanceSortedRootNodes; //The rootNodes list above, but sorted from near to far.
	
		std::vector<std::weak_ptr<Node>> transparentNodes; //Nodes that are parented to the world root.
		std::vector<std::weak_ptr<Node>> distanceSortedTransparentNodes; //The rootNodes list above, but sorted from near to far.

		// Nodes that have been added, or modified, to be sorted into transparent or not.
		std::set<std::weak_ptr<Node>,weak_ptr_compare<Node>> nodesWithModifiedMaterials;

        phmap::flat_hash_map<avs::uid, std::shared_ptr<Node>> nodeLookup;

	private:
		void AddNode(std::shared_ptr<Node> node, const avs::Node& nodeData);
		struct EarlyAnimationControl
		{
			avs::uid animationID;
			float timeOverride;
			float overrideMaximum;
		};

		struct EarlyAnimationSpeed
		{
			avs::uid animationID;
			float speed;
		};

		std::map<avs::uid, avs::uid> parentLookup; //Lookup for the parent of an node, so they can be linked when received. <ChildID, ParentID>
		std::map<avs::uid,std::set<avs::uid>> childLookup;

		std::set<avs::uid> removed_node_uids;
		//Node updates that were received before the node was received.
		std::map<avs::uid, teleport::core::MovementUpdate> earlyMovements;
		std::map<avs::uid, teleport::core::NodeUpdateEnabledState> earlyEnabledUpdates;
		std::map<avs::uid, bool> earlyNodeHighlights;
		std::map<avs::uid, teleport::core::ApplyAnimation> earlyAnimationUpdates;
		std::map<avs::uid, std::vector<EarlyAnimationControl>> earlyAnimationControlUpdates;
		std::map<avs::uid, std::vector<EarlyAnimationSpeed>> earlyAnimationSpeedUpdates;
		/// For tracking which nodes have been hidden.
		std::set<avs::uid> hiddenNodes;
		//Uses the index of the node in the nodeList to determine if it is visible.
		bool IsNodeVisible(avs::uid nodeID) const;

		//Links the node with the passed ID to it's parent. If the node doesn't exist, then it doesn't do anything.
		void LinkToParentNode(std::shared_ptr<Node> child);
		mutable std::mutex nodeLookup_mutex;
		mutable std::mutex rootNodes_mutex;
		mutable std::mutex early_mutex;
		mutable std::mutex distanceSortedRootNodes_mutex;
		mutable std::mutex distanceSortedTransparentNodes_mutex;
	};
}
