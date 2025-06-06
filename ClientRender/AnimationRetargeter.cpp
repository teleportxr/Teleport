#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/math_ex.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <regex>
#include <ozz/base/maths/internal/simd_math_config.h>
#include <ozz/base/maths/simd_math.h>
#include <TeleportCore/Logging.h>
#include "ClientRender/Animation.h"
#pragma optimize("",off)

namespace teleport {
namespace clientrender {


const ozz::animation::offline::RawSkeleton::Joint* FindJointByName(
    const ozz::animation::offline::RawSkeleton& skeleton, 
    const ozz::string& name) ;

// Structure to hold retargeting information for a joint
struct JointRetargetInfo {
    ozz::string parentName;                      // Parent joint name (empty for roots)
    ozz::math::Transform sourceParentModel;      // Source parent's model space transform
    ozz::math::Transform targetParentModel;      // Target parent's model space transform
    bool isValid;                                // Whether this joint exists in both skeletons
    
    JointRetargetInfo() : isValid(false) {
        sourceParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
        sourceParentModel.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        sourceParentModel.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
        
        targetParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
        targetParentModel.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        targetParentModel.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
    }
};

// Helper function to traverse joints and build joint map
void TraverseJoints(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints,
                   std::unordered_map<ozz::string, int>& joint_map,
                   int& joint_index) {
    for (const auto& joint : joints) {
        joint_map[GetMappedBoneName(joint.name)] = joint_index++;
        TraverseJoints(joint.children, joint_map, joint_index);
    }
}

// Helper function to build a joint name to index mapping
std::unordered_map<ozz::string, int> BuildJointMap(const ozz::animation::offline::RawSkeleton& skeleton) {
    std::unordered_map<ozz::string, int> joint_map;
    int joint_index = 0;
    TraverseJoints(skeleton.roots, joint_map, joint_index);
    return joint_map;
}

// Helper function to calculate bone length
float CalculateBoneLength(const ozz::math::Float3& translation) {
    return ozz::math::Length(translation);
}

// Helper function to safely extract float from SimdFloat4
float GetSimdComponent(const ozz::math::SimdFloat4& simd, int index) {
    return simd.m128_f32[index];
}

// Helper function to safely set float in SimdFloat4  
ozz::math::SimdFloat4 SetSimdComponent(float x, float y, float z, float w) {
    ozz::math::SimdFloat4 result;
    result.m128_f32[0] = x;
    result.m128_f32[1] = y;
    result.m128_f32[2] = z;
    result.m128_f32[3] = w;
    return result;
}

// Helper function to convert 3x3 rotation matrix to quaternion
ozz::math::Quaternion MatrixToQuaternion(const ozz::math::Float4x4& matrix) {
    // Extract the 3x3 rotation part (remove scale first)
    ozz::math::Float3 scale_vec(
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[0], 0), GetSimdComponent(matrix.cols[0], 1), GetSimdComponent(matrix.cols[0], 2))),
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[1], 0), GetSimdComponent(matrix.cols[1], 1), GetSimdComponent(matrix.cols[1], 2))),
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[2], 0), GetSimdComponent(matrix.cols[2], 1), GetSimdComponent(matrix.cols[2], 2)))
    );
    
    // Normalize to get pure rotation matrix
    float m00 = GetSimdComponent(matrix.cols[0], 0) / scale_vec.x;
    float m01 = GetSimdComponent(matrix.cols[0], 1) / scale_vec.x;
    float m02 = GetSimdComponent(matrix.cols[0], 2) / scale_vec.x;
    float m10 = GetSimdComponent(matrix.cols[1], 0) / scale_vec.y;
    float m11 = GetSimdComponent(matrix.cols[1], 1) / scale_vec.y;
    float m12 = GetSimdComponent(matrix.cols[1], 2) / scale_vec.y;
    float m20 = GetSimdComponent(matrix.cols[2], 0) / scale_vec.z;
    float m21 = GetSimdComponent(matrix.cols[2], 1) / scale_vec.z;
    float m22 = GetSimdComponent(matrix.cols[2], 2) / scale_vec.z;
    
    // Convert rotation matrix to quaternion using Shepperd's method
    float trace = m00 + m11 + m22;
    float x, y, z, w;
    
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f);
        w = s * 0.5f;
        s = 0.5f / s;
        x = (m21 - m12) * s;
        y = (m02 - m20) * s;
        z = (m10 - m01) * s;
    } else {
        if (m00 > m11 && m00 > m22) {
            float s = sqrtf(1.0f + m00 - m11 - m22);
            x = s * 0.5f;
            s = 0.5f / s;
            y = (m01 + m10) * s;
            z = (m02 + m20) * s;
            w = (m21 - m12) * s;
        } else if (m11 > m22) {
            float s = sqrtf(1.0f + m11 - m00 - m22);
            y = s * 0.5f;
            s = 0.5f / s;
            x = (m01 + m10) * s;
            z = (m12 + m21) * s;
            w = (m02 - m20) * s;
        } else {
            float s = sqrtf(1.0f + m22 - m00 - m11);
            z = s * 0.5f;
            s = 0.5f / s;
            x = (m02 + m20) * s;
            y = (m12 + m21) * s;
            w = (m10 - m01) * s;
        }
    }
    
    return ozz::math::Quaternion(x, y, z, w);
}

