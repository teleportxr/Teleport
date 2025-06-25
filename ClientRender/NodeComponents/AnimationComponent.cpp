#include "AnimationComponent.h"

#include <algorithm>
#include <libavstream/src/platform.hpp>

#include "ClientRender/Animation.h"
#include "ClientRender/AnimationRetargeter.h"
#include "GeometryCache.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/Logging.h"
#include "AnimationState.h"
#include "AnimationInstance.h"

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
	applyAnimation.speedUnitsPerSecond			= speed;
	applyAnimation.animLayer				 = (uint32_t)layer;
	applyAnimation.animationID				 = anim_uid;
	applyAnimation.cacheID					 = cache_id;
	applyAnimation.nodeID					 = 0;
	applyAnimation.loop=true;
	std::chrono::microseconds timestampNowUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
	applyAnimation.timestampUs				 = timestampNowUs.count();
	setAnimationState(timestampNowUs, applyAnimation, root_uid);
}

std::map<std::string,teleport::core::PoseScale>::const_iterator FindMatch(const std::map<std::string,teleport::core::PoseScale>& poses,std::string name)
{
	std::transform(name.begin(), name.end(), name.begin(), ::tolower);
	for(std::map<std::string,teleport::core::PoseScale>::const_iterator p=poses.begin();p!=poses.end();p++)
	{
		std::string f=p->first;
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
	inv.m[0] = M.m[5]  * M.m[10] * M.m[15] - 
			 M.m[5]  * M.m[11] * M.m[14] - 
			 M.m[9]  * M.m[6]  * M.m[15] + 
			 M.m[9]  * M.m[7]  * M.m[14] +
			 M.m[13] * M.m[6]  * M.m[11] - 
			 M.m[13] * M.m[7]  * M.m[10];

	inv.m[4] = -M.m[4]  * M.m[10] * M.m[15] + 
			  M.m[4]  * M.m[11] * M.m[14] + 
			  M.m[8]  * M.m[6]  * M.m[15] - 
			  M.m[8]  * M.m[7]  * M.m[14] - 
			  M.m[12] * M.m[6]  * M.m[11] + 
			  M.m[12] * M.m[7]  * M.m[10];

	inv.m[8] = M.m[4]  * M.m[9] * M.m[15] - 
			 M.m[4]  * M.m[11] * M.m[13] - 
			 M.m[8]  * M.m[5] * M.m[15] + 
			 M.m[8]  * M.m[7] * M.m[13] + 
			 M.m[12] * M.m[5] * M.m[11] - 
			 M.m[12] * M.m[7] * M.m[9];

	inv.m[12] = -M.m[4]  * M.m[9] * M.m[14] + 
			   M.m[4]  * M.m[10] * M.m[13] +
			   M.m[8]  * M.m[5] * M.m[14] - 
			   M.m[8]  * M.m[6] * M.m[13] - 
			   M.m[12] * M.m[5] * M.m[10] + 
			   M.m[12] * M.m[6] * M.m[9];
	return M.m[0] * inv.m[0] + M.m[1] * inv.m[4] + M.m[2] * inv.m[8] + M.m[3] * inv.m[12];
}

void AnimationComponent::Retarget(  Animation &anim) 
{
	// We modify the animation to match the skeleton.
	int idx=0;
	anim.Retarget(owner.GetSkeleton());
	//instance->ApplyRestPose();
}

// CRITICAL FIX: Update GetBoneMatrices to use Ozz results properly
/*void AnimationComponent::GetJointMatrices(std::vector<mat4> &m) const
{
    // Use the computed model-space matrices from Ozz
    size_t numBones = std::min({
        instance->models.size(),
        static_cast<size_t>(Skeleton::MAX_BONES)
    });
    
    m.resize(numBones);
    for (size_t i = 0; i < numBones; i++)
    {
        const ozz::math::Float4x4& ozzMatrix = instance->models[i];
        mat4 jointMatrix= *((mat4*)&ozzMatrix);
		jointMatrix.transpose();
        m[i] = jointMatrix;
    }
}*/

std::shared_ptr<AnimationInstance> AnimationComponent::GetOrCreateAnimationInstance(avs::uid root_uid) 
{
	auto f=animationInstances.find(root_uid);
	if(f!=animationInstances.end())
		return f->second;
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
		return;
	auto instance		= GetOrCreateAnimationInstance(root_uid);
	auto cache		= GeometryCache::GetGeometryCache(applyAnimation.cacheID);
	auto anim			= cache->mAnimationManager.Get(applyAnimation.animationID);
	if(anim)
		Retarget(*anim) ;
	instance->SetAnimationState(timestampUs, applyAnimation);
}

void AnimationComponent::update(int64_t timestampUs, avs::uid root_uid)
{
	if (!owner.GetSkeleton())
		return;
	auto instance = GetOrCreateAnimationInstance(root_uid);
	if (!instance)
	{
		instance.reset(new AnimationInstance(owner.GetSkeleton()));
	}
	static float dt = lastTimestampUs?float(double(timestampUs-lastTimestampUs)/1000000.0):0.0f;
	instance->Update(dt, timestampUs);
	lastTimestampUs=timestampUs;
}
