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

		virtual std::shared_ptr<Node> CreateNode(avs::uid id, const std::string& name) const;

		void AddNode(std::shared_ptr<Node> node, const avs::DataNode& nodeData);

		void RemoveNode(std::shared_ptr<Node> node);
		void RemoveNode(avs::uid nodeID);

		bool HasNode(avs::uid nodeID) const;

		std::shared_ptr<Node> GetNode(avs::uid nodeID) const;
		size_t GetNodeAmount() const;

		//Get list of nodes parented to the world root.
		const nodeList_t& GetRootNodes() const;
		const std::vector<std::shared_ptr<Node>> & GetSortedRootNodes();

		void SetBody(std::shared_ptr<Node> node);
		bool SetBody(avs::uid nodeID);
		std::shared_ptr<Node> GetBody() const;

		void SetLeftHand(std::shared_ptr<Node> node);
		bool SetLeftHand(avs::uid nodeID);
		std::shared_ptr<Node> GetLeftHand() const;

		void SetRightHand(std::shared_ptr<Node> node);
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
		void UpdateNodeAnimation(const avs::NodeUpdateAnimation& animationUpdate);
		void UpdateNodeAnimationControl(avs::uid nodeID, avs::uid animationID, const float* const animationTimeOverride = nullptr, float overrideMaximum = 0.0f);

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
		/// List of hand nodes; handled differently as we don't want them cleaned-up.
        std::unordered_map<avs::uid, std::shared_ptr<Node>> nodeLookup;

		std::shared_ptr<Node> body;
		std::shared_ptr<Node> leftHand;
		std::shared_ptr<Node> rightHand;
	private:
		struct EarlyAnimationControl
		{
			avs::uid animationID;
			const float* timeOverride;
			float overrideMaximum;
		};

		std::map<avs::uid, avs::uid> parentLookup; //Lookup for the parent of an node, so they can be linked when received. <ChildID, ParentID>
		std::map<avs::uid, avs::MovementUpdate> earlyMovements; //Movements that have arrived before the node was received.
		std::map<avs::uid, avs::NodeUpdateEnabledState> earlyEnabledUpdates; //Enabled state updates that have arrived before the node was received.
		std::map<avs::uid, avs::NodeUpdateAnimation> earlyAnimationUpdates; //Animation updates that were received before the node was received.
		std::map<avs::uid, EarlyAnimationControl> earlyAnimationControlUpdates; //Animation control updates that were received before the node was received. Pair is <AnimationID, Animation Time Override>.

		//Uses the index of the node in the nodeList to determine if it is visible.
		bool IsNodeVisible(avs::uid nodeID) const;

		//Links the node with the passed ID to it's parent. If the node doesn't exist, then it doesn't do anything.
		void LinkToParentNode(avs::uid nodeID);
	};
}