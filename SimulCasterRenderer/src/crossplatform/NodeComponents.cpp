#include "NodeComponents.h"

#define CYCLE_ANIMATIONS 1

namespace scr
{
	AnimationComponent::AnimationComponent()
	{}

	AnimationComponent::AnimationComponent(const std::map<avs::uid, std::shared_ptr<Animation>>& animations)
		:animations(animations)
	{
		currentAnimation = this->animations.begin();
	}

	void AnimationComponent::AddAnimation(avs::uid id, std::shared_ptr<Animation> animation)
	{
		animations[id] = animation;
		if (currentAnimation == animations.end())
		{
			currentAnimation = animations.begin();
		}
	}

	void AnimationComponent::update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime)
	{
		//Early-out if there are no animations.
		if(animations.empty())
		{
			return;
		}

		currentAnimationTime = std::min(currentAnimationTime + deltaTime, currentAnimation->second->getAnimationLength());
		currentAnimation->second->seekTime(boneList, currentAnimationTime);

#if CYCLE_ANIMATIONS
		if(currentAnimationTime >= currentAnimation->second->getAnimationLength())
		{
			//Increment animations, and loop back to the start of the list if we reach the end.
			++currentAnimation;
			if(currentAnimation == animations.end())
			{
				currentAnimation = animations.begin();
			}

			currentAnimationTime = 0.0f;
			currentAnimation->second->seekTime(boneList, currentAnimationTime);
		}
#endif
	}

	void VisibilityComponent::update(float deltaTime)
	{
		if (isVisible == false)
		{
			timeSinceLastVisible += deltaTime;
		}
	}

	void VisibilityComponent::setVisibility(bool visible)
	{
		isVisible = visible;

		if (isVisible)
		{
			timeSinceLastVisible = 0;
		}
	}

	bool VisibilityComponent::getVisibility() const
	{
		return isVisible;
	}

	float VisibilityComponent::getTimeSinceLastVisible() const
	{
		return timeSinceLastVisible;
	}
}