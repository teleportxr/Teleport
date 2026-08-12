#include "AnimationRetargeter.h"
#include "TeleportCore/Logging.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

// For operator ""s
using namespace std::literals;
using namespace std::string_literals;
using namespace std::literals::string_literals;

using namespace teleport::clientrender;

template<typename str, typename u>
void replace_all(
    str& s,
    const u& toReplace,
    const u& replaceWith
) {
    str buf;
    std::size_t pos = 0;
    std::size_t prevPos;

    // Reserves rough estimate of final size of string.
    buf.reserve(s.size());

    while (true) {
        prevPos = pos;
        pos = s.find(toReplace, pos);
        if (pos == str::npos)
            break;
        buf.append(s, prevPos, pos - prevPos);
        buf += replaceWith;
        pos += toReplace.size();
    }

    buf.append(s, prevPos, s.size() - prevPos);
    s.swap(buf);
}


/*
* The VRM Bone Names are:
hips
spine
chest
upperChest
neck
head
leftEye
rightEye
jaw

leftUpperLeg
leftLowerLeg
leftFoot
leftToes
rightUpperLeg
rightLowerLeg
rightFoot
rightToes

leftShoulder
leftUpperArm
leftLowerArm
leftHand
rightShoulder
rightUpperArm
rightLowerArm
rightHand

leftThumbMetacarpal
leftThumbProximal
leftThumbDistal
leftIndexProximal
leftIndexIntermediate
leftIndexDistal
leftMiddleProximal
leftMiddleIntermediate
leftMiddleDistal
leftRingProximal
leftRingIntermediate
leftRingDistal
leftLittleProximal
leftLittleIntermediate
leftLittleDistal
rightThumbMetacarpal
rightThumbProximal
rightThumbDistal
rightIndexProximal
rightIndexIntermediate
rightIndexDistal
rightMiddleProximal
rightMiddleIntermediate
rightMiddleDistal
rightRingProximal
rightRingIntermediate
rightRingDistal
rightLittleProximal
rightLittleIntermediate
rightLittleDistal
*/
template <typename stringType> stringType GetMappedBoneName(const stringType &bName)
{
	static std::unordered_map<stringType, stringType> mapping;
	if (!mapping.size())
	{
		mapping["spine1"]		= "chest";
		mapping["spine2"]		= "upperchest";
		mapping["back"]			= "upperchest";

		// arms - with numbered prefix
		mapping["leftarm"]		= "leftupperarm";
		mapping["leftforearm"]	= "leftlowerarm";
		mapping["rightarm"]		= "rightupperarm";
		mapping["rightforearm"] = "rightlowerarm";

		mapping["leftupleg"]	= "leftupperleg";
		mapping["leftleg"]		= "leftlowerleg";
		mapping["lefttoebase"]	= "lefttoes";
		mapping["rightupleg"]	= "rightupperleg";
		mapping["rightleg"]		= "rightlowerleg";
		mapping["righttoebase"] = "righttoes";
	}
	// Every role this understands, so a candidate reading can be tested rather than assumed.
	static std::set<stringType> roles;
	if (!roles.size())
	{
		for (const char *r : {"hips",			"spine",		 "chest",		  "upperchest",	   "neck",			"head",			 "jaw",
							  "lefteye",		"righteye",		 "leftshoulder",  "leftupperarm",  "leftlowerarm",	"lefthand",		 "rightshoulder",
							  "rightupperarm",	"rightlowerarm", "righthand",	  "leftupperleg",  "leftlowerleg",	"leftfoot",		 "lefttoes",
							  "rightupperleg",	"rightlowerleg", "rightfoot",	  "righttoes"})
		{
			roles.insert(stringType(r));
		}
	}
	stringType n = bName;
	std::transform(n.begin(), n.end(), n.begin(), ::tolower);
	// Namespaces: "mixamorig:LeftArm", "Armature|Hips".
	size_t ns = n.find_last_of(":|");
	if (ns != stringType::npos)
	{
		n = n.substr(ns + 1);
	}
	replace_all(n, "bip_c_"s, ""s);
	replace_all(n, "bip_"s, ""s);
	replace_all(n, "mixamorig"s, ""s);
	replace_all(n, "_l_"s, "_left"s);
	replace_all(n, "_r_"s, "_right"s);

	// Trailing index, e.g. Ready Player Me's "Hips_01". Only stripped behind a separator:
	// "Spine1" is the chest, while "Spine_1" is the first joint of a chain.
	for (;;)
	{
		size_t i = n.find_last_not_of("0123456789"s);
		if (i == stringType::npos || i + 1 >= n.length() || (n[i] != '_' && n[i] != '.' && n[i] != '-' && n[i] != ' '))
		{
			break;
		}
		n = n.substr(0, i);
	}

	// Where the meaningful part sits differs by convention - a prefix in "Avatar_Hips", the
	// head of the name in "Hips_01" - so try each reading and take the first that is a role
	// this knows. Guessing one position outright is what made "Hips_01" resolve to "01".
	auto stripSeparators = [](stringType s)
	{
		stringType out;
		for (auto c : s)
		{
			if (c != '_' && c != '.' && c != '-' && c != ' ')
			{
				out.push_back(c);
			}
		}
		return out;
	};
	stringType			   whole = stripSeparators(n);
	std::vector<stringType> candidates{whole};
	size_t				   first = n.find('_');
	if (first != stringType::npos)
	{
		candidates.push_back(stripSeparators(n.substr(first + 1)));
	}
	size_t last = n.find_last_of('_');
	if (last != stringType::npos && last != first)
	{
		candidates.push_back(stripSeparators(n.substr(last + 1)));
	}
	for (const stringType &c : candidates)
	{
		if (c.empty())
		{
			continue;
		}
		auto m = mapping.find(c);
		if (m != mapping.end())
		{
			return m->second;
		}
		if (roles.find(c) != roles.end())
		{
			return c;
		}
	}
	// Nothing recognised: the whole name, so two rigs sharing an unknown convention still
	// match each other.
	return whole;
};
std::string teleport::clientrender::getMappedBoneName(const std::string &bName)
{
	return GetMappedBoneName(bName);
}
ozz::string teleport::clientrender::getMappedBoneName(const ozz::string &bName)
{
	return GetMappedBoneName(bName);
}

