#pragma once

namespace clientrender
{
	class VisibilityComponent
	{
	public:
		enum class InvisibilityReason
		{
			VISIBLE, //Node is not invisible.

			OUT_OF_BOUNDS, //Node should not be renderered because it is too far from the client.
			DISABLED //Node should not be renderered because its renderable components are disabled.
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
