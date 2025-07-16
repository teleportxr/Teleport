#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>

namespace teleport
{
	namespace clientrender
	{
		extern ozz::math::Transform ComputeModelSpaceTransform(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &joint_name);
		extern std::unordered_map<ozz::string, int> BuildJointMap(const ozz::animation::offline::RawSkeleton &skeleton, ozz::vector<ozz::math::Transform> &modelspace_transforms
			, ozz::vector<const ozz::animation::offline::RawSkeleton::Joint*> &joint_list);
		extern std::string getMappedBoneName(const std::string &bName);
		extern ozz::string getMappedBoneName(const ozz::string &bName);
		// Main retargeting function
		ozz::animation::offline::RawAnimation RetargetAnimation(const ozz::animation::offline::RawAnimation &source_animation,
																const ozz::animation::offline::RawSkeleton	&source_skeleton,
																const ozz::animation::offline::RawSkeleton	&target_skeleton);

	}
}