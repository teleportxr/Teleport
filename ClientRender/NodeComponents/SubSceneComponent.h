#pragma once

#include "ClientRender/Mesh.h"
#include "Common.h"
#include "NodeComponents/Component.h"
#include <chrono>
#include <map>
#include <memory>
#include <vector>

namespace teleport
{
	namespace core
	{
		struct ApplyAnimation;
	}
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
			//! @param sessionTimeUs Server-session time, microseconds. See AnimationComponent::PlayAnimation.
			void ForwardAnimation(std::chrono::microseconds sessionTimeUs, const teleport::core::ApplyAnimation &applyAnimation);

			//! Shortcut for local/debug use: play this animation on layer 0, looping, starting now.
			void PlayAnimation(std::chrono::microseconds sessionTimeUs, avs::uid cache_uid, avs::uid anim_uid);

			avs::uid			  mesh_uid = 0;
			std::shared_ptr<Mesh> mesh;
		};
	}
}