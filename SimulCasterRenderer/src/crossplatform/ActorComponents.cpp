#include "ActorComponents.h"

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

	if(currentAnimation == animations.end()) currentAnimation = animations.begin();
}

void AnimationComponent::update(float deltaTime)
{
	//Early-out if there are no animations.
	if(animations.empty()) return;

	currentAnimation->second->update(deltaTime);

	if(currentAnimation->second->finished())
	{
		++currentAnimation;
		if(currentAnimation == animations.end()) currentAnimation = animations.begin();

		currentAnimation->second->restart();
	}
}

void VisibilityComponent::update(float deltaTime)
{
	if(isVisible == false)
	{
		timeSinceLastVisible += deltaTime;
	}
}

void VisibilityComponent::setVisibility(bool visible)
{
	isVisible = visible;

	if(isVisible)
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