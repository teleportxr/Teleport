#include "AnimationState.h"

namespace scr
{
	AnimationState::AnimationState()
		:AnimationState(nullptr)
	{}

	AnimationState::AnimationState(const std::shared_ptr<Animation>& animation)
		:animation(animation)
	{}

	void AnimationState::setAnimation(const std::shared_ptr<Animation>& reference)
	{
		animation = reference;
	}

	std::shared_ptr<Animation> AnimationState::getAnimation() const
	{
		return animation;
	}

	bool AnimationState::hasTimeOverride() const
	{
		return timeOverride != nullptr;
	}

	void AnimationState::clearTimeOverride()
	{
		timeOverride = nullptr;
		timeOverrideMaximum = 0.0f;
	}

	void AnimationState::setTimeOverride(const float* override, float maximum)
	{
		if(override == nullptr || maximum == 0.0f)
		{
			clearTimeOverride();
			return;
		}

		timeOverride = override;
		timeOverrideMaximum = maximum;
	}

	float AnimationState::getNormalisedTimeOverride() const
	{
		return timeOverride && timeOverrideMaximum != 0.0f ? *timeOverride / timeOverrideMaximum : 0.0f;
	}
}
