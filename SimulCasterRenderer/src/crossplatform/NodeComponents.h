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

		void AddAnimation(avs::uid id, std::shared_ptr<Animation> animation);

		void update(const std::vector<std::shared_ptr<scr::Bone>>& boneList, float deltaTime);
	private:
		std::map<avs::uid, std::shared_ptr<Animation>> animations;
		std::map<avs::uid, std::shared_ptr<Animation>>::iterator currentAnimation = animations.begin();
		float currentAnimationTime = 0.0f; //How many milliseconds along the current animation is.
	};

	class VisibilityComponent
	{
	public:
		void update(float deltaTime);

		void setVisibility(bool visible);
		bool getVisibility() const;

		float getTimeSinceLastVisible() const;

	private:
		bool isVisible = true;

		float timeSinceLastVisible = 0;
	};

}