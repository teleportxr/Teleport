#include "ClientRender/Animation.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/vec_float.h"
#include <TeleportCore/Logging.h>
#include <cmath>
#include <functional>
#include <ozz/base/maths/internal/simd_math_config.h>
#include <ozz/base/maths/simd_math.h>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
#pragma optimize("", off)

namespace teleport
{
	namespace clientrender
	{

		const ozz::animation::offline::RawSkeleton::Joint *FindJointByName(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &name);

		// Structure to hold retargeting information for a joint
		struct JointRetargetInfo
		{
			ozz::string			 parentName;		// Parent joint name (empty for roots)
			ozz::math::Transform sourceParentModel; // Source parent's model space transform
			ozz::math::Transform targetParentModel; // Target parent's model space transform
			bool				 isValid;			// Whether this joint exists in both skeletons

			JointRetargetInfo() : isValid(false)
			{
				sourceParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
				sourceParentModel.rotation	  = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
				sourceParentModel.scale		  = ozz::math::Float3(1.0f, 1.0f, 1.0f);

				targetParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
				targetParentModel.rotation	  = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
				targetParentModel.scale		  = ozz::math::Float3(1.0f, 1.0f, 1.0f);
			}
		};

		// Helper function to traverse joints and build joint map
		void TraverseJoints(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints,
							std::unordered_map<ozz::string, int>						   &joint_map,
							int															   &joint_index)
		{
			for (const auto &joint : joints)
			{
				joint_map[GetMappedBoneName(joint.name)] = joint_index++;
				TraverseJoints(joint.children, joint_map, joint_index);
			}
		}

		// Helper function to build a joint name to index mapping
		std::unordered_map<ozz::string, int> BuildJointMap(const ozz::animation::offline::RawSkeleton &skeleton)
		{
			std::unordered_map<ozz::string, int> joint_map;
			int									 joint_index = 0;
			TraverseJoints(skeleton.roots, joint_map, joint_index);
			return joint_map;
		}

		// Helper function to calculate bone length
		float CalculateBoneLength(const ozz::math::Float3 &translation)
		{
			return ozz::math::Length(translation);
		}

		// Helper function to safely extract float from SimdFloat4
		float GetSimdComponent(const ozz::math::SimdFloat4 &simd, int index)
		{
			return simd.m128_f32[index];
		}

		// Helper function to safely set float in SimdFloat4
		ozz::math::SimdFloat4 SetSimdComponent(float x, float y, float z, float w)
		{
			ozz::math::SimdFloat4 result;
			result.m128_f32[0] = x;
			result.m128_f32[1] = y;
			result.m128_f32[2] = z;
			result.m128_f32[3] = w;
			return result;
		}

