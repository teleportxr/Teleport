// Behavioural tests for the animation retargeter in ClientRender/AnimationRetargeter.cpp.
//
// The retargeter maps an animation authored against one rig onto another rig whose bind
// pose may use different local rotations (e.g. a normalised VRMA source onto a VRM 1.0
// avatar). The invariant these tests pin down is model-space equivalence: joints of the
// retargeted animation must trace the same world-space positions and orientations as the
// source, provided the two rigs share the same world-space bind pose.

#include <catch2/catch_test_macros.hpp>

#include "ClientRender/AnimationRetargeter.h"

#include <cmath>
#include <vector>

using namespace teleport::clientrender;
using ozz::animation::offline::RawAnimation;
using ozz::animation::offline::RawSkeleton;
using ozz::math::Float3;
using ozz::math::Quaternion;

namespace
{
	constexpr float kPi		   = 3.14159265358979323846f;
	constexpr float kTolerance = 1e-4f;

	Quaternion AxisAngle(const Float3 &axis, float angle)
	{
		const float half = angle * 0.5f;
		const float s	 = sinf(half);
		return Quaternion(axis.x * s, axis.y * s, axis.z * s, cosf(half));
	}

	RawSkeleton::Joint MakeJoint(const char *name, const Float3 &translation, const Quaternion &rotation = Quaternion::identity())
	{
		RawSkeleton::Joint joint;
		joint.name					= name;
		joint.transform.translation = translation;
		joint.transform.rotation	= rotation;
		joint.transform.scale		= Float3(1.0f, 1.0f, 1.0f);
		return joint;
	}

	// Depth-first flatten, mirroring the joint/track ordering the retargeter relies on.
	void Flatten(const ozz::vector<RawSkeleton::Joint> &joints, int parentIndex, std::vector<const RawSkeleton::Joint *> &list, std::vector<int> &parents)
	{
		for (const auto &joint : joints)
		{
			const int index = (int)list.size();
			list.push_back(&joint);
			parents.push_back(parentIndex);
			Flatten(joint.children, index, list, parents);
		}
	}

	Quaternion NLerpShortest(const Quaternion &a, Quaternion b, float alpha)
	{
		const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		if (dot < 0.0f)
		{
			b = Quaternion(-b.x, -b.y, -b.z, -b.w);
		}
		return ozz::math::NLerp(a, b, alpha);
	}

