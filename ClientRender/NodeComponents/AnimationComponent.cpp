#include "AnimationComponent.h"

#include <algorithm>
#include <libavstream/src/platform.hpp>

#include "ClientRender/Animation.h"
#include "GeometryCache.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/Logging.h"
#include "AnimationState.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/motion_blending_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include <ozz/animation/runtime/track.h>
#include <ozz/base/maths/soa_transform.h>

using namespace teleport::clientrender;

using namespace ozz;

namespace teleport::clientrender
{
	struct AnimationInstance
	{
		uint8_t numAnimationLayerStates = 0;
		AnimationLayerStateSequence	   animationLayerStates[4];
		std::map<avs::uid, std::shared_ptr<Animation>> animations;
		ozz::animation::Skeleton					  *skeleton=nullptr;
		ozz::animation::SamplingJob::Context		   context;
		// Buffer of local transforms as sampled from main animation_.
		ozz::vector<ozz::math::SoaTransform> main_locals;
		// Buffer of local transforms which stores the blending result.
		ozz::vector<ozz::math::SoaTransform> locals;
		// Buffer of model space matrices. These are computed by the local-to-model
		// job after the blending stage.
		ozz::vector<ozz::math::Float4x4> models;

		AnimationInstance(std::shared_ptr<Skeleton> sk) : skeleton(sk->GetOzzSkeleton())
		{
			if(sk)
				Init();
		}

		void Init()
		{
			if(!skeleton)
				return;
			const int num_soa_joints = skeleton->num_soa_joints();
			const int num_joints	 = skeleton->num_joints();
			// Allocates sampling context.
			context.Resize(num_joints);

			// Allocates model space runtime buffers of blended data.
			models.resize(num_joints);

			// Storage for blending stage output.
			locals.resize(num_soa_joints);

			// samplers.resize(1);
		}

		void SetAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation)
		{
			if(!skeleton)
				return;
			if (applyAnimation.animLayer >= numAnimationLayerStates)
			{
				if (applyAnimation.animLayer >= 3)
				{
					TELEPORT_WARN("Exceeded maximum animation layer number.");
					return;
				}
				numAnimationLayerStates=applyAnimation.animLayer + 1;
				const int num_soa_joints = skeleton->num_soa_joints();
				const int num_joints	 = skeleton->num_joints();
				animationLayerStates[applyAnimation.animLayer].Init(num_soa_joints, num_joints);

			}
			AnimationState st;
			st.animationId	  = applyAnimation.animationID;
			st.animationTimeS = applyAnimation.animTimeAtTimestamp;
			st.speedUnitsPerS = applyAnimation.speedUnitsPerSecond;
			// The timestamp where the state applies.
			st.timestampUs	  = applyAnimation.timestampUs;
			st.loop			  = applyAnimation.loop;
			auto cache		  = GeometryCache::GetGeometryCache(applyAnimation.cacheID);
			if (cache)
			{
				auto anim	 = cache->mAnimationManager.Get(applyAnimation.animationID);
				if(anim->GetOzzAnimation().num_tracks()!=skeleton->num_joints())
					return;
				st.animation = anim;
			}
			animationLayerStates[applyAnimation.animLayer].AddState(timestampUs, st);
		}

	
// In AnimationComponent.cpp - Fixed AnimationInstance::Update method

		bool Update(float dt_s, int64_t time_us)
		{
			if (!skeleton || !numAnimationLayerStates)
				return false;

			const int num_soa_joints = skeleton->num_soa_joints();
			const int num_joints	 = skeleton->num_joints();

			// Ensure buffers are properly sized
			locals.resize(num_soa_joints);
			models.resize(num_joints);

			// Process the first animation layer (primary animation)
			auto &layerState  = animationLayerStates[0];
			auto &sampler	  = layerState.sampler;

			// Get current animation state
			const auto &state = layerState.getState(time_us);
			if (!state.animationState.animation)
				return false;

			auto &animation		 = state.animationState.animation->GetOzzAnimation();

			// CRITICAL FIX: Calculate proper animation time
			float animationTimeS = state.animationState.animationTimeS;

			// If we have speed, update time based on elapsed time
			if (state.animationState.speedUnitsPerS != 0.0f)
			{
				float elapsedS = 0.01f*(time_us - state.animationState.timestampUs) / 1000000.0f;
				animationTimeS += elapsedS * state.animationState.speedUnitsPerS;
			}

			// Handle looping
			if (state.animationState.loop && animation.duration() > 0.0f)
			{
				animationTimeS = fmodf(animationTimeS, animation.duration());
			}

			//  Set time ratio correctly (0.0 to 1.0)
			float timeRatio = 0.0f;
			if (animation.duration() > 0.0f)
			{
				timeRatio = animationTimeS / animation.duration();
				timeRatio = std::max(0.0f, std::min(1.0f, timeRatio));
			}

			// Ensure sampler buffers are sized correctly
			sampler.locals.resize(num_soa_joints);
			sampler.joint_weights.resize(num_soa_joints, ozz::math::simd_float4::one());

			//
			ozz::animation::SamplingJob samplingJob;
			samplingJob.animation = &animation;
			samplingJob.context	  = &context;
			samplingJob.ratio	  = timeRatio; // Use calculated time ratio
			samplingJob.output	  = make_span(sampler.locals);

			// Sample the animation
			if (!samplingJob.Run())
			{
				return false;
			}
			const mat4 *m=(const mat4*)models.data();
			//  Convert from local-space to model-space transforms
			ozz::animation::LocalToModelJob ltmJob;
			ltmJob.skeleton = skeleton;
			ltmJob.input	= make_span(sampler.locals); // Use sampled local transforms
			ltmJob.output	= make_span(models);

			if (!ltmJob.Run())
			{
				return false;
			}

			return true;
		}
	};
}

