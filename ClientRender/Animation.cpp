#include "Animation.h"

#include "Node.h"
#include "Transform.h"

#define tinygltf teleport_tinygltf
#include "tiny_gltf.h"
#include <json.hpp>
#include <ozz/base/containers/set.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>

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
	ozz::animation::offline::RawAnimation raw_animation;

	// Sets animation duration (to 1.4s).
	// All the animation keyframes times must be within range [0, duration].
	raw_animation.duration = duration;

	// Creates 3 animation tracks.
	// There should be as many tracks as there are joints in the skeleton that
	// this animation targets.
	raw_animation.tracks.resize(boneKeyframeLists.size());

	// Fills each track with keyframes, in joint local-space.
	// Tracks should be ordered in the same order as joints in the
	// ozz::animation::Skeleton. Joint's names can be used to find joint's
	// index in the skeleton.

	// Fills 1st track with 2 translation keyframes.
	for (int i = 0; i < boneKeyframeLists.size(); i++)
	{
		BoneKeyframeList &b		= boneKeyframeLists[i];
		auto			 &track = raw_animation.tracks[i];
		for (const auto & c : b.positionKeyframes)
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
	//  2. Keyframes' are not sorted in a strict ascending order.
	//  3. Keyframes' are not within [0, duration] range.
	if (!raw_animation.Validate())
	// Test for animation validity. These are the errors that could invalidate
	// an animation:
	//  1. Animation duration is less than 0.
	{
		return;
	}

	//////////////////////////////////////////////////////////////////////////////
	// This final section converts the RawAnimation to a runtime Animation.
	//////////////////////////////////////////////////////////////////////////////

	// Creates a AnimationBuilder instance.
	ozz::animation::offline::AnimationBuilder builder;

	// Executes the builder on the previously prepared RawAnimation, which returns
	// a new runtime animation instance.
	// This operation will fail and return an empty unique_ptr if the RawAnimation
	// isn't valid.
	animation = builder(raw_animation);
}
