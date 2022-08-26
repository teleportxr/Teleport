#include "SkinInstance.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

using namespace clientrender;


SkinInstance::SkinInstance(std::shared_ptr<Skin> s)
	:  skin(s)
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
				TELEPORT_CERR<<"Error building skin instance"<<std::endl;
			}
		}
		bone->SetLocalTransform(b->GetLocalTransform());
		bones.push_back(bone);
	}
	const auto &orig_joints=s->GetJoints();
	for(const auto &j:orig_joints)
	{
		std::shared_ptr<clientrender::Bone> bone = boneMap[j->id];
		joints.push_back(bone);
	}
}

void SkinInstance::UpdateBoneMatrices(const mat4& rootTransform)
{
	//MAX_BONES may be less than the amount of bones we have.
	const auto &inverseBindMatrices	=skin->GetInverseBindMatrices();
	size_t upperBound = std::min<size_t>(joints.size(), Skin::MAX_BONES);
	//const Transform &skinTransform=skin->GetSkinTransform();
	//mat4 common = skinTransform.GetTransformMatrix();
	// Each bone will have a transform that's the product of its global transform and its "inverse bind matrix".
	// The IBM transforms from object space to bone space. The bone transforms from bone space to object space.
	// So in the neutral position these matrices precisely cancel.
	// The IBMs are properties of the skin->, there's one for each bone, and they are unchanging.
	// The bone matrices are continually updated.
	for (size_t i = 0; i < upperBound; i++)
	{
		mat4 g = joints[i]->GetGlobalTransform().GetTransformMatrix();
		mat4 ib = inverseBindMatrices[i];
		mat4 b = g * ib;
		boneMatrices[i] = b;
	}
}