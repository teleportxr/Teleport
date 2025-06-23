#pragma once

#include <map>
#include <memory>
#include <vector>
#include <chrono>

#include "libavstream/common.hpp"
#include "ClientRender/Animation.h"

namespace teleport
{
    namespace clientrender
    {
        class Animation;
        class Node;

        struct AnimationInstance
        {
            struct LayerState
            {
                Animation *animation = nullptr;
                avs::uid animation_uid = 0;
                int64_t startTimestampUs = 0;
                float animTimeAtTimestamp = 0.0f;
                float speed = 1.0f;
            };

            std::map<uint32_t, LayerState> layers;
            Node &node;

            AnimationInstance(Node &n);
            ~AnimationInstance();

            void update(int64_t timestampUs);
            void getJointMatrices(std::vector<mat4> &matrices) const;
            void getBoneMatrices(std::vector<mat4> &matrices, const std::vector<int16_t> &joints, 
                                const std::vector<mat4> &meshInverseBindMatrices) const;
            void playAnimation(avs::uid cache_id, avs::uid anim_uid, uint32_t layer = 0);
            void setAnimationState(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &animationUpdate);
        };
    }
}