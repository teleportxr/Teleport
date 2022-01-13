#pragma once

#include <memory>

#include "ClientRender/Animation.h"

namespace scr
{
	class Animation;

	class AnimationState
	{
	public:
		float speed = 1.0f; //Speed the animation plays at.

		AnimationState();
		AnimationState(const std::shared_ptr<Animation>& animation);

		//Set animation this state is wrapping.
		void setAnimation(const std::shared_ptr<Animation>& reference);
		//Returns state's animation.
		std::shared_ptr<Animation> getAnimation() const;

		bool hasTimeOverride() const;
		void clearTimeOverride();
		void setTimeOverride(const float* override, float maximum);

		//Returns the time override normalised between zero and one; returns zero when there is no time override.
		float getNormalisedTimeOverride() const;

		operator const std::shared_ptr<Animation>& ()
		{
			return animation;
		}
	private:
		std::shared_ptr<Animation> animation;

		const float* timeOverride = nullptr;
		float timeOverrideMaximum = 0.0f;
	};
}