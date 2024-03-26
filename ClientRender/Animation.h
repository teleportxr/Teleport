#pragma once

#include <memory>
#include <vector>

#include "TeleportCore/Animation.h"
#include "Platform/CrossPlatform/Quaterniond.h"

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
			BoneKeyframeList(float duration);

			size_t boneIndex = -1; // Index of the bone used in the bones list.

			std::vector<teleport::core::Vector3Keyframe> positionKeyframes;
			std::vector<teleport::core::Vector4Keyframe> rotationKeyframes;

			void seekTime(std::shared_ptr<Node> bone, float time,float strength,bool loop) const;

		private:
			void blendPositionToTime(float time, vec3 &bonePosition, const std::vector<teleport::core::Vector3Keyframe> &keyframes,float blend,bool loop) const;
			void blendRotationToTime(float time, platform::crossplatform::Quaternionf &boneRotation, const std::vector<teleport::core::Vector4Keyframe> &keyframes, float blend, bool loop) const;
			struct KeyframePair
			{
				uint16_t prev;
				uint16_t next;
				float interp;
			};
			template<typename U>
			KeyframePair getNextKeyframeIndex(float time, const std::vector<U> &keyframes, bool loop) const;
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

			// Sets bone transforms to positions and rotations specified by the animation at the passed time.
			//	boneList : List of bones for the animation.
			//	time : Time the animation will use when moving the bone transforms in seconds.
			void seekTime(const std::vector<std::shared_ptr<clientrender::Node>> &boneList, float time_s, float strength,bool loop) const;

		private:
			float endTime_s = 0.0f; // Seconds the animation lasts for.
		};
	}
}