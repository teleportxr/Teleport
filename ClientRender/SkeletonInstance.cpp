#include "SkeletonInstance.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

using namespace clientrender;


SkeletonInstance::SkeletonInstance(std::shared_ptr<Skeleton> s)
	:  skeleton(s)
{
	const auto &orig_bones=s->GetBones();
	for(const auto &b:orig_bones)
	{
		std::shared_ptr<clientrender::Bone> bone = std::make_shared<clientrender::Bone>(b->id,b->name+"_instance");
		boneMap[b->id]=bone;
		std::shared_ptr<clientrender::Bone> parent;
		std::shared_ptr<clientrender::Bone> p = b->GetParent();
		if(p)
		{
			parent=boneMap[p->id];
		
			if(parent)
			{
				bone->SetParent(parent);
				parent->AddChild(bone);
			}
			else
			{
				TELEPORT_CERR<<"Error building skeleton instance"<<std::endl;
			}
		}
		bone->SetLocalTransform(b->GetLocalTransform());
		bones.push_back(bone);
	}
}

void SkeletonInstance::GetBoneMatrices(const std::vector<mat4> &inverseBindMatrices,const std::vector<int16_t> &jointIndices,std::vector<mat4> &boneMatrices)
{
	//MAX_BONES may be fewer than the amount of bones we have.
	//const auto &inverseBindMatrices	=skeleton->GetInverseBindMatrices();
	size_t upperBound = std::min<size_t>(bones.size(), Skeleton::MAX_BONES);
	upperBound = std::min<size_t>(upperBound, inverseBindMatrices.size());
	if(inverseBindMatrices.size()!=jointIndices.size())
		return;
	//const Transform &skeletonTransform=skeleton->GetSkeletonTransform();
	//mat4 common = skeletonTransform.GetTransformMatrix();
	// Each bone will have a transform that's the product of its global transform and its "inverse bind matrix".
	// The IBM transforms from object space to bone space. The bone transforms from bone space to object space.
	// So in the neutral position these matrices precisely cancel.
	// The IBMs are properties of the skeleton->, there's one for each bone, and they are unchanging.
	// The bone matrices are continually updated.
	boneMatrices.resize(upperBound);
	for (size_t i = 0; i < upperBound; i++)
	{
		mat4 joint_matrix = bones[jointIndices[i]]->GetGlobalTransform().GetTransformMatrix();
		mat4 inverse_bind_matrix = inverseBindMatrices[i];
		mat4 bone_matrix = joint_matrix * inverse_bind_matrix;
		boneMatrices[i] = bone_matrix;
	}
}