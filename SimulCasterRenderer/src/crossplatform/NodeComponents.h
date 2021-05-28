#pragma once

#include <vector>

#include "Animation.h"

namespace scr
{
	class AnimationComponent
	{
	public:
		AnimationComponent();
		AnimationComponent(const std::map<avs::uid, std::shared_ptr<Animation>>& animations);

		void addAnimation(avs::uid id, std::shared_ptr<Animation> animation);
		//Set animation the component is playing.
		//	animationID : ID of the animation to start playing.
		//	startTimestamp : Timestamp of when the animation started playing on the server.
		void setAnimation(avs::uid animationID, uint64_t startTimestamp);

		void update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime);
	private:
		std::map<avs::uid, std::shared_ptr<Animation>> animations;
		std::map<avs::uid, std::shared_ptr<Animation>>::iterator currentAnimation = animations.end();
		float currentAnimationTime = 0.0f; //How many milliseconds along the current animation is.

		//Variables for if we can't start the animation because it has yet to be received, but we want to start when the animation is received.
		avs::uid latestAnimationID = 0;
		uint64_t latestAnimationStartTimestamp = 0;

		//Starts playing animation as if it had started at the passed time.
		//	animationIterator : Iterator for animation to play from animations lookup.
		//	startTimestamp : The timestamp of when the animation started on the server.
		void startAnimation(std::map<avs::uid, std::shared_ptr<Animation>>::iterator animationIterator, uint64_t startTimestamp);
	};

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