#include "AnimationState.h"

namespace clientrender
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

	AnimationTimeMode AnimationState::GetAnimationTimeMode() const
	{
		return animationTimeMode;
	}

	void AnimationState::setAnimationTimeMode(AnimationTimeMode m)
	{
		animationTimeMode = m;
		timeOverrideMaximum = 0.0f;
	}

	void AnimationState::setTimeOverride(float override, float maximum)
	{
		if(animationTimeMode!=AnimationTimeMode::SCRUBBING)
		{
			TELEPORT_CERR<<"AnimationState::setTimeOverride called when not in scrubbing mode."<<std::endl;
		}
		timeOverride = override;
		timeOverrideMaximum = maximum;
	}

	float AnimationState::getNormalisedTimeOverride() const
	{
		return animationTimeMode==AnimationTimeMode::SCRUBBING&&timeOverrideMaximum>0.0f ? timeOverride / timeOverrideMaximum : 0.0f;
	}
}
