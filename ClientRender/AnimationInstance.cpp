#include "ClientRender/AnimationInstance.h"
#include "ClientRender/Animation.h"
#include "ClientRender/GeometryCache.h"
#include "ClientRender/Node.h"
#include "ozz/animation/runtime/local_to_model_job.h"

#pragma optimize("", off)

using namespace teleport::clientrender;

AnimationInstance::AnimationInstance(std::shared_ptr<Skeleton> sk) : skeleton(sk->GetOzzSkeleton())
{
	id = sk->id;
	if (sk)
	{
		Init();
	}
}

void AnimationInstance::Init()
{
	if (!skeleton)
	{
		return;
	}
	const int num_soa_joints = skeleton->num_soa_joints();
	const int num_joints	 = skeleton->num_joints();
	// Allocates sampling context.
	context.Resize(num_joints);

	// Allocates model space runtime buffers of blended data.
	models.resize(num_joints);

	// Storage for blending stage output.
	locals.resize(num_soa_joints);
}

void AnimationInstance::SetAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation)
{
	if (!skeleton)
	{
		return;
	}
	if (applyAnimation.animLayer >= numAnimationLayerStates)
	{
		if (applyAnimation.animLayer >= 3)
		{
			TELEPORT_WARN("Exceeded maximum animation layer number.");
			return;
		}
		numAnimationLayerStates	 = applyAnimation.animLayer + 1;
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
		auto anim = cache->mAnimationManager.Get(applyAnimation.animationID);
		if (anim)
		{
			auto *ozz_animation = anim->GetOzzAnimation(id);
			if (!ozz_animation || ozz_animation->num_tracks() != skeleton->num_joints())
			{
				return;
			}
			st.animation = anim;
		}
	}
	animationLayerStates[applyAnimation.animLayer].AddState(timestampUs, st);
}

bool AnimationInstance::ApplyRestPose()
{
	if (!skeleton || !numAnimationLayerStates)
	{
		return false;
	}

	const int num_soa_joints = skeleton->num_soa_joints();
	const int num_joints	 = skeleton->num_joints();

	// Ensure buffers are properly sized
	locals.resize(num_soa_joints);
	models.resize(num_joints);

	// Ensure sampler buffers are sized correctly
	auto &layerState = animationLayerStates[0];
	auto &sampler	 = layerState.sampler;
	sampler.locals.resize(num_soa_joints);
	sampler.joint_weights.resize(num_soa_joints, ozz::math::simd_float4::one());

	auto ident = ozz::math::Transform::identity();
	for (size_t i = 0; i < sampler.locals.size(); i++)
	{
		sampler.locals[i].rotation	  = ozz::math::SoaQuaternion::Load({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1.0f, 1.0f, 1.0f, 1.0f});
		sampler.locals[i].translation = ozz::math::SoaFloat3::Load({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0});
		sampler.locals[i].scale		  = ozz::math::SoaFloat3::Load({1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f});
	}
	const mat4 *m = (const mat4 *)models.data();
	//  Convert from local-space to model-space transforms
	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = skeleton;
	ltmJob.input	= ozz::make_span(sampler.locals); // Use sampled local transforms
	ltmJob.output	= ozz::make_span(models);

	if (!ltmJob.Run())
	{
		return false;
	}

	return true;
}

// In AnimationComponent.cpp - Fixed AnimationInstance::Update method

