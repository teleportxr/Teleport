#include "Skeleton.h"
#include "GeometryCache.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
// Using Ozz animation
#include "AnimationRetargeter.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

using namespace teleport;
using namespace clientrender;

static std::vector<std::string> name_order={
		"hips",
		"spine",
		"chest",
		"spine1",
		"upperchest",
		"spine2",
		"neck",
		"head",
		"leftshoulder",
		"leftupperarm",
		"leftuparm",
		"leftlowerarm",
		"leftforearm",
		"lefthand",
		"rightshoulder",
		"rightupperarm",
		"rightuparm",
		"rightlowerarm",
		"rightforearm",
		"righthand",
		"leftupperleg",
		"leftupleg",
		"leftlowerleg",
		"leftleg",
		"leftfoot",
		"lefttoes",
		"rightupperleg",
		"rightupleg",
		"rightlowerleg",
		"rightleg",
		"rightfoot",
		"righttoes"};

Skeleton::Skeleton(avs::uid u, const std::string &name) : name(name)
{
	id = u;
}

Skeleton::Skeleton(avs::uid u, const std::string &name, size_t numBones) : name(name)
{
	id = u;
}

void GetRelativeTransform(ozz::math::Transform &relTransform, const ozz::math::Transform &parentTransform, const ozz::math::Transform &childTransform)
{
	{
		// Convert back to local space
		ozz::math::Float3	  target_to_parent	  = childTransform.translation - parentTransform.translation;
		ozz::math::Quaternion parent_inv_rotation = ozz::math::Conjugate(parentTransform.rotation);
		ozz::math::Float3	  local_offset		  = ozz::math::TransformVector(parent_inv_rotation, target_to_parent);

		relTransform.translation = ozz::math::Float3(parentTransform.scale.x != 0.0f ? local_offset.x / parentTransform.scale.x : local_offset.x,
													 parentTransform.scale.y != 0.0f ? local_offset.y / parentTransform.scale.y : local_offset.y,
													 parentTransform.scale.z != 0.0f ? local_offset.z / parentTransform.scale.z : local_offset.z);
	}

	// Rotation retargeting using model space orientations
	{
		// Convert back to local space
		ozz::math::Quaternion target_parent_inv = ozz::math::Conjugate(parentTransform.rotation);
		relTransform.rotation					= target_parent_inv * childTransform.rotation;
	}

	// Scale retargeting
	{
		// Compute scale ratio from bind pose
		ozz::math::Float3 scaleRatio = ozz::math::Float3(childTransform.scale.x != 0.0f ? childTransform.scale.x / parentTransform.scale.x : 1.0f,
														 childTransform.scale.y != 0.0f ? childTransform.scale.y / parentTransform.scale.y : 1.0f,
														 childTransform.scale.z != 0.0f ? childTransform.scale.z / parentTransform.scale.z : 1.0f);

		// Apply ratio to target bind scale
		relTransform.scale = scaleRatio;
	}
}

void RecurseChildrenIntoOzz(ozz::animation::offline::RawSkeleton		&raw_skeleton,
							GeometryCache								&g,
							ozz::animation::offline::RawSkeleton::Joint &root,
							const Node									&n,
							const std::vector<avs::uid>					&boneIds,
							std::vector<int>							&jointMapping,
							int											&count,
							ozz::vector<ozz::math::Transform>			&modelspace_transforms,
							int											 level		  = 0,
							bool										 gap		  = false,
							int											 parent_index = 0)
{
	int idx_in_bones = (int)(std::find(boneIds.begin(), boneIds.end(), n.id) - boneIds.begin());
	root.name	 = getMappedBoneName(n.name);
	
	if(count>=modelspace_transforms.size())
		modelspace_transforms.resize(count+1);
	modelspace_transforms[count]	=	ComputeModelSpaceTransform(raw_skeleton, root.name);

	if (gap)
	{
		const ozz::math::Transform &parentTransform = modelspace_transforms[parent_index];
		const ozz::math::Transform &childTransform	= modelspace_transforms[count];
		GetRelativeTransform(root.transform, parentTransform, childTransform);
	}
	else
	{
		root.transform.translation = *((ozz::math::Float3 *)&n.GetLocalPosition());
		root.transform.rotation	   = *((ozz::math::Quaternion *)&n.GetLocalRotation());
		root.transform.scale	   = *((ozz::math::Float3 *)&n.GetLocalScale());
	}
	if (idx_in_bones >= 0 && idx_in_bones < jointMapping.size())
	{
		jointMapping[idx_in_bones] = count;
		// std::cout<<"MAPPED: " << n.name <<" "<< n.id <<", index "<<idx_in_bones<< " to anim's " <<count-1<<", "<<root.name << std::endl;
	}
	
	std::vector<avs::uid> sorted_children;

	for (auto &child : n.GetChildren())
	{
		sorted_children.push_back(child.lock()->id);
	}
	std::ranges::sort(sorted_children,
					  [&g](avs::uid a, avs::uid b)
					  {
						  std::string nameA = g.mNodeManager.GetNode(a)->name;
						  std::string nameB = g.mNodeManager.GetNode(a)->name;
						  std::ranges::transform(nameA, nameA.begin(), ::tolower);
						  std::ranges::transform(nameB, nameB.begin(), ::tolower);
						  int A = (int)(std::ranges::find_if(name_order, [&nameA](const std::string &str) { return nameA.find(str) != std::string::npos; }) -
										name_order.begin());
						  int B = (int)(std::ranges::find_if(name_order, [&nameB](const std::string &str) { return nameB.find(str) != std::string::npos; }) -
										name_order.begin());
						  return A < B;
					  });
	for (avs::uid u : sorted_children)
	{
		int new_parent_index = count;
		if (auto c = g.mNodeManager.GetNode(u))
		{
			if (std::find(boneIds.begin(), boneIds.end(), c->id) != boneIds.end())
			{
				// only bones in the list...
				root.children.emplace_back();
				ozz::animation::offline::RawSkeleton::Joint &childJoint = root.children.back();
				childJoint.name											= c->name;
				childJoint.transform.translation						= ozz::math::Float3(0.f, 0.f, 0.f);
				childJoint.transform.rotation							= ozz::math::Quaternion(0.f, 0.f, 0.f, 1.f);
				childJoint.transform.scale								= ozz::math::Float3(1.f, 1.f, 1.f);
				count++;
				RecurseChildrenIntoOzz(raw_skeleton, g, childJoint, *c, boneIds, jointMapping, count, modelspace_transforms, level + 1, false, new_parent_index);
			}
			else
			{
				// If the child isn't in the skeleton, use the same parent as before, and recurse, in case it has children in the skeleton.
				// But don't add
				count++;
				RecurseChildrenIntoOzz(raw_skeleton, g, root, *c, boneIds, jointMapping, count, modelspace_transforms, level, true, new_parent_index);
			}
		}
	}
}

