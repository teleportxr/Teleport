#include "NodeComponents.h"

#include <algorithm>

#include "crossplatform/ServerTimestamp.h"

#define CYCLE_ANIMATIONS 0

namespace scr
{
	AnimationComponent::AnimationComponent()
	{}

	AnimationComponent::AnimationComponent(const std::map<avs::uid, std::shared_ptr<Animation>>& animations)
		:animations(animations)
	{
		currentAnimation = this->animations.begin();
	}

	void AnimationComponent::addAnimation(avs::uid id, std::shared_ptr<Animation> animation)
	{
		animations[id] = animation;

		//We only need to set a starting animation if we are cycling the animations.
#if CYCLE_ANIMATIONS
		if(currentAnimation == animations.end())
		{
			currentAnimation = animations.begin();
		}
#else
		//Start playing the animation if we couldn't start it earlier, as we had yet to receive the animation.
		if(id == latestAnimationID)
		{
			auto animationIterator = animations.find(id);
			startAnimation(animationIterator, latestAnimationStartTimestamp);
		}
#endif
	}

	void AnimationComponent::setAnimation(avs::uid animationID, uint64_t startTimestamp)
	{
		auto animationItr = animations.find(animationID);
		if(animationItr != animations.end())
		{
			startAnimation(animationItr, startTimestamp);
		}

		latestAnimationID = animationID;
		latestAnimationStartTimestamp = startTimestamp;
	}

	void AnimationComponent::update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime)
	{
		//Early-out if we're not playing an animation; either from having no animations or the node isn't currently playing an animation.
		if(currentAnimation == animations.end())
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

	void AnimationComponent::startAnimation(std::map<avs::uid, std::shared_ptr<Animation>>::iterator animationIterator, uint64_t startTimestamp)
	{
		currentAnimation = animationIterator;
		currentAnimationTime = static_cast<float>(ServerTimestamp::getCurrentTimestamp() - startTimestamp);
	}

	void VisibilityComponent::update(float deltaTime)
	{
		if (isVisible == false)
		{
			timeSinceLastVisible += deltaTime;
		}
	}

	void VisibilityComponent::setVisibility(bool visible, InvisibilityReason reason)
	{
		isVisible = visible;
		invisibilityReason = !visible ? reason : VisibilityComponent::InvisibilityReason::VISIBLE;

		if(isVisible)
		{
			timeSinceLastVisible = 0;
		}
	}

	bool VisibilityComponent::getVisibility() const
	{
		return isVisible;
	}

	VisibilityComponent::InvisibilityReason VisibilityComponent::getInvisibilityReason()
	{
		return invisibilityReason;
	}

	float VisibilityComponent::getTimeSinceLastVisible() const
	{
		return timeSinceLastVisible;
	}
}