// Helper function to recursively find joint chain
bool FindJointChainRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>	  &joints,
							 const ozz::string												  &target,
							 ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> &chain)
{
	for (const auto &joint : joints)
	{
		chain.push_back(&joint);
		if (GetMappedBoneName(joint.name) == GetMappedBoneName(target))
		{
			return true; // Found target
		}
		if (FindJointChainRecursive(joint.children, target, chain))
		{
			return true; // Found in children
		}
		chain.pop_back(); // Backtrack
	}
	return false;
}

// Combine a parent model-space transform with a child's local transform: result = parent * local.
static ozz::math::Transform CombineTransforms(const ozz::math::Transform &parent, const ozz::math::Transform &local)
{
	ozz::math::Transform result;
	ozz::math::Float3 scaled_translation(local.translation.x * parent.scale.x,
										 local.translation.y * parent.scale.y,
										 local.translation.z * parent.scale.z);
	result.translation = parent.translation + ozz::math::TransformVector(parent.rotation, scaled_translation);
	result.rotation	   = parent.rotation * local.rotation;
	result.scale	   = ozz::math::Float3(parent.scale.x * local.scale.x, parent.scale.y * local.scale.y, parent.scale.z * local.scale.z);
	return result;
}

// Helper function to compute model space transform from root to a specific joint
ozz::math::Transform teleport::clientrender::ComputeModelSpaceTransform(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &joint_name)
{
	ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> chain;

	if (!FindJointChainRecursive(skeleton.roots, joint_name, chain))
	{
		// Joint not found, return identity
		return ozz::math::Transform::identity();
	}

	// Accumulate transforms from root to target
	ozz::math::Transform result = ozz::math::Transform::identity();
	for (const auto *joint : chain)
	{
		result = CombineTransforms(result, joint->transform);
	}
	return result;
}

// Helper function to traverse joints and build joint map
void TraverseJoints(const ozz::animation::offline::RawSkeleton &skeleton
	, const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints
	, std::unordered_map<ozz::string, int> &joint_map
	, ozz::vector<ozz::math::Transform> &modelspace_transforms
	, ozz::vector<const ozz::animation::offline::RawSkeleton::Joint*> &joint_list
	, int &joint_index)
{
	for (const auto &joint : joints)
	{
		ozz::string mapped_bone_name=GetMappedBoneName(joint.name);
		if(joint_map.find(mapped_bone_name)!=joint_map.end())
		{
			TELEPORT_WARN("Two mappings found for {}.", mapped_bone_name);
		}
		else
		{
			joint_map[mapped_bone_name] = joint_index;
		}
		if(joint_index>=modelspace_transforms.size())
			modelspace_transforms.resize(joint_index+1);
		modelspace_transforms[joint_index]=ComputeModelSpaceTransform(skeleton, mapped_bone_name);
		joint_list.push_back(&joint);
		joint_index++;
		TraverseJoints(skeleton, joint.children, joint_map, modelspace_transforms, joint_list, joint_index);
	}
}

