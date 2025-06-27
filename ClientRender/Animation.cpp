#include "Animation.h"

#include "Node.h"
#include "Transform.h"

#define tinygltf teleport_tinygltf
#include "AnimationRetargeter.h"
#include "tiny_gltf.h"
#include <json.hpp>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/containers/set.h>
#include <ozz/base/maths/soa_transform.h>

using nlohmann::json;
using namespace ozz;

using qt = platform::crossplatform::Quaternionf;
using namespace teleport;

using namespace clientrender;

Animation::Animation(const std::string &name) : name(name)
{
}

Animation::Animation(const std::string &name, float dur, std::vector<BoneKeyframeList> bk) : name(name), duration(dur), boneKeyframeLists(bk)
{
	ToOzz();
}

Animation::~Animation()
{
}

// Retrieve end time from latest time in any bone animations.
// ASSUMPTION: This works for Unity, but does it work for Unreal?
void Animation::updateAnimationLength()
{
}

float Animation::getAnimationLengthSeconds()
{
	return endTime_s;
}

void Animation::ToOzz()
{
	raw_animation			= ozz::make_unique<ozz::animation::offline::RawAnimation>();

	// Sets animation duration (to 1.4s).
	// All the animation keyframes times must be within range [0, duration].
	raw_animation->duration = duration;

	// Creates 3 animation tracks.
	// There should be as many tracks as there are joints in the skeleton that
	// this animation targets.
	raw_animation->tracks.resize(boneKeyframeLists.size());

	// Fills each track with keyframes, in joint local-space.
	// Tracks should be ordered in the same order as joints in the
	// ozz::animation::Skeleton. Joint's names can be used to find joint's
	// index in the skeleton.

	// Fills 1st track with 2 translation keyframes.
	for (int i = 0; i < boneKeyframeLists.size(); i++)
	{
		BoneKeyframeList &b		= boneKeyframeLists[i];
		auto			 &track = raw_animation->tracks[i];
		for (const auto &c : b.positionKeyframes)
		{
			// Create a keyframe, at c.time, with a translation value.
			const ozz::animation::offline::RawAnimation::TranslationKey key = {c.time, ozz::math::Float3(c.value.x, c.value.y, c.value.z)};
			track.translations.push_back(key);
		}
		for (const auto &c : b.rotationKeyframes)
		{
			// Create a keyframe, at c.time, with a quaternion value.
			const ozz::animation::offline::RawAnimation::RotationKey key = {c.time, ozz::math::Quaternion(c.value.x, c.value.y, c.value.z, c.value.w)};
			track.rotations.push_back(key);
		}
	}
	// Test for animation validity. These are the errors that could invalidate an animation:
	//  1. Animation duration is less than 0.
	//  2. Keyframes are not sorted in a strict ascending order.
	//  3. Keyframes are not within [0, duration] range.
	if (!raw_animation->Validate())
	{
		return;
	}
}

