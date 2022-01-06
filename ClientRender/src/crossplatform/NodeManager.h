// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

#include "libavstream/geometry/mesh_interface.hpp"

#include "Node.h"
#include "ResourceManager.h"

namespace scr
{
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
		size_t GetNodeAmount() const;

		//Get list of nodes parented to the world root.
		const nodeList_t& GetRootNodes() const;
		const std::vector<std::shared_ptr<Node>> & GetSortedRootNodes();

		bool SetBody(avs::uid nodeID);
		std::shared_ptr<Node> GetBody() const;

		bool SetLeftHand(avs::uid nodeID);
		std::shared_ptr<Node> GetLeftHand() const;

		bool SetRightHand(avs::uid nodeID);
		std::shared_ptr<Node> GetRightHand() const;

		//Causes the node to become visible.
		bool ShowNode(avs::uid nodeID);
		//Causes the node to become invisible.
		bool HideNode(avs::uid nodeID);

		//Make all nodes in the passed list visible, while hiding the nodes that are absent.
		void SetVisibleNodes(const std::vector<avs::uid> visibleNodes);

		bool UpdateNodeTransform(avs::uid nodeID, const avs::vec3& translation, const scr::quat& rotation, const avs::vec3& scale);

		void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList);
		void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList);
		void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted);
		void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate);
		void UpdateNodeAnimationControl(avs::uid nodeID, avs::uid animationID, const float* const animationTimeOverride = nullptr, float overrideMaximum = 0.0f);
		void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed);

		//Tick the node manager along, and remove any nodes that have been invisible for too long.
		//	deltaTime : Milliseconds since last update.
		void Update(float deltaTime);

		//Clear node manager of all nodes.
		void Clear();
		//Clear, and free memory of, all resources; bar from resources on the list.
		//	excludeList : Elements to not clear from the manager; removes UID if it finds the element.
		//	outExistingNodes : List of nodes in the exclude list that were actually in the node manager.
		void ClearCareful(std::vector<uid>& excludeList, std::vector<uid>& outExistingNodes);

	protected:
		nodeList_t rootNodes; //Nodes that are parented to the world root.
		std::vector<std::shared_ptr<scr::Node>> distanceSortedRootNodes; //The rootNodes list above, but sorted from near to far.
	
        std::unordered_map<avs::uid, std::shared_ptr<Node>> nodeLookup;

		uid body;
		uid leftHand;
		uid rightHand;
	private:
		struct EarlyAnimationControl
		{
			avs::uid animationID;
			const float* timeOverride;
			float overrideMaximum;
		};

		struct EarlyAnimationSpeed
		{
			avs::uid animationID;
			float speed;
		};

		std::map<avs::uid, avs::uid> parentLookup; //Lookup for the parent of an node, so they can be linked when received. <ChildID, ParentID>

		//Node updates that were received before the node was received.
		std::map<avs::uid, avs::MovementUpdate> earlyMovements;
		std::map<avs::uid, avs::NodeUpdateEnabledState> earlyEnabledUpdates;
		std::map<avs::uid, bool> earlyNodeHighlights;
		std::map<avs::uid, avs::ApplyAnimation> earlyAnimationUpdates;
		std::map<avs::uid, std::vector<EarlyAnimationControl>> earlyAnimationControlUpdates;
		std::map<avs::uid, std::vector<EarlyAnimationSpeed>> earlyAnimationSpeedUpdates;

		//Uses the index of the node in the nodeList to determine if it is visible.
		bool IsNodeVisible(avs::uid nodeID) const;

		//Links the node with the passed ID to it's parent. If the node doesn't exist, then it doesn't do anything.
		void LinkToParentNode(avs::uid nodeID);
	};
}