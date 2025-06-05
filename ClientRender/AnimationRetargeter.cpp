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

#pragma optimize("",off)

namespace teleport {
namespace clientrender {


/*
* The VRM Bone Names are:
hips 
spine 
chest 	
upperChest 	
neck 		
head 
leftEye 		
rightEye 		
jaw 		
Leg
 	
leftUpperLeg 	
leftLowerLeg 	
leftFoot 	
leftToes 		
rightUpperLeg 
rightLowerLeg 
rightFoot 	
rightToes 
 	
leftShoulder 	
leftUpperArm 	
leftLowerArm 	
leftHand 	
rightShoulder 
rightUpperArm 
rightLowerArm 
rightHand 	
 	
leftThumbMetacarpal 	
leftThumbProximal 		
leftThumbDistal 		
leftIndexProximal 		
leftIndexIntermediate 	
leftIndexDistal 		
leftMiddleProximal 		
leftMiddleIntermediate 	
leftMiddleDistal 		
leftRingProximal 		
leftRingIntermediate 	
leftRingDistal 		
leftLittleProximal 		
leftLittleIntermediate 	
leftLittleDistal 		
rightThumbMetacarpal 	
rightThumbProximal 		
rightThumbDistal 		
rightIndexProximal 		
rightIndexIntermediate 	
rightIndexDistal 		
rightMiddleProximal 	
rightMiddleIntermediate 
rightMiddleDistal 		
rightRingProximal 		
rightRingIntermediate 	
rightRingDistal 		
rightLittleProximal 	
rightLittleIntermediate 
rightLittleDistal
*/

ozz::string GetMappedBoneName(const ozz::string &bName)
{
	static std::unordered_map<ozz::string,ozz::string> mapping;
	if(!mapping.size())
	{
		mapping["spine1"]		 = "chest";
		mapping["spine2"]		 = "upperchest";

		// arms - with numbered prefix
		mapping["leftarm"]		 = "leftupperarm";
		mapping["leftforearm"]	 = "leftlowerarm";
		mapping["rightarm"]		 = "rightupperarm";
		mapping["rightforearm"]	 = "rightlowerarm";

		mapping["leftupleg"]	 = "leftupperleg";
		mapping["leftleg"]		 = "leftlowerleg";
		mapping["lefttoebase"]	 = "lefttoes";
		mapping["rightupleg"]	 = "rightupperleg";
		mapping["rightleg"]		 = "rightlowerleg";
		mapping["righttoebase"]	 = "righttoes";
	}
	ozz::string n=bName;
	std::transform(n.begin(), n.end(), n.begin(), ::tolower);
	n=std::regex_replace(n,std::regex::basic_regex("avatar_"),"");
	auto m=mapping.find(n);
	if(m==mapping.end())
		return n;
	return m->second;
};

const ozz::animation::offline::RawSkeleton::Joint* FindJointByName(
    const ozz::animation::offline::RawSkeleton& skeleton, 
    const ozz::string& name) ;

// Structure to hold retargeting information for a joint
struct JointRetargetInfo {
    ozz::math::Transform sourceBindPose;     // Source joint bind pose
    ozz::math::Transform targetBindPose;     // Target joint bind pose
    float scaleToParentFactor;               // Scale difference for parent bone length
    bool isValid;                            // Whether this joint exists in both skeletons
    
    JointRetargetInfo() : scaleToParentFactor(1.0f), isValid(false) {}
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

// Helper function to build retargeting info recursively
void BuildRetargetInfoRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>& joints,
                               const std::unordered_map<ozz::string, int>& target_joint_map,
                               const ozz::animation::offline::RawSkeleton& target_skeleton,
                               std::vector<JointRetargetInfo>& retarget_info,
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
                // Store bind poses directly
                retarget_info[joint_index].sourceBindPose = source_joint.transform;
                retarget_info[joint_index].targetBindPose = target_joint->transform;
                
                // Calculate scale factor for bone length differences
                float source_bone_length = CalculateBoneLength(source_joint.transform.translation);
                float target_bone_length = CalculateBoneLength(target_joint->transform.translation);
                
