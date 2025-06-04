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

			void InitBindMatrices(const Animation &anim);
			
			//! Shortcut, play this animation on the given layer.
			void PlayAnimation(avs::uid cache_id, avs::uid anim_uid, uint32_t layer = 0);
	
			void GetJointMatrices(std::vector<mat4> &m) const;
			//! Fill in the vector of matrices with the current animation state.
			void GetBoneMatrices(std::vector<mat4> &m) const;

			// Update the animation state.
			void setAnimationState(std::chrono::microseconds timestampUs,const teleport::core::ApplyAnimation &animationUpdate);

			//! @brief Update all animations, given the current timestamp, which is the time in microseconds since the server's datum timestamp.
			//! @param boneList 
			//! @param timestampUs 
			void update( int64_t timestampUs);
			void update(const std::vector<std::shared_ptr<clientrender::Node>> &boneList, int64_t timestampUs);

		private:
			// TODO: The following may need to be extracted into a per-instance structure, as the same
			// component could be used in multiple instances of the same SubScene.
			AnimationInstance *instance=nullptr;
			std::vector<mat4> inverseBindMatrices;
		};
	}

}