	Float3 SampleTranslation(const RawAnimation::JointTrack &track, float time, const Float3 &fallback)
	{
		const auto &keys = track.translations;
		if (keys.empty())
		{
			return fallback;
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
		const float span  = keys[next].time - keys[next - 1].time;
		const float alpha = span > 0.0f ? (time - keys[next - 1].time) / span : 0.0f;
		return ozz::math::Lerp(keys[next - 1].value, keys[next].value, alpha);
	}

	Quaternion SampleRotation(const RawAnimation::JointTrack &track, float time, const Quaternion &fallback)
	{
		const auto &keys = track.rotations;
		if (keys.empty())
		{
			return fallback;
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
		const float span  = keys[next].time - keys[next - 1].time;
		const float alpha = span > 0.0f ? (time - keys[next - 1].time) / span : 0.0f;
		return NLerpShortest(keys[next - 1].value, keys[next].value, alpha);
	}

	struct ModelTransform
	{
		Float3	   translation;
		Quaternion rotation;
	};

	// Sample every track at the given time and accumulate model-space transforms down the rig.
	std::vector<ModelTransform> ModelPose(const RawSkeleton &skeleton, const RawAnimation &animation, float time)
	{
		std::vector<const RawSkeleton::Joint *> list;
		std::vector<int>						parents;
		Flatten(skeleton.roots, -1, list, parents);
		REQUIRE(animation.tracks.size() == list.size());

		std::vector<ModelTransform> pose(list.size());
		for (size_t i = 0; i < list.size(); ++i)
		{
			const Float3	 localTranslation = SampleTranslation(animation.tracks[i], time, list[i]->transform.translation);
			const Quaternion localRotation	  = SampleRotation(animation.tracks[i], time, list[i]->transform.rotation);
			if (parents[i] < 0)
			{
				pose[i] = {localTranslation, localRotation};
			}
			else
			{
				const ModelTransform &parent = pose[parents[i]];
				pose[i].translation			 = parent.translation + ozz::math::TransformVector(parent.rotation, localTranslation);
				pose[i].rotation			 = parent.rotation * localRotation;
			}
		}
		return pose;
	}

	void RequireNear(const Float3 &a, const Float3 &b)
	{
		REQUIRE(std::abs(a.x - b.x) < kTolerance);
		REQUIRE(std::abs(a.y - b.y) < kTolerance);
		REQUIRE(std::abs(a.z - b.z) < kTolerance);
	}

	// Quaternions are equal as rotations up to sign.
	void RequireSameRotation(const Quaternion &a, const Quaternion &b)
	{
		const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		REQUIRE(std::abs(dot) > 1.0f - kTolerance);
	}

	// An animation with one track per joint, every track holding single bind-pose keys.
	RawAnimation BindPoseAnimation(const RawSkeleton &skeleton, float duration)
	{
		std::vector<const RawSkeleton::Joint *> list;
		std::vector<int>						parents;
		Flatten(skeleton.roots, -1, list, parents);

		RawAnimation animation;
		animation.duration = duration;
		animation.tracks.resize(list.size());
		for (size_t i = 0; i < list.size(); ++i)
		{
			animation.tracks[i].translations.push_back({0.0f, list[i]->transform.translation});
			animation.tracks[i].rotations.push_back({0.0f, list[i]->transform.rotation});
			animation.tracks[i].scales.push_back({0.0f, list[i]->transform.scale});
		}
		return animation;
	}
}

TEST_CASE("Retargeting onto an identical rig is a passthrough", "[anim][retarget]")
{
	// hips -> arm -> hand, all identity bind rotations, bones along +x from z=1.
	RawSkeleton rig;
	rig.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	rig.roots[0].children.push_back(MakeJoint("arm", Float3(0.3f, 0.0f, 0.0f)));
	rig.roots[0].children[0].children.push_back(MakeJoint("hand", Float3(0.3f, 0.0f, 0.0f)));
	REQUIRE(rig.Validate());

	RawAnimation animation = BindPoseAnimation(rig, 1.0f);
	// Bend the arm 90 degrees about z over one second.
	animation.tracks[1].rotations.clear();
	animation.tracks[1].rotations.push_back({0.0f, Quaternion::identity()});
	animation.tracks[1].rotations.push_back({1.0f, AxisAngle(Float3(0.0f, 0.0f, 1.0f), kPi * 0.5f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, rig, rig);
	REQUIRE(retargeted.Validate());
	REQUIRE(retargeted.tracks.size() == 3);

	REQUIRE(retargeted.tracks[1].rotations.size() == 2);
	for (size_t k = 0; k < 2; ++k)
	{
		RequireSameRotation(retargeted.tracks[1].rotations[k].value, animation.tracks[1].rotations[k].value);
	}
	for (float time : {0.0f, 0.5f, 1.0f})
	{
		const auto sourcePose = ModelPose(rig, animation, time);
		const auto targetPose = ModelPose(rig, retargeted, time);
		for (size_t i = 0; i < sourcePose.size(); ++i)
		{
			RequireNear(targetPose[i].translation, sourcePose[i].translation);
			RequireSameRotation(targetPose[i].rotation, sourcePose[i].rotation);
		}
	}
}

TEST_CASE("Rigs with differing bind local rotations produce the same world-space motion", "[anim][retarget]")
{
	// This is the reported defect: a normalised source rig (identity bind rotations)
	// retargeted onto a rig with the same world-space bind pose expressed through
	// non-identity local rotations, as VRM 1.0 avatars are free to do.
	RawSkeleton source;
	source.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	source.roots[0].children.push_back(MakeJoint("arm", Float3(0.3f, 0.0f, 0.0f)));
	source.roots[0].children[0].children.push_back(MakeJoint("hand", Float3(0.3f, 0.0f, 0.0f)));
	REQUIRE(source.Validate());

	// Same world positions: arm's frame is yawed 90 degrees, so hand's local offset
	// is re-expressed in that rotated frame.
	const Quaternion armBind = AxisAngle(Float3(0.0f, 0.0f, 1.0f), kPi * 0.5f);
	RawSkeleton		 target;
	target.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	target.roots[0].children.push_back(MakeJoint("arm", Float3(0.3f, 0.0f, 0.0f), armBind));
	target.roots[0].children[0].children.push_back(MakeJoint("hand", Float3(0.0f, -0.3f, 0.0f), ozz::math::Conjugate(armBind)));
	REQUIRE(target.Validate());

	RawAnimation animation = BindPoseAnimation(source, 1.0f);
	// Swing the arm about y (out of the bone axis' plane) and twist the hand about z.
	animation.tracks[1].rotations.clear();
	animation.tracks[1].rotations.push_back({0.0f, Quaternion::identity()});
	animation.tracks[1].rotations.push_back({1.0f, AxisAngle(Float3(0.0f, 1.0f, 0.0f), -kPi * 0.5f)});
	animation.tracks[2].rotations.clear();
	animation.tracks[2].rotations.push_back({0.0f, Quaternion::identity()});
	animation.tracks[2].rotations.push_back({1.0f, AxisAngle(Float3(0.0f, 0.0f, 1.0f), kPi * 0.25f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, source, target);
	REQUIRE(retargeted.Validate());

	for (float time : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
	{
		const auto sourcePose = ModelPose(source, animation, time);
		const auto targetPose = ModelPose(target, retargeted, time);
		for (size_t i = 0; i < sourcePose.size(); ++i)
		{
			RequireNear(targetPose[i].translation, sourcePose[i].translation);
		}
	}
}

TEST_CASE("Animated source joints missing from the target are folded into descendants", "[anim][retarget]")
{
	// Source: hips -> chest -> upperChest -> neck. Target lacks upperChest but the
	// neck sits at the same world position; upperChest's motion must fold into neck.
	RawSkeleton source;
	source.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	source.roots[0].children.push_back(MakeJoint("chest", Float3(0.0f, 0.0f, 0.3f)));
	source.roots[0].children[0].children.push_back(MakeJoint("upperChest", Float3(0.0f, 0.0f, 0.2f)));
	source.roots[0].children[0].children[0].children.push_back(MakeJoint("neck", Float3(0.0f, 0.0f, 0.2f)));
	REQUIRE(source.Validate());

	RawSkeleton target;
	target.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	target.roots[0].children.push_back(MakeJoint("chest", Float3(0.0f, 0.0f, 0.3f)));
	target.roots[0].children[0].children.push_back(MakeJoint("neck", Float3(0.0f, 0.0f, 0.4f)));
	REQUIRE(target.Validate());

	RawAnimation animation = BindPoseAnimation(source, 1.0f);
	// Animate the joint the target does not have.
	animation.tracks[2].rotations.clear();
	animation.tracks[2].rotations.push_back({0.0f, Quaternion::identity()});
	animation.tracks[2].rotations.push_back({1.0f, AxisAngle(Float3(1.0f, 0.0f, 0.0f), kPi * 0.25f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, source, target);
	REQUIRE(retargeted.Validate());
	REQUIRE(retargeted.tracks.size() == 3);

	for (float time : {0.0f, 0.5f, 1.0f})
	{
		const auto sourcePose = ModelPose(source, animation, time);
		const auto targetPose = ModelPose(target, retargeted, time);
		// Source index 3 is the neck; target index 2 is the neck.
		RequireSameRotation(targetPose[2].rotation, sourcePose[3].rotation);
	}
}

TEST_CASE("Topology mismatches fall back to the bind pose without throwing", "[anim][retarget]")
{
	// Source: hand is a root of its own, not a descendant of hips. The target's hand
	// expects its mapped ancestor (hips) to be an ancestor in the source too; when it
	// is not, the track must fall back to the target's bind pose.
	RawSkeleton source;
	source.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	source.roots.push_back(MakeJoint("hand", Float3(0.6f, 0.0f, 1.0f)));
	REQUIRE(source.Validate());

	RawSkeleton target;
	target.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	target.roots[0].children.push_back(MakeJoint("hand", Float3(0.6f, 0.0f, 0.0f)));
	// A joint with no source counterpart at all must also fall back to bind pose.
	target.roots[0].children.push_back(MakeJoint("tail", Float3(-0.2f, 0.0f, 0.0f)));
	REQUIRE(target.Validate());

	RawAnimation animation = BindPoseAnimation(source, 1.0f);
	animation.tracks[1].rotations.clear();
	animation.tracks[1].rotations.push_back({0.0f, AxisAngle(Float3(0.0f, 0.0f, 1.0f), kPi * 0.5f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, source, target);
	REQUIRE(retargeted.Validate());
	REQUIRE(retargeted.tracks.size() == 3);

	// Target track 1 is the hand: bind pose, not the source root's animated rotation.
	REQUIRE(retargeted.tracks[1].rotations.size() == 1);
	RequireSameRotation(retargeted.tracks[1].rotations[0].value, Quaternion::identity());
	RequireNear(retargeted.tracks[1].translations[0].value, Float3(0.6f, 0.0f, 0.0f));
	// Target track 2 is the tail: bind pose.
	REQUIRE(retargeted.tracks[2].rotations.size() == 1);
	RequireNear(retargeted.tracks[2].translations[0].value, Float3(-0.2f, 0.0f, 0.0f));
}

TEST_CASE("Hips translation is retargeted with height scaling; other joints keep bind translations", "[anim][retarget]")
{
	RawSkeleton source;
	source.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	source.roots[0].children.push_back(MakeJoint("arm", Float3(0.3f, 0.0f, 0.0f)));
	REQUIRE(source.Validate());

	// Same rig, twice the hips height.
	RawSkeleton target;
	target.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 2.0f)));
	target.roots[0].children.push_back(MakeJoint("arm", Float3(0.3f, 0.0f, 0.0f)));
	REQUIRE(target.Validate());

	RawAnimation animation = BindPoseAnimation(source, 1.0f);
	animation.tracks[0].translations.clear();
	animation.tracks[0].translations.push_back({0.0f, Float3(0.0f, 0.0f, 1.0f)});
	animation.tracks[0].translations.push_back({1.0f, Float3(0.1f, 0.0f, 1.2f)});
	// The arm's source translation track tries to move it; the retargeted arm must ignore this.
	animation.tracks[1].translations.clear();
	animation.tracks[1].translations.push_back({0.0f, Float3(0.5f, 0.5f, 0.5f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, source, target);
	REQUIRE(retargeted.Validate());

	// Hips: bind plus the source delta scaled by the height ratio (2/1).
	REQUIRE(retargeted.tracks[0].translations.size() == 2);
	RequireNear(retargeted.tracks[0].translations[0].value, Float3(0.0f, 0.0f, 2.0f));
	RequireNear(retargeted.tracks[0].translations[1].value, Float3(0.2f, 0.0f, 2.4f));
	// Arm: single key holding the target's bind translation.
	REQUIRE(retargeted.tracks[1].translations.size() == 1);
	RequireNear(retargeted.tracks[1].translations[0].value, Float3(0.3f, 0.0f, 0.0f));
}

TEST_CASE("Rigs facing opposite directions are yaw-aligned", "[anim][retarget]")
{
	// The VRMA convention is +z-facing in glTF while VRM 0.x avatars face -z: the rigs'
	// bind poses differ by a 180-degree yaw, detectable only from bind translations (both
	// rigs may be fully normalised). World-space deltas must be re-expressed in the
	// target's facing, else pitch/roll-axis rotations reverse: limbs bend the wrong way.
	// Engineering space here: z up, source has its left side at +x.
	RawSkeleton source;
	source.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	source.roots[0].children.push_back(MakeJoint("leftUpperLeg", Float3(0.09f, 0.0f, -0.05f)));
	source.roots[0].children.push_back(MakeJoint("rightUpperLeg", Float3(-0.09f, 0.0f, -0.05f)));
	source.roots[0].children.push_back(MakeJoint("leftUpperArm", Float3(0.13f, 0.0f, 0.48f)));
	source.roots[0].children[2].children.push_back(MakeJoint("leftHand", Float3(0.3f, 0.0f, 0.0f)));
	REQUIRE(source.Validate());

	// The same rig yawed 180 degrees about z: x and y negated.
	RawSkeleton target;
	target.roots.push_back(MakeJoint("hips", Float3(0.0f, 0.0f, 1.0f)));
	target.roots[0].children.push_back(MakeJoint("leftUpperLeg", Float3(-0.09f, 0.0f, -0.05f)));
	target.roots[0].children.push_back(MakeJoint("rightUpperLeg", Float3(0.09f, 0.0f, -0.05f)));
	target.roots[0].children.push_back(MakeJoint("leftUpperArm", Float3(-0.13f, 0.0f, 0.48f)));
	target.roots[0].children[2].children.push_back(MakeJoint("leftHand", Float3(-0.3f, 0.0f, 0.0f)));
	REQUIRE(target.Validate());

	RawAnimation animation = BindPoseAnimation(source, 1.0f);
	// Raise the left arm forward (a pitch about the source's lateral x axis) and walk the
	// hips forward along the source's facing.
	animation.tracks[3].rotations.clear();
	animation.tracks[3].rotations.push_back({0.0f, Quaternion::identity()});
	animation.tracks[3].rotations.push_back({1.0f, AxisAngle(Float3(1.0f, 0.0f, 0.0f), kPi * 0.5f)});
	animation.tracks[0].translations.clear();
	animation.tracks[0].translations.push_back({0.0f, Float3(0.0f, 0.0f, 1.0f)});
	animation.tracks[0].translations.push_back({1.0f, Float3(0.0f, 0.5f, 1.0f)});
	REQUIRE(animation.Validate());

	RawAnimation retargeted = RetargetAnimation(animation, source, target);
	REQUIRE(retargeted.Validate());

	// The whole retargeted motion must be the source motion yawed 180 degrees about z.
	const Quaternion yaw180 = AxisAngle(Float3(0.0f, 0.0f, 1.0f), kPi);
	for (float time : {0.0f, 0.5f, 1.0f})
	{
		const auto sourcePose = ModelPose(source, animation, time);
		const auto targetPose = ModelPose(target, retargeted, time);
		for (size_t i = 0; i < sourcePose.size(); ++i)
		{
			RequireNear(targetPose[i].translation, ozz::math::TransformVector(yaw180, sourcePose[i].translation));
			// Model rotations transform by conjugation: the same world motion seen in a yawed frame.
			RequireSameRotation(targetPose[i].rotation, yaw180 * sourcePose[i].rotation * ozz::math::Conjugate(yaw180));
		}
	}
}

TEST_CASE("Bone name normalisation maps common rig conventions onto VRM names", "[anim][names]")
{
	REQUIRE(getMappedBoneName(std::string("LeftForeArm")) == "leftlowerarm");
	REQUIRE(getMappedBoneName(std::string("J_Bip_C_Hips")) == "hips");
	REQUIRE(getMappedBoneName(std::string("mixamorig_LeftArm")) == "leftupperarm");
	REQUIRE(getMappedBoneName(std::string("hips")) == "hips");
	REQUIRE(getMappedBoneName(std::string("RightUpLeg")) == "rightupperleg");
}

TEST_CASE("Bone names with a trailing index resolve to their role", "[anim][names]")
{
	// Ready Player Me and similar exporters number the joints: "Hips_01", "LeftArm_011".
	// Stripping the prefix up to the first underscore - which is right for "Avatar_Hips" -
	// turned these into "01" and "011", so nothing matched and the avatar held its bind pose.
	REQUIRE(getMappedBoneName(std::string("Hips_01")) == "hips");
	REQUIRE(getMappedBoneName(std::string("Hips_66")) == "hips");
	REQUIRE(getMappedBoneName(std::string("Spine1_03")) == "chest");
	REQUIRE(getMappedBoneName(std::string("Spine2_04")) == "upperchest");
	REQUIRE(getMappedBoneName(std::string("LeftArm_011")) == "leftupperarm");
	REQUIRE(getMappedBoneName(std::string("LeftForeArm_012")) == "leftlowerarm");
	REQUIRE(getMappedBoneName(std::string("LeftUpLeg_050")) == "leftupperleg");
	REQUIRE(getMappedBoneName(std::string("RightToeBase_064")) == "righttoes");
}

TEST_CASE("A meaningful trailing digit is not mistaken for an index", "[anim][names]")
{
	// "Spine1" is the chest; only a digit behind a separator is an index.
	REQUIRE(getMappedBoneName(std::string("Spine1")) == "chest");
	REQUIRE(getMappedBoneName(std::string("Spine_1")) == "spine");
	REQUIRE(getMappedBoneName(std::string("Avatar_Spine1")) == "chest");
}

TEST_CASE("The prefixed convention still resolves", "[anim][names]")
{
	// The VRM the example server streams, which must keep working.
	REQUIRE(getMappedBoneName(std::string("Avatar_Hips")) == "hips");
	REQUIRE(getMappedBoneName(std::string("Avatar_LeftArm")) == "leftupperarm");
	REQUIRE(getMappedBoneName(std::string("Avatar_LeftForeArm")) == "leftlowerarm");
	REQUIRE(getMappedBoneName(std::string("Avatar_LeftToeBase")) == "lefttoes");
	REQUIRE(getMappedBoneName(std::string("mixamorig:LeftArm")) == "leftupperarm");
}
