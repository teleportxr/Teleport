#include "ActorComponents.h"

namespace scr
{

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