void Skeleton::InitBones(GeometryCache &g)
{
	root   = g.mNodeManager.GetNode(rootId);
	auto r = root.lock();
	if (!r)
	{
		return;
	}
	bones.clear();
	for (auto id : boneIds)
	{
		bones.push_back(g.mNodeManager.GetNode(id));
	}
	if (!bones.size())
	{
		return;
	}
	//////////////////////////////////////////////////////////////////////////////
	// The first section builds a RawSkeleton from custom data.
	//////////////////////////////////////////////////////////////////////////////

	// Creates a RawSkeleton.
	raw_skeleton = ozz::make_unique<ozz::animation::offline::RawSkeleton>();
	// Creates the root joint.
	raw_skeleton->roots.resize(1);
	ozz::animation::offline::RawSkeleton::Joint						&ozz_root = raw_skeleton->roots[0];

	ozz::vector<ozz::math::Transform>								 modelspace_transforms;
	ozz::vector<const ozz::animation::offline::RawSkeleton::Joint *> joint_list;
	//auto															 source_joint_map = BuildJointMap(*raw_skeleton.get(), modelspace_transforms, joint_list);
	int																 count			  = 0;
	jointMapping.clear();
	jointMapping.resize(boneIds.size());
	RecurseChildrenIntoOzz(*raw_skeleton.get(), g, ozz_root, *r, boneIds, jointMapping, count, modelspace_transforms);

	if (raw_skeleton->num_joints() != bones.size())
	{
		TELEPORT_WARN("Skeleton mismatch in {} {}: {} != {}", this->id, this->name, raw_skeleton->num_joints(), boneIds.size());
	}
	// Setup root joints name.

	// Setup root joints rest pose transformation, in joint local-space.
	// This is the default skeleton posture (most of the time a T-pose). It's
	// used as a fallback when there's no animation for a joint.
	//...and so on with the whole skeleton hierarchy...

	// Test for skeleton validity.
	// The main invalidity reason is the number of joints, which must be lower
	// than ozz::animation::Skeleton::kMaxJoints.
	if (!raw_skeleton->Validate())
	{
		return;
	}

	//////////////////////////////////////////////////////////////////////////////
	// This final section converts the RawSkeleton to a runtime Skeleton.
	//////////////////////////////////////////////////////////////////////////////

	// Creates a SkeletonBuilder instance.
	ozz::animation::offline::SkeletonBuilder builder;

	// Executes the builder on the previously prepared RawSkeleton, which returns
	// a new runtime skeleton instance.
	// This operation will fail and return an empty unique_ptr if the RawSkeleton
	// isn't valid.
	skeleton = builder(*raw_skeleton);

	// ...use the skeleton as you want...
}

void Skeleton::GetBoneMatrices(std::shared_ptr<GeometryCache> geometryCache,
							   const std::vector<mat4>		 &inverseBindMatrices,
							   const std::vector<int16_t>	 &jointIndices,
							   std::vector<mat4>			 &boneMatrices) const
{
	static bool force_identity = false;
	if (force_identity)
	{
		boneMatrices.resize(Skeleton::MAX_BONES);
		for (size_t i = 0; i < Skeleton::MAX_BONES; i++)
		{
			boneMatrices[i] = mat4::identity();
		}
		return;
	}
	// external skeleton?
	const auto &bones = GetExternalBones();
	if (bones.size() > 0)
	{
		size_t upperBound = std::min(inverseBindMatrices.size(), std::min(bones.size(), std::min<size_t>(jointIndices.size(), Skeleton::MAX_BONES)));
		boneMatrices.resize(upperBound);
		for (size_t i = 0; i < upperBound; i++)
		{
			TELEPORT_ASSERT(i < jointIndices.size());
			TELEPORT_ASSERT(jointIndices[i] < bones.size());
			const auto &node = bones[jointIndices[i]];
			if (!node)
			{
				return;
			}
			const mat4 &joint_matrix		= node->GetGlobalTransform().GetTransformMatrix();
			const mat4 &inverse_bind_matrix = inverseBindMatrices[i];
			mat4		bone_matrix			= joint_matrix * inverse_bind_matrix;
			boneMatrices[i]					= bone_matrix;
		}
		return;
	}
}