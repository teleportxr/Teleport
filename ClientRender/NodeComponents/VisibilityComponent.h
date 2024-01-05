#pragma once

namespace teleport
{
	namespace clientrender
	{
		class VisibilityComponent
		{
		public:
			enum class InvisibilityReason
			{
				VISIBLE = 0, // Node is not invisible.

				OUT_OF_BOUNDS = 1, // Node should not be renderered because it is too far from the client.
				DISABLED = 2	   // Node should not be renderered because its renderable components are disabled.
			};

			void update(float deltaTime);

			void setVisibility(bool visible, InvisibilityReason reason);
			bool getVisibility() const;

			InvisibilityReason getInvisibilityReason();

			float getTimeSinceLastVisible() const;

		private:
			bool isVisible = true;

			InvisibilityReason invisibilityReason;

			float timeSinceLastVisible = 0;
		};
	}
}