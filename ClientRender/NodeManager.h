// (C) Copyright 2018-2022 Simul Software Ltd
#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

#include "libavstream/geometry/mesh_interface.hpp"

#include "Node.h"
#include "ResourceManager.h"

namespace clientrender
{
	//! Manages nodes for a specific server context.
	class NodeManager
	{
	public:
		typedef std::vector<std::shared_ptr<Node>> nodeList_t;

		uint32_t nodeLifetime = 30000; //Milliseconds the manager waits before removing invisible nodes.

		virtual ~NodeManager() = default;

		virtual std::shared_ptr<Node> CreateNode(avs::uid id, const avs::Node &avsNode) ;

		void AddNode(std::shared_ptr<Node> node, const avs::Node& nodeData);

		void RemoveNode(std::shared_ptr<Node> node);
		void RemoveNode(avs::uid nodeID);

		bool HasNode(avs::uid nodeID) const;

		std::shared_ptr<Node> GetNode(avs::uid nodeID) const;
		size_t GetNodeCount() const;

		//Get list of nodes parented to the world root.
		const nodeList_t& GetRootNodes() const;
		const std::vector<std::shared_ptr<Node>> & GetSortedRootNodes();
		const std::vector<std::shared_ptr<Node>>& GetSortedTransparentNodes();

		//Causes the node to become visible.
		bool ShowNode(avs::uid nodeID);
		//Causes the node to become invisible.
		bool HideNode(avs::uid nodeID);

		//Make all nodes in the passed list visible, while hiding the nodes that are absent.
		void SetVisibleNodes(const std::vector<avs::uid> visibleNodes);

		bool UpdateNodeTransform(avs::uid nodeID, const avs::vec3& translation, const clientrender::quat& rotation, const avs::vec3& scale);

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
		void Update(float deltaTime);

		//Clear node manager of all nodes.
		void Clear();
		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingNodes : List of nodes in the exclude list that were actually in the node manager.
		void ClearAllButExcluded(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes);

		//! Get the nodes that have been removed since the last update.
		const std::set<avs::uid> &GetRemovedNodeUids() const;
	protected:
		nodeList_t rootNodes; //Nodes that are parented to the world root.
		std::vector<std::shared_ptr<clientrender::Node>> distanceSortedRootNodes; //The rootNodes list above, but sorted from near to far.
	
		nodeList_t transparentNodes; //Nodes that are parented to the world root.
		std::vector<std::shared_ptr<clientrender::Node>> distanceSortedTransparentNodes; //The rootNodes list above, but sorted from near to far.

		// Nodes that have been added, or modified, to be sorted into transparent or not.
		std::set<std::shared_ptr<clientrender::Node>> nodesWithModifiedMaterials;

        std::unordered_map<avs::uid, std::shared_ptr<Node>> nodeLookup;

	private:
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
		void LinkToParentNode(avs::uid nodeID);
		mutable std::mutex nodeLookup_mutex;
		mutable std::mutex rootNodes_mutex;
		mutable std::mutex distanceSortedRootNodes_mutex;
		mutable std::mutex distanceSortedTransparentNodes_mutex;
	};
}
