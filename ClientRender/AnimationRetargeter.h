#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace teleport
{
	namespace clientrender
	{
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
			stringType n = bName;
			std::transform(n.begin(), n.end(), n.begin(), ::tolower);
			n	   = std::regex_replace(n, std::regex::basic_regex("_l_"), "_left", std::regex_constants::match_any);
			n	   = std::regex_replace(n, std::regex::basic_regex("_r_"), "_right", std::regex_constants::match_any);
			n	   = std::regex_replace(n, std::regex::basic_regex(".*_"), "", std::regex_constants::match_any);
			auto m = mapping.find(n);
			if (m == mapping.end())
			{
				return n;
			}
			return m->second;
		};
		// Main retargeting function
		ozz::animation::offline::RawAnimation RetargetAnimation(const ozz::animation::offline::RawAnimation &source_animation,
																const ozz::animation::offline::RawSkeleton	&source_skeleton,
																const ozz::animation::offline::RawSkeleton	&target_skeleton);

	}
}