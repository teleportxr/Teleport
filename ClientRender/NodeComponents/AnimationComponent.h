#pragma once

#include <map>
#include <memory>
#include <vector>
#include <set>

#include "libavstream/common.hpp"

#include "ClientRender/Animation.h"
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
		struct AnimationInstance;

		class AnimationComponent : public Component
		{
		public:
			AnimationComponent(Node &node);
			virtual ~AnimationComponent();

			void Retarget( Animation &anim);
			
			//! Shortcut, play this animation on the given layer.
			void PlayAnimation(avs::uid cache_id, avs::uid anim_uid, avs::uid root_uid, uint32_t layer = 0, float speed = 1.0f);
	
			// Update the animation state.
			void setAnimationState(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate, avs::uid root_uid);

			//! @brief Update all animations, given the current timestamp, which is the time in microseconds since the server's datum timestamp.
			//! @param boneList 
			//! @param timestampUs 
			void update( int64_t timestampUs, avs::uid root_uid);

			std::shared_ptr<AnimationInstance> GetOrCreateAnimationInstance(avs::uid root_uid);
		private:
			int64_t lastTimestampUs=0;
			std::map<avs::uid,std::shared_ptr<AnimationInstance>> animationInstances;
		};
	}

}