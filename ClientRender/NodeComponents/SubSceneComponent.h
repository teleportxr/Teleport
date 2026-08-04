#pragma once

#include "ClientRender/Mesh.h"
#include "Common.h"
#include "NodeComponents/Component.h"
// Not forward-declared: pendingAnimation stores an ApplyAnimation by value.
#include "TeleportCore/CommonNetworking.h"
#include <chrono>
#include <map>
#include <memory>
#include <vector>

namespace teleport
{
	namespace clientrender
	{
		class Node;

		//! Attached to a node whose renderable content is an entire sub-scene - a glTF/VRM asset
		//! fetched by URL and decoded into its own GeometryCache with client-local uids. The
		//! server knows only this node, never the sub-scene's contents, so anything addressed at
		//! the node - animation above all - has to be forwarded inwards.
		class SubSceneComponent : public Component
		{
		public:
			SubSceneComponent(Node &n) : Component(n)
			{
			}
			virtual ~SubSceneComponent()
			{
			}

			//! The distinct skeleton root nodes inside this sub-scene. Empty if the sub-scene's
			//! cache or mesh has not arrived yet, or if it holds no skeleton.
			std::vector<std::shared_ptr<Node>> GetSkeletonRootNodes() const;

			//! Forward a server-sent animation update to every skeleton in the sub-scene.
			//!
			//! An update that arrives before the sub-scene is usable is **remembered, not
			//! dropped**. This is the normal case, not an edge case: the server acknowledges a
			//! MeshPointer the moment the pointer chunk arrives, long before the asset behind
			//! it has been fetched over HTTPS, decoded and turned into a skeleton. A server has
			//! no way to know when that finishes, and no reason to send the state again --
			//! animation states are sent on change, and standing still is a state.
			//!
			//! @param sessionTimeUs Server-session time, microseconds. See AnimationComponent::PlayAnimation.
			//! @return true if it was applied, false if it was deferred.
			bool ForwardAnimation(std::chrono::microseconds sessionTimeUs, const teleport::core::ApplyAnimation &applyAnimation);

			//! Retry a deferred animation update. Called once per frame from NodeManager::Update;
			//! does nothing unless there is one waiting and the sub-scene has become usable.
			void TryPendingAnimation(std::chrono::microseconds sessionTimeUs);

			//! Is there an animation update waiting for the sub-scene to finish loading?
			bool HasPendingAnimation() const
			{
				return hasPendingAnimation;
			}

			//! Shortcut for local/debug use: play this animation on layer 0, looping, starting now.
			void PlayAnimation(std::chrono::microseconds sessionTimeUs, avs::uid cache_uid, avs::uid anim_uid);

			avs::uid			  mesh_uid = 0;
			std::shared_ptr<Mesh> mesh;

		private:
			//! The most recent update that could not be applied yet. Only the most recent is
			//! kept: these describe a state, not an event, so an older one is of no use once a
			//! newer has arrived.
			teleport::core::ApplyAnimation pendingAnimation;
			bool						   hasPendingAnimation = false;
			//! Throttles the "still waiting" log so a sub-scene that never loads does not
			//! fill the log at frame rate.
			int64_t						   lastPendingWarningUs = 0;
		};
	}
}