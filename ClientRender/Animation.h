#pragma once

#include <memory>
#include <vector>

#include "TeleportCore/InputTypes.h"
#include "TeleportCore/Animation.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/animation/runtime/animation.h"
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/base/containers/map.h>
#include "ClientRender/Skeleton.h"
#include <regex>


namespace teleport
{
	namespace clientrender
	{
		class Node;
		//! A minimal struct for storing rest poses in joints.
		//! A list of keyframes, i.e. a single track for an animation. Defines the positions and rotations for one bone in a skeleton,
		//! across the length of a single animation.
		class BoneKeyframeList
		{
		public:
			size_t boneIndex = -1; // Index of the bone used in the bones list.

			std::vector<teleport::core::Vector3Keyframe> positionKeyframes;
			std::vector<teleport::core::Vector4Keyframe> rotationKeyframes;

			float duration;
		};

		class Animation
		{
		public:
			const std::string name;
			float duration=0.f;
			std::vector<BoneKeyframeList> boneKeyframeLists;

			Animation(const std::string &name);
			Animation(const std::string &name, float duration,std::vector<BoneKeyframeList> boneKeyframes);
			virtual ~Animation();
			std::string getName() const
			{
				return name;
			}

			static const char *getTypeName()
			{
				return "Animation";
			}
			// Updates how long the animations runs for by scanning boneKeyframeLists.
			void updateAnimationLength();

			// Returns how many seconds long the animation is.
			float getAnimationLengthSeconds();

			//! Load from glb (gltf binary) format.
			bool LoadFromGlb(const uint8_t *data, size_t size);

			//! Create a version of the animation retargeted to the given skeleton.
			void Retarget(std::shared_ptr<clientrender::Skeleton> skeleton);

			ozz::animation::Animation *GetOzzAnimation(uint64_t skeleton_hash);
			const std::map<std::string, teleport::core::PoseScale> &GetRestPoses() const
			{
				return restPoses;
			}
		private:
			ozz::unique_ptr<ozz::animation::offline::RawSkeleton>	raw_skeleton;
			//ozz::unique_ptr<ozz::animation::Skeleton>				ozz_skeleton;
			ozz::unique_ptr<ozz::animation::offline::RawAnimation> raw_animation;
			ozz::map<uint64_t,ozz::unique_ptr<ozz::animation::Animation>> retargeted_animations;
			//! Mapping from node names to the initial poses.
			std::map<std::string, teleport::core::PoseScale> restPoses;
			void ToOzz();
			float endTime_s = 0.0f; // Seconds the animation lasts for.
		};
	}
}