#include "SkeletonInstance.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "GeometryCache.h"

using namespace teleport;
using namespace clientrender;


SkeletonInstance::SkeletonInstance(std::shared_ptr<Skeleton> s)
	:  skeleton(s)
{
}

SkeletonInstance::~SkeletonInstance()
{
}

void SkeletonInstance::GetBoneMatrices(std::shared_ptr<GeometryCache> geometryCache,const std::vector<mat4> &inverseBindMatrices, const std::vector<int16_t> &jointIndices, std::vector<mat4> &boneMatrices)
{
	static bool force_identity = false;
	if(force_identity)
	{
		boneMatrices.resize(Skeleton::MAX_BONES);
		for (size_t i = 0; i < Skeleton::MAX_BONES; i++)
		{
			boneMatrices[i] = mat4::identity();
		}
		return;
	} 
// external skeleton?
	const auto &bone_ids = skeleton->GetExternalBoneIds();
	if(bone_ids.size()>0)
	{
		size_t upperBound = std::min<size_t>(jointIndices.size(), Skeleton::MAX_BONES);
		boneMatrices.resize(upperBound);
		for (size_t i = 0; i < upperBound; i++)
		{
			TELEPORT_ASSERT(i<jointIndices.size());
			TELEPORT_ASSERT(jointIndices[i] < bone_ids.size());
			avs::uid bone_node_uid = bone_ids[jointIndices[i]];
			auto node=geometryCache->mNodeManager.GetNode(bone_node_uid);
			if(!node)
				return;
			mat4 joint_matrix = node->GetGlobalTransform().GetTransformMatrix();
			mat4 inverse_bind_matrix = inverseBindMatrices[i];
			mat4 bone_matrix = joint_matrix * inverse_bind_matrix;
			boneMatrices[i] = bone_matrix;
		}
		return;
	}
}