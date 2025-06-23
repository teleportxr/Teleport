#include "ClientRender/AnimationInstance.h"
#include "ClientRender/Node.h"
#include "ClientRender/GeometryCache.h"
#include "Core/Animation.h"

namespace teleport
{
    namespace clientrender
    {
        AnimationInstance::AnimationInstance(Node &n)
            : node(n)
        {
        }

        AnimationInstance::~AnimationInstance()
        {
        }

        void AnimationInstance::update(int64_t timestampUs)
        {
            for (auto &layer : layers)
            {
                LayerState &state = layer.second;
                if (!state.animation)
                    continue;

                // Calculate current animation time based on timestamp
                float elapsedTimeS = (timestampUs - state.startTimestampUs) / 1000000.0f;
                float currentAnimTime = state.animTimeAtTimestamp + elapsedTimeS * state.speed;

                // Apply animation at the calculated time
                state.animation->apply(node, currentAnimTime);
            }
        }

        void AnimationInstance::getJointMatrices(std::vector<mat4> &matrices) const
        {
            // Get the joint matrices from the node hierarchy
            node.getJointMatrices(matrices);
        }

        void AnimationInstance::getBoneMatrices(std::vector<mat4> &matrices, 
                                              const std::vector<int16_t> &joints, 
                                              const std::vector<mat4> &meshInverseBindMatrices) const
        {
            // Get the bone matrices for the specified joints
            matrices.resize(joints.size());
            std::vector<mat4> jointMatrices;
            getJointMatrices(jointMatrices);

            for (size_t i = 0; i < joints.size(); i++)
            {
                int16_t jointIdx = joints[i];
                if (jointIdx >= 0 && jointIdx < jointMatrices.size())
                {
                    // Compute bone matrix = joint matrix * inverse bind matrix
                    matrices[i] = jointMatrices[jointIdx] * meshInverseBindMatrices[i];
                }
                else
                {
                    // Use identity matrix for invalid joints
                    matrices[i] = mat4(1.0f);
                }
            }
        }

        void AnimationInstance::playAnimation(avs::uid cache_id, avs::uid anim_uid, uint32_t layer)
        {
            // Find the animation in the cache and play it
            GeometryCache *cache = GeometryCache::GetCache(cache_id);
            if (!cache)
                return;

            Animation *anim = cache->GetAnimation(anim_uid);
            if (!anim)
                return;

            LayerState &state = layers[layer];
            state.animation = anim;
            state.animation_uid = anim_uid;
            state.startTimestampUs = 0;
            state.animTimeAtTimestamp = 0.0f;
            state.speed = 1.0f;
        }

        void AnimationInstance::setAnimationState(std::chrono::microseconds timestampUs, 
                                                const teleport::core::ApplyAnimation &animationUpdate)
        {
            uint32_t layer = animationUpdate.animationLayer;
            LayerState &state = layers[layer];

            // Update the animation state based on the animation update
            GeometryCache *cache = GeometryCache::GetCache(node.getCacheID());
            if (!cache)
                return;

            Animation *anim = cache->GetAnimation(animationUpdate.animationID);
            if (!anim)
                return;

            state.animation = anim;
            state.animation_uid = animationUpdate.animationID;
            state.startTimestampUs = animationUpdate.timestampUs;
            state.animTimeAtTimestamp = animationUpdate.animTimeAtTimestamp;
            state.speed = animationUpdate.speed;
        }
    }
}