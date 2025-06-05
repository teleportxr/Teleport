#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/math_ex.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace teleport
{
namespace clientrender
{
	// Main retargeting function
	ozz::animation::offline::RawAnimation RetargetAnimation(const ozz::animation::offline::RawAnimation &source_animation,
															const ozz::animation::offline::RawSkeleton	&source_skeleton,
															const ozz::animation::offline::RawSkeleton	&target_skeleton);

}
}