AnimationComponent::AnimationComponent(Node &n) : Component(n)
{
}

AnimationComponent::~AnimationComponent()
{
	delete instance;
}
void AnimationComponent::PlayAnimation(avs::uid cache_id, avs::uid anim_uid, uint32_t layer)
{
	teleport::core::ApplyAnimation applyAnimation;
	applyAnimation.animLayer				 = (uint32_t)layer;
	applyAnimation.animationID				 = anim_uid;
	applyAnimation.cacheID					 = cache_id;
	applyAnimation.nodeID					 = 0;
	applyAnimation.loop=true;
	std::chrono::microseconds timestampNowUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
	applyAnimation.timestampUs				 = timestampNowUs.count();
	setAnimationState(timestampNowUs, applyAnimation);
}

// CRITICAL FIX: Update GetBoneMatrices to use Ozz results properly
void AnimationComponent::GetJointMatrices(std::vector<mat4> &m) const
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
}

void AnimationComponent::GetBoneMatrices(std::vector<mat4> &m, const std::vector<mat4> &inverseBindMatrices) const
{
    if (!instance || instance->models.empty())
    {
        // Fallback to identity matrices
        m.resize(std::min(inverseBindMatrices.size(), static_cast<size_t>(Skeleton::MAX_BONES)));
        for (size_t i = 0; i < m.size(); i++)
        {
            m[i] = mat4::identity();
        }
        return;
    }

    // Use the computed model-space matrices from Ozz
    size_t numBones = std::min({
        instance->models.size(),
        inverseBindMatrices.size(),
        static_cast<size_t>(Skeleton::MAX_BONES)
    });
    
    m.resize(numBones);
    
    for (size_t i = 0; i < numBones; i++)
    {
        // Convert Ozz matrix to your matrix format
        const ozz::math::Float4x4& ozzMatrix = instance->models[i];
        
        // CRITICAL: Ozz matrices are column-major, ensure correct conversion
        mat4 jointMatrix= *((mat4*)&ozzMatrix);
        jointMatrix.transpose();
        // Apply inverse bind matrix to get final bone matrix
        const mat4& inverseBindMatrix = inverseBindMatrices[i];
        m[i] = jointMatrix * inverseBindMatrix;
    }
}

void AnimationComponent::setAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation)
{
	if (applyAnimation.animLayer >= 32)
	{
		TELEPORT_WARN("Exceeded maximum animation layer number.");
		return;
	}
	if (!owner.GetSkeleton())
		return;
	if (!instance)
		instance = new AnimationInstance(owner.GetSkeleton());
	instance->SetAnimationState(timestampUs, applyAnimation);
}

void AnimationComponent::update(int64_t timestampUs)
{
	if (!owner.GetSkeleton())
		return;
	if (!instance)
	{
		instance = new AnimationInstance(owner.GetSkeleton());
	}
	static float dt = 0.00001f;
	instance->Update(dt, timestampUs);
}
void AnimationComponent::update(const std::vector<std::shared_ptr<clientrender::Node>> &boneList, int64_t timestampUs)
{
#if 0
	// Early-out if we have no layers.
	if (!animationLayerStates.size())
	{
		return;
	}
	// each animation layer is applied on top of the previous one.
	for (const auto &s : animationLayerStates)
	{
		InstantaneousAnimationState state = s.getState(timestampUs);
		// This state provides:
		//	two animationStates, previous and next.
		//	an interpolation value between 0 and 1.0
		const AnimationState &s1		  = state.previousAnimationState;
		const AnimationState &s2		  = state.animationState;
		auto				  a1		  = animations.find(s1.animationId);
		if (state.interpolation < 1.f && a1 != animations.end() && a1->second)
		{
			a1->second->seekTime(boneList, s1.animationTimeS, 1.0f, s1.loop);
		}
		auto a2 = animations.find(s2.animationId);
		if (state.interpolation > 0.f && a2 != animations.end() && a2->second)
		{
			a2->second->seekTime(boneList, s2.animationTimeS, state.interpolation, s2.loop);
		}
	}
#endif
}