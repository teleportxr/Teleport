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
			animationStates.emplace(animationPair.first, AnimationState(animationPair.second));
		}

		currentAnimationState = animationStates.begin();
	}

	void AnimationComponent::addAnimation(avs::uid id, std::shared_ptr<scr::Animation> animation)
	{
		animationStates[id] = animation;

		//We only need to set a starting animation if we are cycling the animations.
#if CYCLE_ANIMATIONS
		if(currentAnimationState == animationStates.end())
		{
			currentAnimationState = animationStates.begin();
		}
#else
		//Start playing the animation if we couldn't start it earlier, as we had yet to receive the animation.
		if(id == latestAnimationID)
		{
			auto animationIterator = animationStates.find(id);
			startAnimation(animationIterator, latestAnimationStartTimestamp);
		}
#endif
	}

	void AnimationComponent::setAnimation(avs::uid animationID, uint64_t startTimestamp)
	{
#if !CYCLE_ANIMATIONS
		auto animationItr = animationStates.find(animationID);
		if(animationItr != animationStates.end())
		{
			startAnimation(animationItr, startTimestamp);
		}

		latestAnimationID = animationID;
		latestAnimationStartTimestamp = startTimestamp;
#endif
	}

	void AnimationComponent::setAnimationTimeOverride(avs::uid animationID, const float* timeOverride, float overrideMaximum)
	{
		auto animationIt = animationStates.find(animationID);
		if(animationIt == animationStates.end())
		{
			return;
		}

		animationIt->second.setTimeOverride(timeOverride, overrideMaximum);
	}

	void AnimationComponent::setAnimationSpeed(avs::uid animationID, float speed)
	{
		auto animationIt = animationStates.find(animationID);
		if(animationIt == animationStates.end())
		{
			return;
		}
		
		animationIt->second.speed = speed;
	}

	void AnimationComponent::update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime)
	{
		//Early-out if we're not playing an animation; either from having no animations or the node isn't currently playing an animation.
		if(currentAnimationState == animationStates.end())
		{
			return;
		}

		std::shared_ptr<Animation> animation = currentAnimationState->second.getAnimation();
		currentAnimationTime = std::min(currentAnimationTime + deltaTime * currentAnimationState->second.speed, animation->getAnimationLength());;
		animation->seekTime(boneList, getAnimationTimeValue());

#if CYCLE_ANIMATIONS
		if(currentAnimationTime >= animation->getAnimationLength())
		{
			//Increment animations, and loop back to the start of the list if we reach the end.
			++currentAnimationState;
			if(currentAnimationState == animationStates.end())
			{
				currentAnimationState = animationStates.begin();
			}

			currentAnimationTime = 0.0f;
			animation->seekTime(boneList, currentAnimationTime);
		}
#endif
	}

	const AnimationStateMap& AnimationComponent::GetAnimationStates() const
	{
		return animationStates;
	}

	const AnimationState& AnimationComponent::GetCurrentAnimationState() const
	{
		return currentAnimationState->second;
	}

	float AnimationComponent::GetCurrentAnimationTime() const
	{
		return currentAnimationTime;
	}

	void AnimationComponent::startAnimation(AnimationStateMap::iterator animationIterator, uint64_t startTimestamp)
	{
		currentAnimationState = animationIterator;
		currentAnimationTime = static_cast<float>(teleport::client::ServerTimestamp::getCurrentTimestamp() - startTimestamp);
	}

	float AnimationComponent::getAnimationTimeValue()
	{
		AnimationState animationState = currentAnimationState->second;
		if(!animationState.hasTimeOverride())
		{
			return currentAnimationTime;
		}

		float normalisedTime = animationState.getNormalisedTimeOverride();
		float animationLength = animationState.getAnimation()->getAnimationLength();
		return normalisedTime * animationLength;
	}
}