// Helper function to build a joint name to index mapping
std::unordered_map<ozz::string, int> teleport::clientrender::BuildJointMap(
	const ozz::animation::offline::RawSkeleton &skeleton
	, ozz::vector<ozz::math::Transform> &modelspace_transforms
	, ozz::vector<const ozz::animation::offline::RawSkeleton::Joint*> &joint_list)
{
	std::unordered_map<ozz::string, int> joint_map;
	int									 joint_index = 0;
	TraverseJoints(skeleton, skeleton.roots, joint_map, modelspace_transforms, joint_list, joint_index);
	return joint_map;
}

namespace
{
	// Depth-first index of a raw skeleton. The traversal order matches ozz's SkeletonBuilder
	// (IterateJointsDF), and therefore the runtime skeleton's joint order and the track order
	// of animations imported against it.
	struct SkeletonIndex
	{
		std::unordered_map<ozz::string, int>							 jointMap;		// mapped bone name -> depth-first index (first occurrence wins)
		ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> jointList;		// depth-first order
		ozz::vector<int>												 parentIndices; // depth-first order, -1 for roots
		ozz::vector<ozz::math::Transform>								 bindModel;		// accumulated bind-pose model-space transforms
		ozz::vector<ozz::string>										 mappedNames;	// depth-first order
	};

