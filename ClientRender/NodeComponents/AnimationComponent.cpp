#include "AnimationComponent.h"

#include <algorithm>
#include <libavstream/src/platform.hpp>

#include "ClientRender/Animation.h"

#include "TeleportClient/ServerTimestamp.h"

#define CYCLE_ANIMATIONS 0

namespace clientrender
{
	AnimationComponent::AnimationComponent()
	{}

	AnimationComponent::AnimationComponent(const std::map<avs::uid, std::shared_ptr<clientrender::Animation>>& animations)
	{
		for(const auto& animationPair : animations)
		{
			animationStates.emplace(animationPair.first, AnimationState(animationPair.second));
		}

		currentAnimationState = animationStates.begin();
	}

	void AnimationComponent::addAnimation(avs::uid id, std::shared_ptr<clientrender::Animation> animation)
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
			startAnimation(animationIterator, latestAnimationStartTimestampUnixUTCMs);
		}
#endif
	}

	void AnimationComponent::setAnimation(avs::uid animationID, uint64_t startTimestampUtcMs)
	{
#if !CYCLE_ANIMATIONS
		auto animationItr = animationStates.find(animationID);
		if(animationItr != animationStates.end())
		{
			startAnimation(animationItr, startTimestampUtcMs);
		}

		latestAnimationID = animationID;
		latestAnimationStartTimestampUnixUTCMs = startTimestampUtcMs;
#endif
	}

	void AnimationComponent::setAnimation(avs::uid animationID)
	{
		static auto tBegin = avs::Platform::getTimestamp();
		auto ts = avs::Platform::getTimestamp();
		double ms=avs::Platform::getTimeElapsedInMilliseconds(tBegin, ts);
 		setAnimation(animationID,(uint64_t)ms);
	}
	
		
	void AnimationComponent::setAnimationTimeOverride(avs::uid animationID, float timeOverride, float overrideMaximum)
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

	void AnimationComponent::update(const std::vector<std::shared_ptr<clientrender::Bone>>& boneList, float deltaTimeS)
	{
		//Early-out if we're not playing an animation; either from having no animations or the node isn't currently playing an animation.
		if(currentAnimationState == animationStates.end())
		{
			return;
		}

		std::shared_ptr<Animation> animation = currentAnimationState->second.getAnimation();
		currentAnimationState->second.currentAnimationTimeS+=deltaTimeS * currentAnimationState->second.speed;
		currentAnimationState->second.currentAnimationTimeS = std::max(0.0f,std::min(currentAnimationState->second.currentAnimationTimeS, animation->getAnimationLengthSeconds()));
		animation->seekTime(boneList, currentAnimationState->second.currentAnimationTimeS);

#if CYCLE_ANIMATIONS
		if(currentAnimationState->second.currentAnimationTimeS >= animation->getAnimationLengthSeconds())
		{
			//Increment animations, and loop back to the start of the list if we reach the end.
			++currentAnimationState;
			if(currentAnimationState == animationStates.end())
			{
				currentAnimationState = animationStates.begin();
			}

			currentAnimationState->second.currentAnimationTimeS = 0.0f;
			animation->seekTime(boneList, currentAnimationState->second.currentAnimationTimeS);
		}
#endif
	}

	const AnimationStateMap& AnimationComponent::GetAnimationStates() const
	{
		return animationStates;
	}
	
	AnimationState* AnimationComponent:: GetAnimationState(avs::uid u) 
	{
 		auto i=animationStates.find(u);
		if(i==animationStates.end())
			return nullptr;
		return &i->second;
	}

	const AnimationState *AnimationComponent::GetCurrentAnimationState() const
	{
		if(currentAnimationState==animationStates.end())
			return nullptr;
		return &currentAnimationState->second;
	}

	float AnimationComponent::GetCurrentAnimationTimeSeconds() const
	{
		return currentAnimationState->second.currentAnimationTimeS;
	}

	void AnimationComponent::startAnimation(AnimationStateMap::iterator animationIterator, uint64_t startTimestampUtcMs)
	{
 		if(currentAnimationState != animationIterator)
		{
 			currentAnimationState = animationIterator;
			currentAnimationState->second.currentAnimationTimeS = 0.f;//float(0.001 * (teleport::client::ServerTimestamp::getCurrentTimestampUTCUnixMs() - startTimestampUtcMs));
		}
	}
}
