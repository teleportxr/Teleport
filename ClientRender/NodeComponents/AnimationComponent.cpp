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
	if (root_uid == 0)
	{
		if (animationInstances.size())
		{
			return animationInstances.begin()->second;
		}
		return std::shared_ptr<AnimationInstance>();
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
	if (!owner.GetSkeleton())
	{
		return;
	}
	auto instance = GetOrCreateAnimationInstance(root_uid);
	auto cache	  = GeometryCache::GetGeometryCache(applyAnimation.cacheID);
	auto anim	  = cache->mAnimationManager.Get(applyAnimation.animationID);
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
	auto instance = GetOrCreateAnimationInstance(root_uid);
	if (!instance)
	{
		instance.reset(new AnimationInstance(owner.GetSkeleton()));
	}
	float dt		= lastTimestampUs ? float(double(timestampUs - lastTimestampUs) / 1000000.0) : 0.0f;
	lastTimestampUs = timestampUs;
	return instance->Update(dt, timestampUs);
}
