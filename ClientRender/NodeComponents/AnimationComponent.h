#pragma once

#include <map>
#include <memory>
#include <vector>

#include "libavstream/common.hpp"

#include "AnimationState.h"
#include "Component.h"

namespace teleport
{
	namespace clientrender
	{
		class Animation;
		class Bone;
		typedef std::map<avs::uid, AnimationState> AnimationStateMap;

		class AnimationComponent : public Component
		{
		public:
			AnimationComponent();
			virtual ~AnimationComponent() {}
			AnimationComponent(const std::map<avs::uid, std::shared_ptr<clientrender::Animation>> &animations);

			void addAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation);
			// Set animation the component is playing.
			//	animationID : ID of the animation to start playing.
			//	startTimestamp : Timestamp of when the animation started playing on the server.
			void setAnimation(avs::uid animationID, uint64_t startTimestamp);
			void setAnimation(avs::uid animationID);
			// Causes the time used to seek the animation position to be overriden by the passed value.
			// Passing in a nullptr will cause the animation to use the default; i.e. the current time in the animation controller.
			//	animationID : ID of the animation we are changing the target for.
			//	timeOverride : Pointer to the float that will be used as the current animation time.
			//	valueMaximum : Maximum value the override can be; i.e. number is of the range [0.0, valueMaximum].
			void setAnimationTimeOverride(avs::uid animationID, float timeOverride, float overrideMaximum = 0.0f);

			void setAnimationSpeed(avs::uid animationID, float speed);

			void update(const std::vector<std::shared_ptr<clientrender::Bone>> &boneList, float deltaTime);

			const AnimationStateMap &GetAnimationStates() const;
			AnimationState *GetAnimationState(avs::uid);
			const AnimationState *GetCurrentAnimationState() const;
			float GetCurrentAnimationTimeSeconds() const;

		private:
			AnimationStateMap animationStates;
			AnimationStateMap::iterator currentAnimationState = animationStates.end();

			// Variables for if we can't start the animation because it has yet to be received, but we want to start when the animation is received.
			avs::uid latestAnimationID = 0;
			uint64_t latestAnimationStartTimestampUnixUTCMs = 0;

			// Starts playing animation as if it had started at the passed time.
			//	animationIterator : Iterator for animation to play from animations lookup.
			//	startTimestamp : The timestamp of when the animation started on the server.
			void startAnimation(AnimationStateMap::iterator animationIterator, uint64_t startTimestamp);
		};
	}

}