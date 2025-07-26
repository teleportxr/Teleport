#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <vector>

#include "ClientRender/Animation.h"
#include "ClientRender/NodeComponents/AnimationState.h"
#include "libavstream/common.hpp"

namespace teleport
{
	namespace clientrender
	{
		class Animation;
		class Node;

		struct AnimationInstance
		{
			bool										   animating			   = false;
			uint8_t										   numAnimationLayerStates = 0;
			AnimationLayerStateSequence					   animationLayerStates[4];
			std::map<avs::uid, std::shared_ptr<Animation>> animations;
			ozz::animation::Skeleton					  *skeleton = nullptr;
			// Buffer of local transforms as sampled from main animation_.
			ozz::vector<ozz::math::SoaTransform> main_locals;
			// Buffer of local transforms which stores the blending result.
			ozz::vector<ozz::math::SoaTransform> locals;
			// Buffer of model space matrices. These are computed by the local-to-model
			// job after the blending stage.
			ozz::vector<ozz::math::Float4x4> models;
			avs::uid						 id = 0;
			AnimationInstance(std::shared_ptr<Skeleton> sk);
			void Init();
			void SetAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &applyAnimation);
			bool Update(float dt_s, int64_t time_us);
			void GetBoneMatrices(std::vector<mat4>			&m,
								 const std::vector<int16_t> &mapMeshToSkeleton,
								 const std::vector<mat4>	&inverseBindMatrices,
								 const std::vector<int>		&mapSkeletonToAnim) const;
		};
	}
}