	void BuildSkeletonIndexRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints,
									 int parentIndex,
									 const ozz::math::Transform &parentModel,
									 SkeletonIndex &index)
	{
		for (const auto &joint : joints)
		{
			const int	jointIndex = (int)index.jointList.size();
			ozz::string mappedName = GetMappedBoneName(joint.name);
			if (index.jointMap.find(mappedName) != index.jointMap.end())
			{
				TELEPORT_WARN("Duplicate mapped bone name {}; keeping the first occurrence.", mappedName.c_str());
			}
			else
			{
				index.jointMap[mappedName] = jointIndex;
			}
			const ozz::math::Transform model = CombineTransforms(parentModel, joint.transform);
			index.jointList.push_back(&joint);
			index.parentIndices.push_back(parentIndex);
			index.mappedNames.push_back(mappedName);
			index.bindModel.push_back(model);
			BuildSkeletonIndexRecursive(joint.children, jointIndex, model, index);
		}
	}

	SkeletonIndex BuildSkeletonIndex(const ozz::animation::offline::RawSkeleton &skeleton)
	{
		SkeletonIndex index;
		BuildSkeletonIndexRecursive(skeleton.roots, -1, ozz::math::Transform::identity(), index);
		return index;
	}

	// Static per-target-track retargeting constants.
	//
	// Derivation: proper hierarchical retargeting defines, per sample time t and in bind-pose
	// model space (B = bind model rotation, q = animated local rotation, S/T = source/target,
	// R = the facing-alignment rotation taking the source rig's bind facing onto the target's):
	//   RS_j(t) = RS_parent(j)(t) * qS_j(t)              (animated source model rotation)
	//   dW_j(t) = RS_j(t) * conj(BS_j)                   (world-space delta from bind)
	//   RT_j(t) = R * dW_j(t) * conj(R) * BT_j           (delta re-expressed in the target's facing)
	//   qT_j(t) = conj(RT_parent(j)(t)) * RT_j(t)        (back to target local space)
	// Substituting RT_parent(t) = R * dW_parent(t) * conj(R) * BT_parent, the animated parent
	// chain cancels exactly, leaving a purely local per-key formula with static coefficients:
	//   qT_j(t) = preRotation * qS_j(t) * postRotation
	//   preRotation  = conj(BT_parent(j)) * R * BS_parent(j)
	//   postRotation = conj(BS_j) * conj(R) * BT_j
	// This requires the two rigs to be in the same bind pose (a T-pose) but NOT to share a
	// facing direction (VRM 0.x avatars face -Z in glTF while VRMA animations face +Z: R is
	// then a 180-degree yaw), and NOT to have matching bind local rotations (VRM 1.0 and
	// Mixamo-style rigs need not have identity bind rotations) - the two respects in which the
	// previous implementation went wrong.
	//
	// When the source has animated joints between j's mapped ancestor and j that have no
	// target counterpart, their animated locals must be folded in, ancestor-first:
	//   qT_j(t) = conj(BT_parent) * R * BS_anchor * qS_k1(t) * ... * qS_kn(t) * qS_j(t) * conj(BS_j) * conj(R) * BT_j
	// (at bind pose BS_anchor * bS_k1 * ... * bS_kn = BS_parent(j), recovering the simple form).
	struct TrackRetarget
	{
		bool				  valid				  = false;
		int					  sourceTrack		  = -1;
		ozz::vector<int>	  foldedSourceTracks; // source joints strictly between the anchor and sourceTrack, ancestor-first
		ozz::math::Quaternion preRotation		  = ozz::math::Quaternion::identity();
		ozz::math::Quaternion postRotation		  = ozz::math::Quaternion::identity();
		bool				  retargetTranslation = false; // hips and roots only: other joints keep the target's bind translations
		ozz::math::Quaternion alignRotation		  = ozz::math::Quaternion::identity(); // source-to-target facing alignment, for translation deltas
		ozz::math::Transform  sourceParentBindModel = ozz::math::Transform::identity();
		ozz::math::Transform  targetParentBindModel = ozz::math::Transform::identity();
		ozz::math::Float3	  sourceBindModelPos	= ozz::math::Float3(0.0f, 0.0f, 0.0f);
		ozz::math::Float3	  targetBindModelPos	= ozz::math::Float3(0.0f, 0.0f, 0.0f);
		float				  heightScale			= 1.0f;
		ozz::math::Float3	  sourceBindLocalScale	= ozz::math::Float3(1.0f, 1.0f, 1.0f);
		ozz::math::Float3	  targetBindLocalScale	= ozz::math::Float3(1.0f, 1.0f, 1.0f);
	};

	// Yaw rotation aligning the source rig's bind facing with the target's, derived from the
	// lateral (left-to-right) axis of paired limb joints. Facing cannot be read from bind
	// rotations - fully normalised rigs have identity rotations everywhere and encode their
	// facing purely in the bind translations. Engineering space: z is up, yaw is about z.
	ozz::math::Quaternion ComputeAlignmentRotation(const SkeletonIndex &src, const SkeletonIndex &tgt)
	{
		static const std::pair<const char *, const char *> lateralPairs[] = {
			{"leftupperleg", "rightupperleg"},
			{"leftupperarm", "rightupperarm"},
			{"leftshoulder", "rightshoulder"},
			{"lefthand", "righthand"},
		};
		for (const auto &[leftName, rightName] : lateralPairs)
		{
			auto srcLeft  = src.jointMap.find(leftName);
			auto srcRight = src.jointMap.find(rightName);
			auto tgtLeft  = tgt.jointMap.find(leftName);
			auto tgtRight = tgt.jointMap.find(rightName);
			if (srcLeft == src.jointMap.end() || srcRight == src.jointMap.end() || tgtLeft == tgt.jointMap.end() || tgtRight == tgt.jointMap.end())
			{
				continue;
			}
			const ozz::math::Float3 srcLateral = src.bindModel[srcRight->second].translation - src.bindModel[srcLeft->second].translation;
			const ozz::math::Float3 tgtLateral = tgt.bindModel[tgtRight->second].translation - tgt.bindModel[tgtLeft->second].translation;
			// Project onto the horizontal plane; skip degenerate pairs.
			const float srcLength = sqrtf(srcLateral.x * srcLateral.x + srcLateral.y * srcLateral.y);
			const float tgtLength = sqrtf(tgtLateral.x * tgtLateral.x + tgtLateral.y * tgtLateral.y);
			if (srcLength < 1e-5f || tgtLength < 1e-5f)
			{
				continue;
			}
			const float dot	  = srcLateral.x * tgtLateral.x + srcLateral.y * tgtLateral.y;
			const float cross = srcLateral.x * tgtLateral.y - srcLateral.y * tgtLateral.x;
			const float yaw	  = atan2f(cross, dot);
			if (fabsf(yaw) > 0.01f)
			{
				TELEPORT_INFO("Retargeting: rotating source rig {} degrees about vertical to match the target's facing.", yaw * 180.0f / 3.14159265f);
			}
			const float half = yaw * 0.5f;
			return ozz::math::Quaternion(0.0f, 0.0f, sinf(half), cosf(half));
		}
		return ozz::math::Quaternion::identity();
	}

	ozz::vector<TrackRetarget> BuildTrackRetargets(const SkeletonIndex &src, const SkeletonIndex &tgt)
	{
		const ozz::math::Quaternion alignRotation	  = ComputeAlignmentRotation(src, tgt);
		const ozz::math::Quaternion alignRotationConj = ozz::math::Conjugate(alignRotation);
		ozz::vector<TrackRetarget>	retargets(tgt.jointList.size());
		for (size_t i = 0; i < tgt.jointList.size(); ++i)
		{
			TrackRetarget &tr		= retargets[i];
			auto		   sourceIt = src.jointMap.find(tgt.mappedNames[i]);
			if (sourceIt == src.jointMap.end())
			{
				// No source counterpart: the caller emits bind-pose keys.
				continue;
			}
			const int sj = sourceIt->second;

			// Find the nearest target ancestor that also exists in the source: the anchor.
			int sa = -1;
			for (int p = tgt.parentIndices[i]; p >= 0; p = tgt.parentIndices[p])
			{
				auto it = src.jointMap.find(tgt.mappedNames[p]);
				if (it != src.jointMap.end())
				{
					sa = it->second;
					break;
				}
			}

			// Collect source joints strictly between the anchor and sj, whose animated locals
			// must be folded into this track because the target has no joints for them.
			// sa == -1 means the anchor is the space above the source roots, reached when p
			// runs off the root (-1).
			bool reached = false;
			for (int p = src.parentIndices[sj];;)
			{
				if (p == sa)
				{
					reached = true;
					break;
				}
				if (p < 0)
				{
					break;
				}
				tr.foldedSourceTracks.push_back(p);
				p = src.parentIndices[p];
			}
			if (!reached)
			{
				TELEPORT_WARN("Bone {}: its mapped ancestor is not an ancestor in the source skeleton; using bind pose.", tgt.mappedNames[i].c_str());
				tr.foldedSourceTracks.clear();
				continue;
			}
			std::reverse(tr.foldedSourceTracks.begin(), tr.foldedSourceTracks.end());

			const int					tp					  = tgt.parentIndices[i];
			const ozz::math::Transform &targetParentBindModel = tp >= 0 ? tgt.bindModel[tp] : ozz::math::Transform::identity();
			const ozz::math::Quaternion sourceAnchorRotation  = sa >= 0 ? src.bindModel[sa].rotation : ozz::math::Quaternion::identity();

			tr.sourceTrack			 = sj;
			tr.alignRotation		 = alignRotation;
			tr.preRotation			 = ozz::math::Conjugate(targetParentBindModel.rotation) * alignRotation * sourceAnchorRotation;
			tr.postRotation			 = ozz::math::Conjugate(src.bindModel[sj].rotation) * alignRotationConj * tgt.bindModel[i].rotation;
			tr.targetParentBindModel = targetParentBindModel;
			const int sp			 = src.parentIndices[sj];
			tr.sourceParentBindModel = sp >= 0 ? src.bindModel[sp] : ozz::math::Transform::identity();
			tr.sourceBindModelPos	 = src.bindModel[sj].translation;
			tr.targetBindModelPos	 = tgt.bindModel[i].translation;
			tr.sourceBindLocalScale	 = src.jointList[sj]->transform.scale;
			tr.targetBindLocalScale	 = tgt.jointList[i]->transform.scale;
			tr.retargetTranslation	 = (tgt.mappedNames[i] == "hips") || tp < 0;
			if (tr.retargetTranslation)
			{
				// Scale the source's motion by the rigs' relative height. Bind model z is up in
				// Engineering space; fall back to the bind translation length ratio, then to 1.
				const float sourceHeight = src.bindModel[sj].translation.z;
				const float targetHeight = tgt.bindModel[i].translation.z;
				if (std::abs(sourceHeight) > 1e-5f)
				{
					tr.heightScale = targetHeight / sourceHeight;
				}
				else
				{
					const float sourceLength = ozz::math::Length(src.bindModel[sj].translation);
					const float targetLength = ozz::math::Length(tgt.bindModel[i].translation);
					tr.heightScale			 = sourceLength > 1e-5f ? targetLength / sourceLength : 1.0f;
				}
			}
			tr.valid = true;
		}
		return retargets;
	}

	// Sample a track's rotation at an arbitrary time: bracketing keys with shortest-path NLerp,
	// matching the ozz runtime sampler. An empty track holds the bind local rotation.
	ozz::math::Quaternion SampleTrackRotation(const ozz::animation::offline::RawAnimation::JointTrack &track,
											  float time,
											  const ozz::math::Quaternion &bindLocalRotation)
	{
		const auto &keys = track.rotations;
		if (keys.empty())
		{
			return bindLocalRotation;
		}
		if (time <= keys.front().time)
		{
			return keys.front().value;
		}
		if (time >= keys.back().time)
		{
			return keys.back().value;
		}
		size_t next = 1;
		while (keys[next].time < time)
		{
			++next;
		}
		const auto &k0	  = keys[next - 1];
		const auto &k1	  = keys[next];
		const float span  = k1.time - k0.time;
		const float alpha = span > 0.0f ? (time - k0.time) / span : 0.0f;
		ozz::math::Quaternion b = k1.value;
		const float dot = k0.value.x * b.x + k0.value.y * b.y + k0.value.z * b.z + k0.value.w * b.w;
		if (dot < 0.0f)
		{
			b = ozz::math::Quaternion(-b.x, -b.y, -b.z, -b.w);
		}
		return ozz::math::NLerp(k0.value, b, alpha);
	}

	// Emit a single key per channel holding the given local transform.
	void EmitBindPoseKeys(ozz::animation::offline::RawAnimation::JointTrack &track, const ozz::math::Transform &bindLocal)
	{
		track.translations.push_back({0.0f, bindLocal.translation});
		track.rotations.push_back({0.0f, bindLocal.rotation});
		track.scales.push_back({0.0f, bindLocal.scale});
	}
}

