#include "AnimationComponent.h"

#include <algorithm>

#include "crossplatform/Animation.h"

#include "TeleportClient/ServerTimestamp.h"

#define CYCLE_ANIMATIONS 0

namespace scr
{
	AnimationComponent::AnimationComponent()
	{}

	AnimationComponent::AnimationComponent(const std::map<avs::uid, std::shared_ptr<scr::Animation>>& animations)
	{
		for(const auto& animationPair : animations)
		{
			this->animations.emplace(animationPair.first, AnimationState(animationPair.second));
		}

		currentAnimation = this->animations.begin();
	}

	void AnimationComponent::addAnimation(avs::uid id, std::shared_ptr<scr::Animation> animation)
	{
		animations.emplace(id, animation);

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

	void AnimationComponent::setAnimationTimeOverride(avs::uid animationID, const float* timeOverride, float overrideMaximum)
	{
		auto animationIt = animations.find(animationID);
		if(animationIt == animations.end())
		{
			return;
		}

		animationIt->second.setTimeOverride(timeOverride, overrideMaximum);
	}

	void AnimationComponent::update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime)
	{
		//Early-out if we're not playing an animation; either from having no animations or the node isn't currently playing an animation.
		if(currentAnimation == animations.end())
		{
			return;
		}

		std::shared_ptr<Animation> animation = currentAnimation->second.getAnimation();
		currentAnimationTime = std::min(currentAnimationTime + deltaTime, animation->getAnimationLength());;
		animation->seekTime(boneList, getAnimationTimeValue());

#if CYCLE_ANIMATIONS
		if(currentAnimationTime >= animation->getAnimationLength())
		{
			//Increment animations, and loop back to the start of the list if we reach the end.
			++currentAnimation;
			if(currentAnimation == animations.end())
			{
				currentAnimation = animations.begin();
			}

			currentAnimationTime = 0.0f;
			animation->seekTime(boneList, currentAnimationTime);
		}
#endif
	}

	void AnimationComponent::startAnimation(AnimationLookup_t::iterator animationIterator, uint64_t startTimestamp)
	{
		currentAnimation = animationIterator;
		currentAnimationTime = 0.0f;
	}

	float AnimationComponent::getAnimationTimeValue()
	{
		AnimationState currentAnimationState = currentAnimation->second;
		if(!currentAnimationState.hasTimeOverride())
		{
			return currentAnimationTime;
		}

		float normalisedTime = currentAnimationState.getNormalisedTimeOverride();
		float animationLength = currentAnimationState.getAnimation()->getAnimationLength();
		return normalisedTime * animationLength;
	}
}