// Helper function to convert transform to matrix and back
ozz::math::Transform MatrixToTransform(const ozz::math::Float4x4& matrix) {
    ozz::math::Transform transform;
    
    // Extract translation
    transform.translation = ozz::math::Float3(GetSimdComponent(matrix.cols[3], 0), GetSimdComponent(matrix.cols[3], 1), GetSimdComponent(matrix.cols[3], 2));
    
    // Extract scale
    ozz::math::Float3 scale_vec(
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[0], 0), GetSimdComponent(matrix.cols[0], 1), GetSimdComponent(matrix.cols[0], 2))),
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[1], 0), GetSimdComponent(matrix.cols[1], 1), GetSimdComponent(matrix.cols[1], 2))),
        ozz::math::Length(ozz::math::Float3(GetSimdComponent(matrix.cols[2], 0), GetSimdComponent(matrix.cols[2], 1), GetSimdComponent(matrix.cols[2], 2)))
    );
    transform.scale = scale_vec;
    
    // Extract rotation using our custom function
    transform.rotation = MatrixToQuaternion(matrix);
    
    return transform;
}

ozz::math::Float4x4 TransformToMatrix(const ozz::math::Transform& transform) {
    // Create identity matrix
    ozz::math::Float4x4 result = ozz::math::Float4x4::identity();
    
    // Apply rotation - convert quaternion to rotation matrix manually
    const float x = transform.rotation.x;
    const float y = transform.rotation.y;
    const float z = transform.rotation.z;
    const float w = transform.rotation.w;
    
    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;
    const float xx = x * x2;
    const float xy = x * y2;
    const float xz = x * z2;
    const float yy = y * y2;
    const float yz = y * z2;
    const float zz = z * z2;
    const float wx = w * x2;
    const float wy = w * y2;
    const float wz = w * z2;
    
    // Build rotation and scale matrix
    float m00 = (1.0f - (yy + zz)) * transform.scale.x;
    float m01 = (xy + wz) * transform.scale.x;
    float m02 = (xz - wy) * transform.scale.x;
    
    float m10 = (xy - wz) * transform.scale.y;
    float m11 = (1.0f - (xx + zz)) * transform.scale.y;
    float m12 = (yz + wx) * transform.scale.y;
    
    float m20 = (xz + wy) * transform.scale.z;
    float m21 = (yz - wx) * transform.scale.z;
    float m22 = (1.0f - (xx + yy)) * transform.scale.z;
    
    // Set matrix columns using SIMD interface
    result.cols[0] = SetSimdComponent(m00, m01, m02, 0.0f);
    result.cols[1] = SetSimdComponent(m10, m11, m12, 0.0f);
    result.cols[2] = SetSimdComponent(m20, m21, m22, 0.0f);
    result.cols[3] = SetSimdComponent(transform.translation.x, transform.translation.y, transform.translation.z, 1.0f);
    
    return result;
}

