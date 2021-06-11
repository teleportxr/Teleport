#pragma once

#include <map>
#include <memory>
#include <vector>

#include "libavstream/common.hpp"

#include "AnimationState.h"

namespace scr
{
	class Animation;
	class Bone;

	class AnimationComponent
	{
	public:
		AnimationComponent();
		AnimationComponent(const std::map<avs::uid, std::shared_ptr<scr::Animation>>& animations);

		void addAnimation(avs::uid id, std::shared_ptr<scr::Animation> animation);
		//Set animation the component is playing.
		//	animationID : ID of the animation to start playing.
		//	startTimestamp : Timestamp of when the animation started playing on the server.
		void setAnimation(avs::uid animationID, uint64_t startTimestamp);

		//Causes the time used to seek the animation position to be overriden by the passed value.
		//Passing in a nullptr will cause the animation to use the default; i.e. the current time in the animation controller.
		//	animationID : ID of the animation we are changing the target for.
		//	timeOverride : Pointer to the float that will be used as the current animation time.
		//	valueMaximum : Maximum value the override can be; i.e. number is of the range [0.0, valueMaximum].
		void setAnimationTimeOverride(avs::uid animationID, const float* timeOverride = nullptr, float overrideMaximum = 0.0f);

		void update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime);
	private:
		typedef std::map<avs::uid, AnimationState> AnimationLookup_t;

		AnimationLookup_t animations;
		AnimationLookup_t::iterator currentAnimation = animations.end();
		float currentAnimationTime = 0.0f; //How many milliseconds along the current animation is.

		//Variables for if we can't start the animation because it has yet to be received, but we want to start when the animation is received.
		avs::uid latestAnimationID = 0;
		uint64_t latestAnimationStartTimestamp = 0;

		//Starts playing animation as if it had started at the passed time.
		//	animationIterator : Iterator for animation to play from animations lookup.
		//	startTimestamp : The timestamp of when the animation started on the server.
		void startAnimation(AnimationLookup_t::iterator animationIterator, uint64_t startTimestamp);

		//Animation may have an animation override, so we need to use the current time in the animation component only when it is set.
		float getAnimationTimeValue();
	};
}
