#include "NodeComponents/SubSceneComponent.h"
#include "GeometryCache.h"
#include "NodeComponents/AnimationComponent.h"
#include "TeleportCore/CommonNetworking.h"
#include <set>
using namespace teleport;
using namespace clientrender;

std::vector<std::shared_ptr<Node>> SubSceneComponent::GetSkeletonRootNodes() const
{
	std::vector<std::shared_ptr<Node>> roots;
	if (!mesh || !mesh->GetMeshCreateInfo().subscene_cache_uid)
	{
		return roots;
	}
	auto cache = GeometryCache::GetGeometryCache(mesh->GetMeshCreateInfo().subscene_cache_uid);
	if (!cache)
	{
		return roots;
	}
	// One sub-scene can hold several skeletons sharing a root, so collect distinct roots.
	const auto		  &sk_ids = cache->mSkeletonManager.GetAllIDs();
	std::set<avs::uid> root_uids;
	for (auto sk_id : sk_ids)
	{
		auto sk = cache->mSkeletonManager.Get(sk_id);
		if (!sk)
		{
			continue;
		}
		root_uids.insert(sk->GetRootId());
	}
	for (auto root_uid : root_uids)
	{
		auto node = cache->mNodeManager.GetNode(root_uid);
		if (node)
		{
			roots.push_back(node);
		}
	}
	return roots;
}

void SubSceneComponent::ApplyAnimation(std::chrono::microseconds sessionTimeUs, const teleport::core::ApplyAnimation &applyAnimation)
{
	if (!applyAnimation.animationID)
	{
		return;
	}
	// root_uid is owner.id because the renderer keys each sub-scene's animation instance on
	// SubSceneNodeStates::root_id, which is the id of the node that holds the sub-scene.
	// Any other value creates an instance that is written but never ticked.
	for (auto node : GetSkeletonRootNodes())
	{
		auto animC = node->GetOrCreateComponent<AnimationComponent>();
		if (animC)
		{
			animC->setAnimationState(sessionTimeUs, applyAnimation, owner.id);
		}
	}
}

void SubSceneComponent::PlayAnimation(std::chrono::microseconds sessionTimeUs, avs::uid cache_uid, avs::uid anim_uid)
{
	teleport::core::ApplyAnimation applyAnimation;
	applyAnimation.animLayer		   = 0;
	applyAnimation.animationID		   = anim_uid;
	applyAnimation.cacheID			   = cache_uid;
	applyAnimation.nodeID			   = owner.id;
	applyAnimation.loop				   = true;
	applyAnimation.speedUnitsPerSecond = 1.0f;
	applyAnimation.timestampUs		   = sessionTimeUs.count();
	ApplyAnimation(sessionTimeUs, applyAnimation);
}