// Helper function to count total joints
void CountJoints(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints, int& total_joints) {
    total_joints += (int)joints.size();
    for (const auto& joint : joints) {
        CountJoints(joint.children, total_joints);
    }
}
    
bool find_parent(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints, 
                      const ozz::string& target, const ozz::string& current_parent, ozz::string& parent_name)
       {
        for (const auto& joint : joints) {
            ozz::string joint_name = GetMappedBoneName(joint.name);
            if (joint_name == target) {
                parent_name = current_parent;
                return true;
            }
            if (find_parent(joint.children, target, joint_name, parent_name)) {
                return true;
            }
        }
        return false;
    };
// Helper function to get parent joint name
ozz::string GetParentJointName(const ozz::animation::offline::RawSkeleton& skeleton, 
                              const ozz::string& joint_name) {
    ozz::string parent_name = "";
    find_parent(skeleton.roots, GetMappedBoneName(joint_name), "", parent_name);
    return parent_name;
}
// Helper function to recursively find joint chain
bool FindJointChainRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints, 
                            const ozz::string& target,
                            ozz::vector<const ozz::animation::offline::RawSkeleton::Joint*>& chain) {
    for (const auto& joint : joints) {
        chain.push_back(&joint);
        if (GetMappedBoneName(joint.name) == GetMappedBoneName(target)) {
            return true; // Found target
        }
        if (FindJointChainRecursive(joint.children, target, chain)) {
            return true; // Found in children
        }
        chain.pop_back(); // Backtrack
    }
    return false;
}

// Helper function to compute model space transform from root to a specific joint
ozz::math::Transform ComputeModelSpaceTransform(const ozz::animation::offline::RawSkeleton& skeleton, 
                                                const ozz::string& joint_name) {
    ozz::vector<const ozz::animation::offline::RawSkeleton::Joint*> chain;
    
    if (!FindJointChainRecursive(skeleton.roots, joint_name, chain)) {
        // Joint not found, return identity
        ozz::math::Transform identity;
        identity.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
        identity.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        identity.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
        return identity;
    }
    
    // Accumulate transforms from root to target
    ozz::math::Transform result;
    result.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
    result.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    result.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
    
    for (const auto* joint : chain) {
        // Combine transforms: result = result * joint->transform
        
        // Scale the translation by current scale
        ozz::math::Float3 scaled_translation = ozz::math::Float3(
            joint->transform.translation.x * result.scale.x,
            joint->transform.translation.y * result.scale.y,
            joint->transform.translation.z * result.scale.z
        );
        
        // Rotate the scaled translation by current rotation
        ozz::math::Float3 rotated_translation = ozz::math::TransformVector(result.rotation, scaled_translation);
        
        // Add to result translation
        result.translation = result.translation + rotated_translation;
        
        // Combine rotations
        result.rotation = result.rotation * joint->transform.rotation;
        
        // Combine scales
        result.scale = ozz::math::Float3(
            result.scale.x * joint->transform.scale.x,
            result.scale.y * joint->transform.scale.y,
            result.scale.z * joint->transform.scale.z
        );
    }
    
    return result;
}
// Helper function to build retargeting info recursively
void BuildRetargetInfoRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints,
                               const std::unordered_map<ozz::string, int>& target_joint_map,
                               const ozz::animation::offline::RawSkeleton& source_skeleton,
                               const ozz::animation::offline::RawSkeleton& target_skeleton,
                               ozz::vector<JointRetargetInfo>& retarget_info,
                               int& joint_index) {
    for (const auto& source_joint : joints) {
	    ozz::string source_name=GetMappedBoneName(source_joint.name);
        auto target_it = target_joint_map.find(source_name);
        if (target_it == target_joint_map.end()) {
        // not found?
            TELEPORT_WARN("Bone {} not found in target.",source_joint.name);
        }
        
        if (target_it != target_joint_map.end()) {
            // Find corresponding joint in target skeleton
            const auto* target_joint = FindJointByName(target_skeleton, source_name);
            
            if (target_joint) {
                // Get parent joint name
                auto &ret = retarget_info[joint_index];
                ret.parentName = GetParentJointName(source_skeleton, source_name);
                
                // Compute parent model space transforms
                if (!ret.parentName.empty()) {
                    ret.sourceParentModel = ComputeModelSpaceTransform(source_skeleton, ret.parentName);
                    ret.targetParentModel = ComputeModelSpaceTransform(target_skeleton, ret.parentName);
                } else {
                    // Root joint - parent is identity
                    ret.sourceParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
                    ret.sourceParentModel.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                    ret.sourceParentModel.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
                    
                    ret.targetParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
                    ret.targetParentModel.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                    ret.targetParentModel.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
                }
                
                ret.isValid = true;
            }
        }
        
        joint_index++;
        BuildRetargetInfoRecursive(source_joint.children, target_joint_map, source_skeleton, target_skeleton, retarget_info, joint_index);
    }
}