                if (source_bone_length > 0.0f) {
                    retarget_info[joint_index].scaleToParentFactor = target_bone_length / source_bone_length;
                } else {
                    retarget_info[joint_index].scaleToParentFactor = 1.0f;
                }
                
                retarget_info[joint_index].isValid = true;
            }
        }
        
        joint_index++;
        BuildRetargetInfoRecursive(source_joint.children, target_joint_map, target_skeleton, retarget_info, joint_index);
    }
}

// Build retargeting information for all joints
std::vector<JointRetargetInfo> BuildRetargetMap(
    const ozz::animation::offline::RawSkeleton& source_skeleton,
    const ozz::animation::offline::RawSkeleton& target_skeleton) {
    
    auto source_joint_map = BuildJointMap(source_skeleton);
    auto target_joint_map = BuildJointMap(target_skeleton);
    
    // Count total joints in source skeleton
    int total_joints = 0;
    CountJoints(source_skeleton.roots, total_joints);
    
    std::vector<JointRetargetInfo> retarget_info(total_joints);
    
    // Build retargeting info
    int joint_index = 0;
    BuildRetargetInfoRecursive(source_skeleton.roots, target_joint_map, target_skeleton, retarget_info, joint_index);
    
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
        if (result)
            return result;
    }
    return nullptr;
}
// Helper function to apply a retargeting transformation to a keyframe transform
ozz::math::Transform ApplyRetargeting(const ozz::math::Transform& source_transform,
                                     const ozz::math::Transform& source_bind,
                                     const ozz::math::Transform& target_bind) {
    ozz::math::Transform result;
    
    // For translation: apply the difference in bind pose translations plus scale
    ozz::math::Float3 translation_diff = target_bind.translation - source_bind.translation;
    result.translation = source_transform.translation + translation_diff;
    
    // For rotation: apply the difference in bind pose rotations
    ozz::math::Quaternion rotation_diff = target_bind.rotation * ozz::math::Conjugate(source_bind.rotation);
    result.rotation = rotation_diff * source_transform.rotation;
    
    // For scale: apply relative scaling with safety checks
    result.scale = ozz::math::Float3(
        source_bind.scale.x != 0.0f ? source_transform.scale.x * (target_bind.scale.x / source_bind.scale.x) : source_transform.scale.x,
        source_bind.scale.y != 0.0f ? source_transform.scale.y * (target_bind.scale.y / source_bind.scale.y) : source_transform.scale.y,
        source_bind.scale.z != 0.0f ? source_transform.scale.z * (target_bind.scale.z / source_bind.scale.z) : source_transform.scale.z
    );
    
    return result;
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
        
        // Retarget translation keyframes
        for (const auto& source_key : source_track.translations) {
            ozz::animation::offline::RawAnimation::TranslationKey target_key;
            target_key.time = source_key.time;
            
            // Create transform from translation keyframe
            ozz::math::Transform source_transform;
            source_transform.translation = source_key.value;
            source_transform.rotation = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);  // identity
            source_transform.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
            
            // Apply retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargeting(
                source_transform, 
                retarget.sourceBindPose, 
                retarget.targetBindPose
            );
            
            target_key.value = retargeted_transform.translation * retarget.scaleToParentFactor;
            target_track.translations.push_back(target_key);
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
            
            // Apply retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargeting(
                source_transform, 
                retarget.sourceBindPose, 
                retarget.targetBindPose
            );
            
            target_key.value = retargeted_transform.rotation;
            target_track.rotations.push_back(target_key);
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
            
            // Apply retargeting
            ozz::math::Transform retargeted_transform = ApplyRetargeting(
                source_transform, 
                retarget.sourceBindPose, 
                retarget.targetBindPose
            );
            
            target_key.value = retargeted_transform.scale;
            target_track.scales.push_back(target_key);
        }
    }
    
    return target_animation;
}

} // namespace animation
} // namespace clientrender