bool AnimationInstance::Update(float dt_s, int64_t time_us)
{
	if (!skeleton || !numAnimationLayerStates)
	{
		return false;
	}

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
	{
		return false;
	}

	auto *animation = state.animationState.animation->GetOzzAnimation(id);
	if (!animation)
	{
		return false;
	}

	// CRITICAL FIX: Calculate proper animation time
	float animationTimeS = state.animationState.animationTimeS;

	// If we have speed, update time based on elapsed time
	if (state.animationState.speedUnitsPerS != 0.0f)
	{
		// float elapsedS = 0.01f*(time_us - state.animationState.timestampUs) / 1000000.0f;
		// animationTimeS += dt_s * state.animationState.speedUnitsPerS;
	}

	// Handle looping
	/*if (state.animationState.loop && animation->duration() > 0.0f)
	{
		animationTimeS = fmodf(animationTimeS, animation->duration());
	}*/

	//  Set time ratio correctly (0.0 to 1.0)
	float timeRatio = 0.0f;
	float d = animation->duration();
	if (d > 0.0f)
	{
		timeRatio		  = animationTimeS / d;
		// Wraps in the unit interval [0:1]
		const float loops = floorf(timeRatio);
		timeRatio		  = timeRatio - loops;
	}

	// Ensure sampler buffers are sized correctly
	sampler.locals.resize(num_soa_joints);
	sampler.joint_weights.resize(num_soa_joints, ozz::math::simd_float4::one());

	//
	ozz::animation::SamplingJob samplingJob;
	samplingJob.animation = animation;
	samplingJob.context	  = &context;
	samplingJob.ratio	  = timeRatio; // Use calculated time ratio
	samplingJob.output	  = ozz::make_span(sampler.locals);

	// Sample the animation
	if (!samplingJob.Run())
	{
		return false;
	}
	const mat4 *m = (const mat4 *)models.data();
	//  Convert from local-space to model-space transforms
	ozz::animation::LocalToModelJob ltmJob;
	ltmJob.skeleton = skeleton;
	ltmJob.input	= ozz::make_span(sampler.locals); // Use sampled local transforms
	ltmJob.output	= ozz::make_span(models);

	if (!ltmJob.Run())
	{
		return false;
	}

	return true;
}



void AnimationInstance::GetBoneMatrices(std::vector<mat4> &m, const std::vector<int16_t> &mapMeshToSkeleton, const std::vector<mat4> &inverseBindMatrices
	, const std::vector<int> &mapSkeletonToAnim) const
{
    if (models.empty()||!inverseBindMatrices.size())
    {
        // Fallback to identity matrices
        //m.resize(std::min(inverseBindMatrices.size(), static_cast<size_t>(Skeleton::MAX_BONES)));
        for (size_t i = 0; i < m.size(); i++)
        {
            m[i] = mat4::identity();
        }
        return;
    }
    // Use the computed model-space matrices from Ozz
    size_t numBones = std::min({
        inverseBindMatrices.size(),
        static_cast<size_t>(Skeleton::MAX_BONES)
    });
    m.resize(numBones);
	//const auto &mapMeshToSkeleton = owner.GetJointIndices();
    if(inverseBindMatrices.size()!=mapMeshToSkeleton.size())
	{
		TELEPORT_WARN("Bad skin mapping");
	}
	static uint64_t force_ident=0;//0x14;//0xFFFFFFFFFFFF0000;
    for (size_t i = 0; i < numBones; i++)
    {
        // Convert Ozz matrix to your matrix format
		int sk_index = mapMeshToSkeleton[i];
		if(sk_index<0||sk_index>=mapSkeletonToAnim.size())
			continue;
		int anim_joint_index = mapSkeletonToAnim[sk_index];
        const ozz::math::Float4x4& ozzMatrix = models[anim_joint_index];
        // CRITICAL: Ozz matrices are column-major, ensure correct conversion
        mat4 joint_matrix= *((mat4*)&ozzMatrix);
		// transpose so that in row-major format, translation is in the right-hand column
		joint_matrix.transpose();
        // Apply inverse bind matrix to get final bone matrix
        const mat4& inverseBindMatrix = inverseBindMatrices[i];
        m[i] = joint_matrix * inverseBindMatrix;
		//m[i].transpose(); 206, 219 hand and ring
		if(force_ident&(1ULL<<i))
			m[i]=mat4::identity();
    }
}