// Build retargeting information for all joints
ozz::vector<JointRetargetInfo> BuildRetargetMap(
    const ozz::animation::offline::RawSkeleton& source_skeleton,
    const ozz::animation::offline::RawSkeleton& target_skeleton) {
    
    auto source_joint_map = BuildJointMap(source_skeleton);
    auto target_joint_map = BuildJointMap(target_skeleton);
    
    // Count total joints in source skeleton
    int total_joints = 0;
    CountJoints(source_skeleton.roots, total_joints);
    
    ozz::vector<JointRetargetInfo> retarget_info(total_joints);
    
    // Build retargeting info
    int joint_index = 0;
    BuildRetargetInfoRecursive(source_skeleton.roots, target_joint_map, source_skeleton, target_skeleton, retarget_info, joint_index);
    
    return retarget_info;
}

// Helper function to recursively find joint by name
const ozz::animation::offline::RawSkeleton::Joint* FindJointRecursive(
    const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints, 
    const ozz::string& name) {
    for (const auto& joint : joints) {
        if (GetMappedBoneName(joint.name)== name) {
            return &joint;
        }
        auto result = FindJointRecursive(joint.children, name);
        if (result) return result;
    }
    return nullptr;
}

// Helper function to compute local transform from model space transform and parent model space
ozz::math::Transform ComputeLocalFromModel(const ozz::math::Transform& model_transform,
                                          const ozz::math::Transform& parent_model_transform) {
    ozz::math::Transform result;
    
    // Compute inverse parent transform
    ozz::math::Quaternion parent_inv_rotation = ozz::math::Conjugate(parent_model_transform.rotation);
    ozz::math::Float3 parent_inv_scale = ozz::math::Float3(
        parent_model_transform.scale.x != 0.0f ? 1.0f / parent_model_transform.scale.x : 1.0f,
        parent_model_transform.scale.y != 0.0f ? 1.0f / parent_model_transform.scale.y : 1.0f,
        parent_model_transform.scale.z != 0.0f ? 1.0f / parent_model_transform.scale.z : 1.0f
    );
    
    // Local translation = inv_parent_rotation * inv_parent_scale * (model_translation - parent_translation)
    ozz::math::Float3 translation_diff = model_transform.translation - parent_model_transform.translation;
    ozz::math::Float3 scaled_diff = ozz::math::Float3(
        translation_diff.x * parent_inv_scale.x,
        translation_diff.y * parent_inv_scale.y,
        translation_diff.z * parent_inv_scale.z
    );
    result.translation = ozz::math::TransformVector(parent_inv_rotation, scaled_diff);
    
    // Local rotation = inv_parent_rotation * model_rotation
    result.rotation = parent_inv_rotation * model_transform.rotation;
    
    // Local scale = model_scale / parent_scale
    result.scale = ozz::math::Float3(
        parent_model_transform.scale.x != 0.0f ? model_transform.scale.x / parent_model_transform.scale.x : model_transform.scale.x,
        parent_model_transform.scale.y != 0.0f ? model_transform.scale.y / parent_model_transform.scale.y : model_transform.scale.y,
        parent_model_transform.scale.z != 0.0f ? model_transform.scale.z / parent_model_transform.scale.z : model_transform.scale.z
    );
    
    return result;
}
// Helper function to apply a retargeting transformation to a keyframe transform in model space
ozz::math::Transform ApplyRetargetingModelSpace(const ozz::math::Transform& source_local_transform,
                                               const ozz::string& joint_name,
                                               const ozz::animation::offline::RawSkeleton& source_skeleton,
                                               const ozz::animation::offline::RawSkeleton& target_skeleton,
                                               const ozz::math::Transform& source_parent_model,
                                               const ozz::math::Transform& target_parent_model) {
    // Get bind poses in model space xyz = left up forward
    ozz::math::Transform source_bind_model = ComputeModelSpaceTransform(source_skeleton, joint_name);
    ozz::math::Transform target_bind_model = ComputeModelSpaceTransform(target_skeleton, joint_name);
    
    // Convert local keyframe to model space relative to source bind pose
    ozz::math::Transform source_local_bind = ComputeLocalFromModel(source_bind_model, source_parent_model);
    
    // Combine parent model transform with local keyframe transform
    ozz::math::Transform source_model_keyframe;
    
    // Scale the translation by parent scale
    ozz::math::Float3 scaled_translation = ozz::math::Float3(
        source_local_transform.translation.x * source_parent_model.scale.x,
        source_local_transform.translation.y * source_parent_model.scale.y,
        source_local_transform.translation.z * source_parent_model.scale.z
    );
    
    // Rotate the scaled translation by parent rotation
    ozz::math::Float3 rotated_translation = ozz::math::TransformVector(source_parent_model.rotation, scaled_translation);
    
    // Final model space transform
    source_model_keyframe.translation = source_parent_model.translation + rotated_translation;
    source_model_keyframe.rotation = source_parent_model.rotation * source_local_transform.rotation;
    source_model_keyframe.scale = ozz::math::Float3(
        source_parent_model.scale.x * source_local_transform.scale.x,
        source_parent_model.scale.y * source_local_transform.scale.y,
        source_parent_model.scale.z * source_local_transform.scale.z
    );
    
    // Apply retargeting in model space
    // Compute the difference from source bind pose
    ozz::math::Transform model_space_delta;
    
    // Translation delta
    model_space_delta.translation = source_model_keyframe.translation - source_bind_model.translation;
    
    // Rotation delta
    ozz::math::Quaternion source_bind_inv = ozz::math::Conjugate(source_bind_model.rotation);
    model_space_delta.rotation = source_bind_inv * source_model_keyframe.rotation;
    
    // Scale delta
    model_space_delta.scale = ozz::math::Float3(
        source_bind_model.scale.x != 0.0f ? source_model_keyframe.scale.x / source_bind_model.scale.x : 1.0f,
        source_bind_model.scale.y != 0.0f ? source_model_keyframe.scale.y / source_bind_model.scale.y : 1.0f,
        source_bind_model.scale.z != 0.0f ? source_model_keyframe.scale.z / source_bind_model.scale.z : 1.0f
    );
    
    // Apply delta to target bind pose
    ozz::math::Transform target_model_result;
    target_model_result.translation = target_bind_model.translation + model_space_delta.translation;
    target_model_result.rotation = target_bind_model.rotation * model_space_delta.rotation;
    target_model_result.scale = ozz::math::Float3(
        target_bind_model.scale.x * model_space_delta.scale.x,
        target_bind_model.scale.y * model_space_delta.scale.y,
        target_bind_model.scale.z * model_space_delta.scale.z
    );
    
    // Convert back to local space
    return ComputeLocalFromModel(target_model_result, target_parent_model);
}

