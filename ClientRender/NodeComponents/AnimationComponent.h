#pragma once

#include <map>
#include <memory>
#include <vector>
#include <set>

#include "libavstream/common.hpp"

#include "AnimationState.h"
#include "Component.h"

namespace teleport
{
	namespace core
	{
		struct ApplyAnimation;
	}
	namespace clientrender
	{
		class Animation;
		class Node;
		class GeometryCache;

		class AnimationComponent : public Component
		{
		public:
			AnimationComponent();
			AnimationComponent(const std::map<avs::uid, std::shared_ptr<Animation>> &anims);
			virtual ~AnimationComponent() {}

			//! This informs the component that an animation is ready for use.
			void addAnimation(avs::uid id, std::shared_ptr<Animation>);
			//! This informs the component that an animation is no longer available.
			void removeAnimation(avs::uid id);
			// Update the animation state.
			void setAnimationState(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate);

			//! @brief Update all animations, given the current timestamp, which is the time in microseconds since the server's datum timestamp.
			//! @param boneList 
			//! @param timestampUs 
			void update(const std::vector<std::shared_ptr<clientrender::Node>> &boneList, int64_t timestampUs);

			const std::vector<AnimationLayerStateSequence> &GetAnimationLayerStates() const;

		private:
			std::vector<AnimationLayerStateSequence> animationLayerStates;
			std::map<avs::uid,std::shared_ptr<Animation>> animations;
		};
	}

}