// Main retargeting function
ozz::animation::offline::RawAnimation teleport::clientrender::RetargetAnimation(const ozz::animation::offline::RawAnimation &source_animation,
														const ozz::animation::offline::RawSkeleton	&source_skeleton,
														const ozz::animation::offline::RawSkeleton	&target_skeleton)
{
	SkeletonIndex src = BuildSkeletonIndex(source_skeleton);
	SkeletonIndex tgt = BuildSkeletonIndex(target_skeleton);
	TELEPORT_INFO("Retargeting animation from {} to {} joints", src.jointList.size(), tgt.jointList.size());

	ozz::animation::offline::RawAnimation targetAnimation;
	targetAnimation.duration = source_animation.duration;
	targetAnimation.name	 = source_animation.name + "_retargeted";
	targetAnimation.tracks.resize(tgt.jointList.size());

	// Source tracks must be in the source skeleton's depth-first joint order (see SkeletonIndex).
	if (source_animation.tracks.size() != src.jointList.size())
	{
		TELEPORT_WARN("Animation {} has {} tracks but its skeleton has {} joints; emitting bind pose.",
					  source_animation.name.c_str(), source_animation.tracks.size(), src.jointList.size());
		for (size_t i = 0; i < targetAnimation.tracks.size(); ++i)
		{
			EmitBindPoseKeys(targetAnimation.tracks[i], tgt.jointList[i]->transform);
		}
		return targetAnimation;
	}

	ozz::vector<TrackRetarget> trackRetargets = BuildTrackRetargets(src, tgt);
	for (size_t i = 0; i < tgt.jointList.size(); ++i)
	{
		const TrackRetarget		   &tr			   = trackRetargets[i];
		auto					   &targetTrack	   = targetAnimation.tracks[i];
		const ozz::math::Transform &targetBindLocal = tgt.jointList[i]->transform;

		if (!tr.valid)
		{
			// No source counterpart: hold the target's bind-pose local transform.
			EmitBindPoseKeys(targetTrack, targetBindLocal);
			continue;
		}
		const auto &sourceTrack = source_animation.tracks[tr.sourceTrack];

		// Rotations: qT(t) = pre * (folded source locals)(t) * qS(t) * post.
		if (tr.foldedSourceTracks.empty())
		{
			for (const auto &key : sourceTrack.rotations)
			{
				targetTrack.rotations.push_back({key.time, ozz::math::Normalize(tr.preRotation * key.value * tr.postRotation)});
			}
		}
		else
		{
			// Emit keys at the union of the primary and folded tracks' key times, so an
			// animated folded joint is not undersampled at the primary track's key times.
			ozz::vector<float> keyTimes;
			for (const auto &key : sourceTrack.rotations)
			{
				keyTimes.push_back(key.time);
			}
			for (int folded : tr.foldedSourceTracks)
			{
				for (const auto &key : source_animation.tracks[folded].rotations)
				{
					keyTimes.push_back(key.time);
				}
			}
			std::sort(keyTimes.begin(), keyTimes.end());
			keyTimes.erase(std::unique(keyTimes.begin(), keyTimes.end()), keyTimes.end());
			for (float time : keyTimes)
			{
				ozz::math::Quaternion q = tr.preRotation;
				for (int folded : tr.foldedSourceTracks)
				{
					q = q * SampleTrackRotation(source_animation.tracks[folded], time, src.jointList[folded]->transform.rotation);
				}
				q = q * SampleTrackRotation(sourceTrack, time, src.jointList[tr.sourceTrack]->transform.rotation);
				targetTrack.rotations.push_back({time, ozz::math::Normalize(q * tr.postRotation)});
			}
		}
		if (targetTrack.rotations.empty())
		{
			targetTrack.rotations.push_back({0.0f, targetBindLocal.rotation});
		}

		// Translations: only the hips (or a root) inherits the source's motion, height-scaled;
		// every other joint keeps the target's bind translation so its proportions are preserved.
		if (tr.retargetTranslation && !sourceTrack.translations.empty())
		{
			for (const auto &key : sourceTrack.translations)
			{
				// Source model-space position, composed through the bind parent chain: valid
				// because hips/root ancestors are not themselves animated in humanoid rigs.
				const ozz::math::Transform &sp = tr.sourceParentBindModel;
				ozz::math::Float3			scaled(key.value.x * sp.scale.x, key.value.y * sp.scale.y, key.value.z * sp.scale.z);
				ozz::math::Float3			sourceModelPos = sp.translation + ozz::math::TransformVector(sp.rotation, scaled);
				ozz::math::Float3			delta		   = ozz::math::TransformVector(tr.alignRotation, sourceModelPos - tr.sourceBindModelPos);
				ozz::math::Float3			targetModelPos(tr.targetBindModelPos.x + delta.x * tr.heightScale,
														   tr.targetBindModelPos.y + delta.y * tr.heightScale,
														   tr.targetBindModelPos.z + delta.z * tr.heightScale);
				// Back to local space through the target parent's bind model transform.
				const ozz::math::Transform &tp	   = tr.targetParentBindModel;
				ozz::math::Float3			offset = ozz::math::TransformVector(ozz::math::Conjugate(tp.rotation), targetModelPos - tp.translation);
				ozz::math::Float3			local(tp.scale.x != 0.0f ? offset.x / tp.scale.x : offset.x,
												  tp.scale.y != 0.0f ? offset.y / tp.scale.y : offset.y,
												  tp.scale.z != 0.0f ? offset.z / tp.scale.z : offset.z);
				targetTrack.translations.push_back({key.time, local});
			}
		}
		else
		{
			targetTrack.translations.push_back({0.0f, targetBindLocal.translation});
		}

		// Scales: apply the source's scale ratio from bind to the target's bind scale.
		for (const auto &key : sourceTrack.scales)
		{
			ozz::math::Float3 ratio(tr.sourceBindLocalScale.x != 0.0f ? key.value.x / tr.sourceBindLocalScale.x : 1.0f,
									tr.sourceBindLocalScale.y != 0.0f ? key.value.y / tr.sourceBindLocalScale.y : 1.0f,
									tr.sourceBindLocalScale.z != 0.0f ? key.value.z / tr.sourceBindLocalScale.z : 1.0f);
			targetTrack.scales.push_back({key.time,
										  ozz::math::Float3(tr.targetBindLocalScale.x * ratio.x,
															tr.targetBindLocalScale.y * ratio.y,
															tr.targetBindLocalScale.z * ratio.z)});
		}
		if (targetTrack.scales.empty())
		{
			targetTrack.scales.push_back({0.0f, targetBindLocal.scale});
		}
	}

	return targetAnimation;
}
