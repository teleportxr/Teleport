#pragma once

#include <memory>
#include <vector>

#include "TeleportCore/Animation.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/animation/runtime/animation.h"


namespace teleport
{
	namespace clientrender
	{
		class Node;

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
			ozz::animation::Animation &GetOzzAnimation()
			{
				return *animation;
			}
		private:
			ozz::unique_ptr<ozz::animation::Animation> animation;
			void ToOzz();
			float endTime_s = 0.0f; // Seconds the animation lasts for.
		};
	}
}