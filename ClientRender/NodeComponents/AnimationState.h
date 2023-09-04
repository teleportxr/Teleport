#pragma once

#include <memory>

#include "ClientRender/Animation.h"

namespace clientrender
{
	class Animation;
	enum class AnimationTimeMode
	{
		INVALID=0,
		TIMESTAMP,	// Animation automatically based on timestamp and speed.
		SCRUBBING	// Animate based on explicit setting of time value.
	};
	//! Manages the state of a specific animation applied to a specific skeleton/skeleton.
	class AnimationState
	{
	public:
		float speed = 1.0f; //Speed the animation plays at.
		float currentAnimationTimeS = 0.0f;

		AnimationState();
		AnimationState(const std::shared_ptr<Animation>& animation);

		//Set animation this state is wrapping.
		void setAnimation(const std::shared_ptr<Animation>& reference);
		//Returns state's animation.
		std::shared_ptr<Animation> getAnimation() const;

		AnimationTimeMode GetAnimationTimeMode() const;
		void setAnimationTimeMode(AnimationTimeMode m);
		void setTimeOverride(float override, float maximum);

		//Returns the time override normalised between zero and one; returns zero when there is no time override.
		float getNormalisedTimeOverride() const;

		operator const std::shared_ptr<Animation>& ()
		{
			return animation;
		}
	private:
		std::shared_ptr<Animation> animation;
		
		float timeOverride = 0.0f;
		float timeOverrideMaximum = 0.0f;
		AnimationTimeMode animationTimeMode=AnimationTimeMode::TIMESTAMP;
	};
}