void MatrixDecompose(const mat4 &matrix, vec3 &scale, vec4 &rotation, vec3 &translation)
{
	// Extract translation (last column)
	translation.x	  = matrix._m30;
	translation.y	  = matrix._m31;
	translation.z	  = matrix._m32;

	// Extract scale (length of first three columns)
	scale.x			  = sqrt(matrix._m00 * matrix._m00 + matrix._m01 * matrix._m01 + matrix._m02 * matrix._m02);
	scale.y			  = sqrt(matrix._m10 * matrix._m10 + matrix._m11 * matrix._m11 + matrix._m12 * matrix._m12);
	scale.z			  = sqrt(matrix._m20 * matrix._m20 + matrix._m21 * matrix._m21 + matrix._m22 * matrix._m22);

	// Check for negative determinant (indicates reflection)
	float determinant = matrix._m00 * (matrix._m11 * matrix._m22 - matrix._m21 * matrix._m12) -
						matrix._m01 * (matrix._m10 * matrix._m22 - matrix._m12 * matrix._m20) +
						matrix._m02 * (matrix._m10 * matrix._m21 - matrix._m11 * matrix._m20);

	if (determinant < 0)
	{
		scale.x = -scale.x;
	}

	// Create rotation matrix by removing scale
	mat4 rotationMatrix = matrix;
	rotationMatrix._m00 /= scale.x;
	rotationMatrix._m01 /= scale.x;
	rotationMatrix._m02 /= scale.x;
	rotationMatrix._m10 /= scale.y;
	rotationMatrix._m11 /= scale.y;
	rotationMatrix._m12 /= scale.y;
	rotationMatrix._m20 /= scale.z;
	rotationMatrix._m21 /= scale.z;
	rotationMatrix._m22 /= scale.z;

	// Convert rotation matrix to quaternion (x, y, z, w)
	float trace = rotationMatrix._m00 + rotationMatrix._m11 + rotationMatrix._m22;

	if (trace > 0)
	{
		float s	   = sqrt(trace + 1.0f) * 2; // s = 4 * qw
		rotation.w = 0.25f * s;
		rotation.x = (rotationMatrix._m21 - rotationMatrix._m12) / s;
		rotation.y = (rotationMatrix._m02 - rotationMatrix._m20) / s;
		rotation.z = (rotationMatrix._m10 - rotationMatrix._m01) / s;
	}
	else if (rotationMatrix._m00 > rotationMatrix._m11 && rotationMatrix._m00 > rotationMatrix._m22)
	{
		float s	   = sqrt(1.0f + rotationMatrix._m00 - rotationMatrix._m11 - rotationMatrix._m22) * 2; // s = 4 * qx
		rotation.w = (rotationMatrix._m21 - rotationMatrix._m12) / s;
		rotation.x = 0.25f * s;
		rotation.y = (rotationMatrix._m01 + rotationMatrix._m10) / s;
		rotation.z = (rotationMatrix._m02 + rotationMatrix._m20) / s;
	}
	else if (rotationMatrix._m11 > rotationMatrix._m22)
	{
		float s	   = sqrt(1.0f + rotationMatrix._m11 - rotationMatrix._m00 - rotationMatrix._m22) * 2; // s = 4 * qy
		rotation.w = (rotationMatrix._m02 - rotationMatrix._m20) / s;
		rotation.x = (rotationMatrix._m01 + rotationMatrix._m10) / s;
		rotation.y = 0.25f * s;
		rotation.z = (rotationMatrix._m12 + rotationMatrix._m21) / s;
	}
	else
	{
		float s	   = sqrt(1.0f + rotationMatrix._m22 - rotationMatrix._m00 - rotationMatrix._m11) * 2; // s = 4 * qz
		rotation.w = (rotationMatrix._m10 - rotationMatrix._m01) / s;
		rotation.x = (rotationMatrix._m02 + rotationMatrix._m20) / s;
		rotation.y = (rotationMatrix._m12 + rotationMatrix._m21) / s;
		rotation.z = 0.25f * s;
	}
}
ozz::animation::Animation *Animation::GetOzzAnimation(uint64_t skeleton_hash)
{
	auto a=retargeted_animations.find(skeleton_hash);
	if(a==retargeted_animations.end())
		return nullptr;
	return a->second.get();
}

void Animation::Retarget(std::shared_ptr<Skeleton> target_skeleton)
{
	if (!target_skeleton->GetRawSkeleton())
	{
		return;
	}
	if(retargeted_animations[target_skeleton->id])
		return;
	ozz::animation::offline::RawAnimation retargeted_raw_animation = *raw_animation;

	// Load your skeletons and animation
	ozz::animation::offline::RawAnimation source_animation;

	// Retarget the animation
	auto retargeted_animation = clientrender::RetargetAnimation(*raw_animation, *raw_skeleton, *(target_skeleton->GetRawSkeleton()));
	for(size_t i=0;i<100;i++)
		clientrender::RetargetAnimation(*raw_animation, *raw_skeleton, *(target_skeleton->GetRawSkeleton()));
	// Validate and convert to runtime format
	if (retargeted_animation.Validate())
	{
		//////////////////////////////////////////////////////////////////////////////
		// This final section converts the RawAnimation to a runtime Animation.
		//////////////////////////////////////////////////////////////////////////////

		// Creates a AnimationBuilder instance.
		ozz::animation::offline::AnimationBuilder builder;

		// Executes the builder on the previously prepared RawAnimation, which returns
		// a new runtime animation instance.
		// This operation will fail and return an empty unique_ptr if the RawAnimation
		// isn't valid.
		retargeted_animations[target_skeleton->id] = builder(retargeted_animation);
	}
}