// Helper function to find joint by name in skeleton
const ozz::animation::offline::RawSkeleton::Joint* FindJointByName(
    const ozz::animation::offline::RawSkeleton& skeleton, 
    const ozz::string& name) {
    return FindJointRecursive(skeleton.roots, name);
}

// Main retargeting function
ozz::animation::offline::RawAnimation RetargetAnimation(
    const ozz::animation::offline::RawAnimation& source_animation,
    const ozz::animation::offline::RawSkeleton& source_skeleton,
    const ozz::animation::offline::RawSkeleton& target_skeleton) {
    
    // Build retargeting mapping
    auto retarget_info = BuildRetargetMap(source_skeleton, target_skeleton);
    
    // Create output animation
    ozz::animation::offline::RawAnimation target_animation;
    target_animation.duration = source_animation.duration;
    target_animation.name = source_animation.name + "_retargeted";
    
    // Resize tracks to match source animation
    target_animation.tracks.resize(source_animation.tracks.size());
    
    // Process each track
    for (size_t track_idx = 0; track_idx < source_animation.tracks.size(); ++track_idx) {
        const auto& source_track = source_animation.tracks[track_idx];
        auto& target_track = target_animation.tracks[track_idx];
        
        // Check if this track has valid retargeting info
        if (track_idx >= retarget_info.size() || !retarget_info[track_idx].isValid) {
            // Copy original track if no retargeting info available
            target_track = source_track;
            continue;
        }
        
        const auto& retarget = retarget_info[track_idx];
        
        // Get joint name for this track (assuming track index matches skeleton joint order)
        ozz::string joint_name = "";
        int current_index = 0;
        std::function<bool(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>&)> find_joint_name = 
            [&](const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints) -> bool {
            for (const auto& joint : joints) {
                if (current_index == track_idx) {
                    joint_name = joint.name;
                    return true;
                }
                current_index++;
                if (find_joint_name(joint.children)) {
                    return true;
                }
            }
            return false;
        };
        find_joint_name(source_skeleton.roots);
        
        // Retarget translation keyframes
        for (const auto& source_key : source_track.translations) {
            ozz::animation::offline::RawAnimation::TranslationKey target_key;
            target_key.time = source_key.time;
            
            // Create transform from translation keyframe
            ozz::math::Transform source_transform;
            source_transform.translation = source_key.value;
            source_transform.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);  // identity
            source_transform.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
            
            // Apply model space retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargetingModelSpace(
                source_transform, 
                joint_name,
                source_skeleton,
                target_skeleton,
                retarget.sourceParentModel,
                retarget.targetParentModel
            );
            
            target_key.value = retargeted_transform.translation;
            target_track.translations.push_back(target_key);
            break;
        }
        
        // Retarget rotation keyframes
        for (const auto& source_key : source_track.rotations) {
            ozz::animation::offline::RawAnimation::RotationKey target_key;
            target_key.time = source_key.time;
            
            // Create transform from rotation keyframe
            ozz::math::Transform source_transform;
            source_transform.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
            source_transform.rotation = source_key.value;
            source_transform.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
            
            // Apply model space retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargetingModelSpace(
                source_transform, 
                joint_name,
                source_skeleton,
                target_skeleton,
                retarget.sourceParentModel,
                retarget.targetParentModel
            );
            
            target_key.value = retargeted_transform.rotation;
            target_track.rotations.push_back(target_key);
            break;
        }
        
        // Retarget scale keyframes
        for (const auto& source_key : source_track.scales) {
            ozz::animation::offline::RawAnimation::ScaleKey target_key;
            target_key.time = source_key.time;
            
            // Create transform from scale keyframe
            ozz::math::Transform source_transform;
            source_transform.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
            source_transform.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);  // identity
            source_transform.scale = source_key.value;
            
            // Apply model space retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargetingModelSpace(
                source_transform, 
                joint_name,
                source_skeleton,
                target_skeleton,
                retarget.sourceParentModel,
                retarget.targetParentModel
            );
            
            target_key.value = retargeted_transform.scale;
            target_track.scales.push_back(target_key);
            break;
        }
    }
    
    return target_animation;
}

} 
} 