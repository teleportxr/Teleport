#include "AnimationComponent.h"

#include <algorithm>
#include <libavstream/src/platform.hpp>

#include "AnimationInstance.h"
#include "AnimationState.h"
#include "ClientRender/Animation.h"
#include "ClientRender/AnimationRetargeter.h"
#include "GeometryCache.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/Logging.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/motion_blending_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include <ozz/animation/runtime/track.h>
#include <ozz/base/maths/soa_transform.h>
#include <regex>

using namespace teleport::clientrender;
using namespace ozz;

AnimationComponent::AnimationComponent(Node &n) : Component(n)
{
}

AnimationComponent::~AnimationComponent()
{
}

void AnimationComponent::PlayAnimation(avs::uid cache_id, avs::uid anim_uid, avs::uid root_uid, uint32_t layer, float speed)
{
	teleport::core::ApplyAnimation applyAnimation;
	applyAnimation.speedUnitsPerSecond		 = speed;
	applyAnimation.animLayer				 = (uint32_t)layer;
	applyAnimation.animationID				 = anim_uid;
	applyAnimation.cacheID					 = cache_id;
	applyAnimation.nodeID					 = 0;
	applyAnimation.loop						 = true;
	std::chrono::microseconds timestampNowUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
	applyAnimation.timestampUs				 = timestampNowUs.count();
	setAnimationState(timestampNowUs, applyAnimation, root_uid);
}

std::map<std::string, teleport::core::PoseScale>::const_iterator FindMatch(const std::map<std::string, teleport::core::PoseScale> &poses, std::string name)
{
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
	for (std::map<std::string, teleport::core::PoseScale>::const_iterator p = poses.begin(); p != poses.end(); p++)
	{
		std::string f = p->first;
		std::transform(f.begin(), f.end(), f.begin(), ::tolower);
		if (name.find(f) < name.length())
		{
			return p;
		}
	}
	return poses.end();
}

float det(const mat4 &M)
{
	mat4 inv;
	inv.m[0] = M.m[5] * M.m[10] * M.m[15] - M.m[5] * M.m[11] * M.m[14] - M.m[9] * M.m[6] * M.m[15] + M.m[9] * M.m[7] * M.m[14] + M.m[13] * M.m[6] * M.m[11] -
			   M.m[13] * M.m[7] * M.m[10];

	inv.m[4] = -M.m[4] * M.m[10] * M.m[15] + M.m[4] * M.m[11] * M.m[14] + M.m[8] * M.m[6] * M.m[15] - M.m[8] * M.m[7] * M.m[14] - M.m[12] * M.m[6] * M.m[11] +
			   M.m[12] * M.m[7] * M.m[10];

	inv.m[8] = M.m[4] * M.m[9] * M.m[15] - M.m[4] * M.m[11] * M.m[13] - M.m[8] * M.m[5] * M.m[15] + M.m[8] * M.m[7] * M.m[13] + M.m[12] * M.m[5] * M.m[11] -
			   M.m[12] * M.m[7] * M.m[9];

	inv.m[12] = -M.m[4] * M.m[9] * M.m[14] + M.m[4] * M.m[10] * M.m[13] + M.m[8] * M.m[5] * M.m[14] - M.m[8] * M.m[6] * M.m[13] - M.m[12] * M.m[5] * M.m[10] +
				M.m[12] * M.m[6] * M.m[9];
	return M.m[0] * inv.m[0] + M.m[1] * inv.m[4] + M.m[2] * inv.m[8] + M.m[3] * inv.m[12];
}

void AnimationComponent::Retarget(Animation &anim)
{
	// We modify the animation to match the skeleton.
	int idx = 0;
	anim.Retarget(owner.GetSkeleton());
}

std::shared_ptr<AnimationInstance> AnimationComponent::GetOrCreateAnimationInstance(avs::uid root_uid)
{
	// Zero is the top-level instance: the renderer keys the root cache's states under 0 as well
	// (InstanceRenderer's subSceneStatesMap[0]), so setting and updating a node that is not in a
	// sub-scene both land here. Fall back to any existing instance first, then create - returning
	// null instead of creating left a node outside a sub-scene with nowhere to put its state.
	if (root_uid == 0 && animationInstances.size())
	{
		return animationInstances.begin()->second;
	}
	auto f = animationInstances.find(root_uid);
	if (f != animationInstances.end())
	{
		return f->second;
	}
	animationInstances.emplace(root_uid, std::make_shared<AnimationInstance>(owner.GetSkeleton()));
	return animationInstances[root_uid];
}

void AnimationComponent::setAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation, avs::uid root_uid)
{
	if (applyAnimation.animLayer >= 32)
	{
		TELEPORT_WARN("Exceeded maximum animation layer number.");
		return;
	}
	// Nothing to drive. The caller is expected to have resolved the animatable node already - a
	// sub-scene's root node has no skeleton of its own - so reaching here means the state is about to
	// be lost. Say so: silently dropping it is indistinguishable from an avatar that simply will not
	// animate, which is expensive to diagnose.
	if (!owner.GetSkeleton())
	{
		TELEPORT_WARN("Animation {} not applied to node {} ({}): it has no skeleton.", applyAnimation.animationID, owner.id, owner.name);
		return;
	}
	auto instance = GetOrCreateAnimationInstance(root_uid);
	if (!instance)
	{
		return;
	}
	// Zero is not a cache: it is the protocol's "the cache containing nodeID", which only the caller
	// can resolve. GetGeometryCache returns null for it, and for a cache that has since been destroyed.
	auto cache = GeometryCache::GetGeometryCache(applyAnimation.cacheID);
	if (!cache)
	{
		TELEPORT_WARN("Animation {} not applied to node {} ({}): no geometry cache {}.", applyAnimation.animationID, owner.id, owner.name,
					  applyAnimation.cacheID);
		return;
	}
	auto anim = cache->mAnimationManager.Get(applyAnimation.animationID);
	if (anim)
	{
		Retarget(*anim);
	}
	instance->SetAnimationState(timestampUs, applyAnimation);
}

bool AnimationComponent::update(int64_t timestampUs, avs::uid root_uid)
{
	if (!owner.GetSkeleton())
	{
		return false;
	}
	// GetOrCreateAnimationInstance stores what it creates, so there is no case left where this is
	// null. Building a throwaway here instead would discard the state again on every frame.
	auto instance = GetOrCreateAnimationInstance(root_uid);
	if (!instance)
	{
		return false;
	}
	float dt		= lastTimestampUs ? float(double(timestampUs - lastTimestampUs) / 1000000.0) : 0.0f;
	lastTimestampUs = timestampUs;
	return instance->Update(dt, timestampUs);
}
