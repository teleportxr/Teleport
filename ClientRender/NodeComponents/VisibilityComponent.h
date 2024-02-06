#pragma once
#include <cstdint>
namespace teleport
{
	namespace clientrender
	{
		class VisibilityComponent
		{
		public:
			enum class InvisibilityReason
			{
				VISIBLE = 0,		// Node is visible.

				OUT_OF_BOUNDS = 1, // Node should not be renderered because it is too far from the client.
				DISABLED = 2	   // Node should not be renderered because its renderable components are disabled.
			};

			void update(int64_t timestamp_us);

			void setVisibility(bool visible, InvisibilityReason reason);
			bool getVisibility() const;

			InvisibilityReason getInvisibilityReason();

			float getTimeSinceLastVisibleS(int64_t timestamp_us) const;

		private:
			bool isVisible = true;

			InvisibilityReason invisibilityReason;

			int64_t timestamp_last_visible_us = 0;
		};
	}
}