		// Helper function to convert 3x3 rotation matrix to quaternion
		ozz::math::Quaternion MatrixToQuaternion(const ozz::math::Float4x4 &matrix)
		{
			// Extract the 3x3 rotation part (remove scale first)
			ozz::math::Float3 scale_vec(ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[0], 0), GetSimdComponent(matrix.cols[0], 1), GetSimdComponent(matrix.cols[0], 2))),
										ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[1], 0), GetSimdComponent(matrix.cols[1], 1), GetSimdComponent(matrix.cols[1], 2))),
										ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[2], 0), GetSimdComponent(matrix.cols[2], 1), GetSimdComponent(matrix.cols[2], 2))));

			// Normalize to get pure rotation matrix
			float m00	= GetSimdComponent(matrix.cols[0], 0) / scale_vec.x;
			float m01	= GetSimdComponent(matrix.cols[0], 1) / scale_vec.x;
			float m02	= GetSimdComponent(matrix.cols[0], 2) / scale_vec.x;
			float m10	= GetSimdComponent(matrix.cols[1], 0) / scale_vec.y;
			float m11	= GetSimdComponent(matrix.cols[1], 1) / scale_vec.y;
			float m12	= GetSimdComponent(matrix.cols[1], 2) / scale_vec.y;
			float m20	= GetSimdComponent(matrix.cols[2], 0) / scale_vec.z;
			float m21	= GetSimdComponent(matrix.cols[2], 1) / scale_vec.z;
			float m22	= GetSimdComponent(matrix.cols[2], 2) / scale_vec.z;

			// Convert rotation matrix to quaternion using Shepperd's method
			float trace = m00 + m11 + m22;
			float x, y, z, w;

			if (trace > 0.0f)
			{
				float s = sqrtf(trace + 1.0f);
				w		= s * 0.5f;
				s		= 0.5f / s;
				x		= (m21 - m12) * s;
				y		= (m02 - m20) * s;
				z		= (m10 - m01) * s;
			}
			else
			{
				if (m00 > m11 && m00 > m22)
				{
					float s = sqrtf(1.0f + m00 - m11 - m22);
					x		= s * 0.5f;
					s		= 0.5f / s;
					y		= (m01 + m10) * s;
					z		= (m02 + m20) * s;
					w		= (m21 - m12) * s;
				}
				else if (m11 > m22)
				{
					float s = sqrtf(1.0f + m11 - m00 - m22);
					y		= s * 0.5f;
					s		= 0.5f / s;
					x		= (m01 + m10) * s;
					z		= (m12 + m21) * s;
					w		= (m02 - m20) * s;
				}
				else
				{
					float s = sqrtf(1.0f + m22 - m00 - m11);
					z		= s * 0.5f;
					s		= 0.5f / s;
					x		= (m02 + m20) * s;
					y		= (m12 + m21) * s;
					w		= (m10 - m01) * s;
				}
			}

			return ozz::math::Quaternion(x, y, z, w);
		}

		// Helper function to convert transform to matrix and back
		ozz::math::Transform MatrixToTransform(const ozz::math::Float4x4 &matrix)
		{
			ozz::math::Transform transform;

			// Extract translation
			transform.translation =
				ozz::math::Float3(GetSimdComponent(matrix.cols[3], 0), GetSimdComponent(matrix.cols[3], 1), GetSimdComponent(matrix.cols[3], 2));

			// Extract scale
			ozz::math::Float3 scale_vec(ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[0], 0), GetSimdComponent(matrix.cols[0], 1), GetSimdComponent(matrix.cols[0], 2))),
										ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[1], 0), GetSimdComponent(matrix.cols[1], 1), GetSimdComponent(matrix.cols[1], 2))),
										ozz::math::Length(ozz::math::Float3(
											GetSimdComponent(matrix.cols[2], 0), GetSimdComponent(matrix.cols[2], 1), GetSimdComponent(matrix.cols[2], 2))));
			transform.scale	   = scale_vec;

			// Extract rotation using our custom function
			transform.rotation = MatrixToQuaternion(matrix);

			return transform;
		}

		ozz::math::Float4x4 TransformToMatrix(const ozz::math::Transform &transform)
		{
			// Create identity matrix
			ozz::math::Float4x4 result = ozz::math::Float4x4::identity();

			// Apply rotation - convert quaternion to rotation matrix manually
			const float x			   = transform.rotation.x;
			const float y			   = transform.rotation.y;
			const float z			   = transform.rotation.z;
			const float w			   = transform.rotation.w;

			const float x2			   = x + x;
			const float y2			   = y + y;
			const float z2			   = z + z;
			const float xx			   = x * x2;
			const float xy			   = x * y2;
			const float xz			   = x * z2;
			const float yy			   = y * y2;
			const float yz			   = y * z2;
			const float zz			   = z * z2;
			const float wx			   = w * x2;
			const float wy			   = w * y2;
			const float wz			   = w * z2;

			// Build rotation and scale matrix
			float m00				   = (1.0f - (yy + zz)) * transform.scale.x;
			float m01				   = (xy + wz) * transform.scale.x;
			float m02				   = (xz - wy) * transform.scale.x;

			float m10				   = (xy - wz) * transform.scale.y;
			float m11				   = (1.0f - (xx + zz)) * transform.scale.y;
			float m12				   = (yz + wx) * transform.scale.y;

			float m20				   = (xz + wy) * transform.scale.z;
			float m21				   = (yz - wx) * transform.scale.z;
			float m22				   = (1.0f - (xx + yy)) * transform.scale.z;

			// Set matrix columns using SIMD interface
			result.cols[0]			   = SetSimdComponent(m00, m01, m02, 0.0f);
			result.cols[1]			   = SetSimdComponent(m10, m11, m12, 0.0f);
			result.cols[2]			   = SetSimdComponent(m20, m21, m22, 0.0f);
			result.cols[3]			   = SetSimdComponent(transform.translation.x, transform.translation.y, transform.translation.z, 1.0f);

			return result;
		}

		// Helper function to count total joints
		void CountJoints(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints, int &total_joints)
		{
			total_joints += (int)joints.size();
			for (const auto &joint : joints)
			{
				CountJoints(joint.children, total_joints);
			}
		}

		bool find_parent(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints,
						 const ozz::string												&target,
						 const ozz::string												&current_parent,
						 ozz::string													&parent_name)
		{
			for (const auto &joint : joints)
			{
				ozz::string joint_name = GetMappedBoneName(joint.name);
				if (joint_name == target)
				{
					parent_name = current_parent;
					return true;
				}
				if (find_parent(joint.children, target, joint_name, parent_name))
				{
					return true;
				}
			}
			return false;
		};
		// Helper function to get parent joint name
		ozz::string GetParentJointName(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &joint_name)
		{
			ozz::string parent_name = "";
			find_parent(skeleton.roots, GetMappedBoneName(joint_name), "", parent_name);
			return parent_name;
		}
		// Helper function to recursively find joint chain
		bool FindJointChainRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint>	  &joints,
									 const ozz::string												  &target,
									 ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> &chain)
		{
			for (const auto &joint : joints)
			{
				chain.push_back(&joint);
				if (GetMappedBoneName(joint.name) == GetMappedBoneName(target))
				{
					return true; // Found target
				}
				if (FindJointChainRecursive(joint.children, target, chain))
				{
					return true; // Found in children
				}
				chain.pop_back(); // Backtrack
			}
			return false;
		}

		// Helper function to compute model space transform from root to a specific joint
		ozz::math::Transform ComputeModelSpaceTransform(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &joint_name)
		{
			ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> chain;

			if (!FindJointChainRecursive(skeleton.roots, joint_name, chain))
			{
				// Joint not found, return identity
				ozz::math::Transform identity;
				identity.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
				identity.rotation	 = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
				identity.scale		 = ozz::math::Float3(1.0f, 1.0f, 1.0f);
				return identity;
			}

			// Accumulate transforms from root to target
			ozz::math::Transform result;
			result.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
			result.rotation	   = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
			result.scale	   = ozz::math::Float3(1.0f, 1.0f, 1.0f);

			for (const auto *joint : chain)
			{
				// Combine transforms: result = result * joint->transform

				// Scale the translation by current scale
				ozz::math::Float3 scaled_translation  = ozz::math::Float3(joint->transform.translation.x * result.scale.x,
																		  joint->transform.translation.y * result.scale.y,
																		  joint->transform.translation.z * result.scale.z);

				// Rotate the scaled translation by current rotation
				ozz::math::Float3 rotated_translation = ozz::math::TransformVector(result.rotation, scaled_translation);

				// Add to result translation
				result.translation					  = result.translation + rotated_translation;

				// Combine rotations
				result.rotation						  = result.rotation * joint->transform.rotation;

				// Combine scales
				result.scale						  = ozz::math::Float3(
					 result.scale.x * joint->transform.scale.x, result.scale.y * joint->transform.scale.y, result.scale.z * joint->transform.scale.z);
			}

			return result;
		}
		// Helper function to build retargeting info recursively
		void BuildRetargetInfoRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints,
										const std::unordered_map<ozz::string, int>					   &target_joint_map,
										const ozz::animation::offline::RawSkeleton					   &source_skeleton,
										const ozz::animation::offline::RawSkeleton					   &target_skeleton,
										ozz::vector<JointRetargetInfo>								   &retarget_info,
										int															   &joint_index)
		{
			for (const auto &source_joint : joints)
			{
				ozz::string source_name = GetMappedBoneName(source_joint.name);
				auto		target_it	= target_joint_map.find(source_name);
				if (target_it == target_joint_map.end())
				{
					// not found?
					TELEPORT_WARN("Bone {} not found in target.", source_joint.name);
				}

				if (target_it != target_joint_map.end())
				{
					// Find corresponding joint in target skeleton
					const auto *target_joint = FindJointByName(target_skeleton, source_name);

					if (target_joint)
					{
						// Get parent joint name
						auto &ret	   = retarget_info[joint_index];
						ret.parentName = GetParentJointName(source_skeleton, source_name);

						// Compute parent model space transforms
						if (!ret.parentName.empty())
						{
							ret.sourceParentModel = ComputeModelSpaceTransform(source_skeleton, ret.parentName);
							ret.targetParentModel = ComputeModelSpaceTransform(target_skeleton, ret.parentName);
						}
						else
						{
							// Root joint - parent is identity
							ret.sourceParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
							ret.sourceParentModel.rotation	  = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
							ret.sourceParentModel.scale		  = ozz::math::Float3(1.0f, 1.0f, 1.0f);

							ret.targetParentModel.translation = ozz::math::Float3(0.0f, 0.0f, 0.0f);
							ret.targetParentModel.rotation	  = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
							ret.targetParentModel.scale		  = ozz::math::Float3(1.0f, 1.0f, 1.0f);
						}

						ret.isValid = true;
					}
				}

				joint_index++;
				BuildRetargetInfoRecursive(source_joint.children, target_joint_map, source_skeleton, target_skeleton, retarget_info, joint_index);
			}
		}

		// Build retargeting information for all joints
		ozz::vector<JointRetargetInfo> BuildRetargetMap(const ozz::animation::offline::RawSkeleton &source_skeleton,
														const ozz::animation::offline::RawSkeleton &target_skeleton)
		{

			auto source_joint_map = BuildJointMap(source_skeleton);
			auto target_joint_map = BuildJointMap(target_skeleton);

			// Count total joints in source skeleton
			int total_joints	  = 0;
			CountJoints(source_skeleton.roots, total_joints);

			ozz::vector<JointRetargetInfo> retarget_info(total_joints);

			// Build retargeting info
			int joint_index = 0;
			BuildRetargetInfoRecursive(source_skeleton.roots, target_joint_map, source_skeleton, target_skeleton, retarget_info, joint_index);

			return retarget_info;
		}

		// Helper function to recursively find joint by name
		const ozz::animation::offline::RawSkeleton::Joint *FindJointRecursive(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints,
																			  const ozz::string												 &name)
		{
			for (const auto &joint : joints)
			{
				if (GetMappedBoneName(joint.name) == name)
				{
					return &joint;
				}
				auto result = FindJointRecursive(joint.children, name);
				if (result)
				{
					return result;
				}
			}
			return nullptr;
		}

		// Helper function to compute local transform from model space transform and parent model space
		ozz::math::Transform ComputeLocalFromModel(const ozz::math::Transform &model_transform, const ozz::math::Transform &parent_model_transform)
		{
			ozz::math::Transform result;

			// Compute inverse parent transform
			ozz::math::Quaternion parent_inv_rotation = ozz::math::Conjugate(parent_model_transform.rotation);
			ozz::math::Float3	  parent_inv_scale	  = ozz::math::Float3(parent_model_transform.scale.x != 0.0f ? 1.0f / parent_model_transform.scale.x : 1.0f,
																	  parent_model_transform.scale.y != 0.0f ? 1.0f / parent_model_transform.scale.y : 1.0f,
																   parent_model_transform.scale.z != 0.0f ? 1.0f / parent_model_transform.scale.z : 1.0f);

			// Local translation = inv_parent_rotation * inv_parent_scale * (model_translation - parent_translation)
			ozz::math::Float3 translation_diff = model_transform.translation - parent_model_transform.translation;
			ozz::math::Float3 scaled_diff =
				ozz::math::Float3(translation_diff.x * parent_inv_scale.x, translation_diff.y * parent_inv_scale.y, translation_diff.z * parent_inv_scale.z);
			result.translation = ozz::math::TransformVector(parent_inv_rotation, scaled_diff);

			// Local rotation = inv_parent_rotation * model_rotation
			result.rotation	   = parent_inv_rotation * model_transform.rotation;

			// Local scale = model_scale / parent_scale
			result.scale =
				ozz::math::Float3(parent_model_transform.scale.x != 0.0f ? model_transform.scale.x / parent_model_transform.scale.x : model_transform.scale.x,
								  parent_model_transform.scale.y != 0.0f ? model_transform.scale.y / parent_model_transform.scale.y : model_transform.scale.y,
								  parent_model_transform.scale.z != 0.0f ? model_transform.scale.z / parent_model_transform.scale.z : model_transform.scale.z);

			return result;
		}
		// Helper function to apply a retargeting transformation to a keyframe transform in model space
		ozz::math::Transform ApplyRetargetingModelSpace(const ozz::math::Transform				   &source_local_transform,
														const ozz::string						   &joint_name,
														const ozz::animation::offline::RawSkeleton &source_skeleton,
														const ozz::animation::offline::RawSkeleton &target_skeleton,
														const ozz::math::Transform				   &source_parent_model,
														const ozz::math::Transform				   &target_parent_model)
		{
			// Get bind poses in model space xyz = left up forward
			ozz::math::Transform source_bind_model = ComputeModelSpaceTransform(source_skeleton, joint_name);
			ozz::math::Transform target_bind_model = ComputeModelSpaceTransform(target_skeleton, joint_name);

			// Convert local keyframe to model space relative to source bind pose
			ozz::math::Transform source_local_bind = ComputeLocalFromModel(source_bind_model, source_parent_model);

			// Combine parent model transform with local keyframe transform
			ozz::math::Transform source_model_keyframe;

			// Scale the translation by parent scale
			ozz::math::Float3 scaled_translation  = ozz::math::Float3(source_local_transform.translation.x * source_parent_model.scale.x,
																	  source_local_transform.translation.y * source_parent_model.scale.y,
																	  source_local_transform.translation.z * source_parent_model.scale.z);

			// Rotate the scaled translation by parent rotation
			ozz::math::Float3 rotated_translation = ozz::math::TransformVector(source_parent_model.rotation, scaled_translation);

			// Final model space transform
			source_model_keyframe.translation	  = source_parent_model.translation + rotated_translation;
			source_model_keyframe.rotation		  = source_parent_model.rotation * source_local_transform.rotation;
			source_model_keyframe.scale			  = ozz::math::Float3(source_parent_model.scale.x * source_local_transform.scale.x,
															  source_parent_model.scale.y * source_local_transform.scale.y,
															  source_parent_model.scale.z * source_local_transform.scale.z);

			// Apply retargeting in model space
			// Compute the difference from source bind pose
			ozz::math::Transform model_space_delta;

			// Translation delta
			model_space_delta.translation		  = source_model_keyframe.translation - source_bind_model.translation;

			// Rotation delta
			ozz::math::Quaternion source_bind_inv = ozz::math::Conjugate(source_bind_model.rotation);
			model_space_delta.rotation			  = source_bind_inv * source_model_keyframe.rotation;

			// Scale delta
			model_space_delta.scale = ozz::math::Float3(source_bind_model.scale.x != 0.0f ? source_model_keyframe.scale.x / source_bind_model.scale.x : 1.0f,
														source_bind_model.scale.y != 0.0f ? source_model_keyframe.scale.y / source_bind_model.scale.y : 1.0f,
														source_bind_model.scale.z != 0.0f ? source_model_keyframe.scale.z / source_bind_model.scale.z : 1.0f);

			// Apply delta to target bind pose
			ozz::math::Transform target_model_result;
			target_model_result.translation = target_bind_model.translation + model_space_delta.translation;
			target_model_result.rotation	= target_bind_model.rotation * model_space_delta.rotation;
			target_model_result.scale		= ozz::math::Float3(target_bind_model.scale.x * model_space_delta.scale.x,
															target_bind_model.scale.y * model_space_delta.scale.y,
															target_bind_model.scale.z * model_space_delta.scale.z);

			// Convert back to local space
			return ComputeLocalFromModel(target_model_result, target_parent_model);
		}

		// Helper function to find joint by name in skeleton
		const ozz::animation::offline::RawSkeleton::Joint *FindJointByName(const ozz::animation::offline::RawSkeleton &skeleton, const ozz::string &name)
		{
			return FindJointRecursive(skeleton.roots, name);
		}

		// Helper to traverse and collect joint names in order
		void CollectJointNames(const ozz::vector<ozz::animation::offline::RawSkeleton::Joint> &joints, ozz::vector<ozz::string> &jointNames)
		{
			for (const auto &joint : joints)
			{
				jointNames.push_back(GetMappedBoneName(joint.name));
				CollectJointNames(joint.children, jointNames);
			}
		}

		// Main retargeting function
		ozz::animation::offline::RawAnimation RetargetAnimation(const ozz::animation::offline::RawAnimation &source_animation,
																const ozz::animation::offline::RawSkeleton	&source_skeleton,
																const ozz::animation::offline::RawSkeleton	&target_skeleton)
		{
			// Build joint name lists for both skeletons
			ozz::vector<ozz::string> sourceJointNames;
			ozz::vector<ozz::string> targetJointNames;
			CollectJointNames(source_skeleton.roots, sourceJointNames);
			CollectJointNames(target_skeleton.roots, targetJointNames);

			// Build joint name to track index mapping for source animation
			std::unordered_map<ozz::string, int> sourceNameToTrack;
			for (int i = 0; i < sourceJointNames.size(); ++i)
			{
				sourceNameToTrack[sourceJointNames[i]] = i;
			}

			// Build retargeting mapping
			auto retargetInfo = BuildRetargetMap(source_skeleton, target_skeleton);

			// Create output animation
			ozz::animation::offline::RawAnimation targetAnimation;
			targetAnimation.duration = source_animation.duration;
			targetAnimation.name	 = source_animation.name + "_retargeted";

			// Resize tracks to match TARGET skeleton joint count
			targetAnimation.tracks.resize(targetJointNames.size());

			// Process each joint in the target skeleton
			for (size_t targetTrackIdx = 0; targetTrackIdx < targetJointNames.size(); ++targetTrackIdx)
			{
				const ozz::string &targetJointName = targetJointNames[targetTrackIdx];
				auto			  &targetTrack	   = targetAnimation.tracks[targetTrackIdx];

				// Find corresponding source track
				auto sourceTrackIt				   = sourceNameToTrack.find(targetJointName);
				if (sourceTrackIt == sourceNameToTrack.end())
				{
					// This joint doesn't exist in the animation - create identity keyframes
					
					ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> chain;
					
					ozz::animation::offline::RawAnimation::TranslationKey transKey;
					ozz::animation::offline::RawAnimation::RotationKey rotKey;
					ozz::animation::offline::RawAnimation::ScaleKey scaleKey;

					if (FindJointChainRecursive(target_skeleton.roots, targetJointName, chain))
					{
						transKey.value=chain.back()->transform.translation;
						rotKey.value=chain.back()->transform.rotation;
						scaleKey.value=chain.back()->transform.scale;
					}
					else
					{
						transKey.value = ozz::math::Float3(0.0f, 0.0f, 0.0f);
						rotKey.value = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
						scaleKey.value = ozz::math::Float3(1.0f, 1.0f, 1.0f);
					}
					transKey.time  = 0.0f;
					targetTrack.translations.push_back(transKey);

					rotKey.time	 = 0.0f;
					targetTrack.rotations.push_back(rotKey);

					scaleKey.time  = 0.0f;
					targetTrack.scales.push_back(scaleKey);

					continue;
				}

				int			sourceTrackIdx = sourceTrackIt->second;
				const auto &sourceTrack	   = source_animation.tracks[sourceTrackIdx];

				// Get retargeting info for this joint
				const auto &retarget	   = retargetInfo[sourceTrackIdx];

				if (!retarget.isValid)
				{
					// No valid retargeting info - use bind pose
					ozz::animation::offline::RawAnimation::TranslationKey transKey;
					transKey.time  = 0.0f;
					transKey.value = ozz::math::Float3(0.0f, 0.0f, 0.0f);
					targetTrack.translations.push_back(transKey);

					ozz::animation::offline::RawAnimation::RotationKey rotKey;
					rotKey.time	 = 0.0f;
					rotKey.value = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
					targetTrack.rotations.push_back(rotKey);

					ozz::animation::offline::RawAnimation::ScaleKey scaleKey;
					scaleKey.time  = 0.0f;
					scaleKey.value = ozz::math::Float3(1.0f, 1.0f, 1.0f);
					targetTrack.scales.push_back(scaleKey);

					continue;
				}

				// Retarget translation keyframes
				for (const auto &sourceKey : sourceTrack.translations)
				{
					ozz::animation::offline::RawAnimation::TranslationKey targetKey;
					targetKey.time = sourceKey.time;

					// Create transform from translation keyframe
					ozz::math::Transform sourceTransform;
					sourceTransform.translation				 = sourceKey.value;
					sourceTransform.rotation				 = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
					sourceTransform.scale					 = ozz::math::Float3(1.0f, 1.0f, 1.0f);

					// Apply model space retargeting
					ozz::math::Transform retargetedTransform = ApplyRetargetingModelSpace(
						sourceTransform, targetJointName, source_skeleton, target_skeleton, retarget.sourceParentModel, retarget.targetParentModel);

					targetKey.value = retargetedTransform.translation;
					targetTrack.translations.push_back(targetKey);
				}

				// Handle empty tracks - add at least one keyframe
				if (targetTrack.translations.empty())
				{
					ozz::animation::offline::RawAnimation::TranslationKey key;
					key.time  = 0.0f;
					key.value = ozz::math::Float3(0.0f, 0.0f, 0.0f);
					targetTrack.translations.push_back(key);
				}

				// Retarget rotation keyframes
				for (const auto &sourceKey : sourceTrack.rotations)
				{
					ozz::animation::offline::RawAnimation::RotationKey targetKey;
					targetKey.time = sourceKey.time;

					// Create transform from rotation keyframe
					ozz::math::Transform sourceTransform;
					sourceTransform.translation				 = ozz::math::Float3(0.0f, 0.0f, 0.0f);
					sourceTransform.rotation				 = sourceKey.value;
					sourceTransform.scale					 = ozz::math::Float3(1.0f, 1.0f, 1.0f);

					// Apply model space retargeting
					ozz::math::Transform retargetedTransform = ApplyRetargetingModelSpace(
						sourceTransform, targetJointName, source_skeleton, target_skeleton, retarget.sourceParentModel, retarget.targetParentModel);

					targetKey.value = retargetedTransform.rotation;
					targetTrack.rotations.push_back(targetKey);
				}

				if (targetTrack.rotations.empty())
				{
					ozz::animation::offline::RawAnimation::RotationKey key;
					key.time  = 0.0f;
					key.value = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
					targetTrack.rotations.push_back(key);
				}

				// Retarget scale keyframes
				for (const auto &sourceKey : sourceTrack.scales)
				{
					ozz::animation::offline::RawAnimation::ScaleKey targetKey;
					targetKey.time = sourceKey.time;

					// Create transform from scale keyframe
					ozz::math::Transform sourceTransform;
					sourceTransform.translation				 = ozz::math::Float3(0.0f, 0.0f, 0.0f);
					sourceTransform.rotation				 = ozz::math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
					sourceTransform.scale					 = sourceKey.value;

					// Apply model space retargeting
					ozz::math::Transform retargetedTransform = ApplyRetargetingModelSpace(
						sourceTransform, targetJointName, source_skeleton, target_skeleton, retarget.sourceParentModel, retarget.targetParentModel);

					targetKey.value = retargetedTransform.scale;
					targetTrack.scales.push_back(targetKey);
				}

				if (targetTrack.scales.empty())
				{
					ozz::animation::offline::RawAnimation::ScaleKey key;
					key.time  = 0.0f;
					key.value = ozz::math::Float3(1.0f, 1.0f, 1.0f);
					targetTrack.scales.push_back(key);
				}
			}

			return targetAnimation;
		}

	}
}