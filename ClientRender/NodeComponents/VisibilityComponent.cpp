#include "VisibilityComponent.h"

namespace teleport
{
	namespace clientrender
	{
		void VisibilityComponent::update(int64_t timestamp_us)
		{
			if (isVisible )
			{
				timestamp_last_visible_us = timestamp_us;
			}
		}

		void VisibilityComponent::setVisibility(bool visible, InvisibilityReason reason)
		{
			isVisible = visible;
			invisibilityReason = !visible ? reason : VisibilityComponent::InvisibilityReason::VISIBLE;

		}

		bool VisibilityComponent::getVisibility() const
		{
			return isVisible;
		}

		VisibilityComponent::InvisibilityReason VisibilityComponent::getInvisibilityReason()
		{
			return invisibilityReason;
		}

		float VisibilityComponent::getTimeSinceLastVisibleS(int64_t timestamp_us) const
		{
			return float(double(timestamp_us - timestamp_last_visible_us) / 1000000.0);
		}
	}
}