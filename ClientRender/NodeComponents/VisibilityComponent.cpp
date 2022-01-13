#include "VisibilityComponent.h"

namespace clientrender
{
	void VisibilityComponent::update(float deltaTime)
	{
		if(isVisible == false)
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