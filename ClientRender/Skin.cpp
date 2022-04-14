#include "Skin.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

namespace clientrender
{
	Skin::Skin(const std::string& name)
		:name(name)
	{}

	Skin::Skin(const std::string& name, const std::vector<mat4>& ibm, size_t numBones, const Transform& skinTransform)
		: name(name), inverseBindMatrices(ibm), bones(numBones), skinTransform(skinTransform)
	{}

	void Skin::UpdateBoneMatrices(const mat4& rootTransform)
	{
		//MAX_BONES may be less than the amount of bones we have.
		size_t upperBound = std::min<size_t>(joints.size(), MAX_BONES);

		mat4 common = skinTransform.GetTransformMatrix();
		// Each bone will have a transform that's the product of its global transform and its "inverse bind matrix".
		// The IBM transforms from object space to bone space. The bone transforms from bone space to object space.
		// So in the neutral position these matrices precisely cancel.
		// The IBMs are properties of the skin, there's one for each bone, and they are unchanging.
		// The bone matrices are continually updated.
		for (size_t i = 0; i < upperBound; i++)
		{
			mat4 g = joints[i]->GetGlobalTransform().GetTransformMatrix();
			mat4 ib = inverseBindMatrices[i];
			mat4 b = g * ib;
			boneMatrices[i] = b;
		}
	}

	void Skin::SetBone(size_t index, std::shared_ptr<Bone> bone)
	{
		if (index < bones.size())
		{
			bones[index] = bone;
		}
		else
		{
			TELEPORT_COUT << "ERROR: Attempted to add bone to skin (" << name << ") at index " << index << " greater than size " << bones.size() << "!\n";
		}
	}
	void Skin::SetJoints(const std::vector<std::shared_ptr<Bone>>& j)
	{
		joints = j;
	}
}