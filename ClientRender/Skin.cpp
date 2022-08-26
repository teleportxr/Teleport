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

	std::shared_ptr<Bone> Skin::GetBoneByName(const char *txt)
	{
		for(auto b:bones)
		{
			if(b->name==txt)
				return b;
		}
		return nullptr;
	}

	void Skin::SetBone(size_t index, std::shared_ptr<Bone> bone)
	{
		if (index < bones.size())
		{
			bones[index] = bone;
		}
		else
		{
			TELEPORT_CERR << "ERROR: Attempted to add bone to skin (" << name << ") at index " << index << " greater than size " << bones.size() << "!\n";
		}
	}
	void Skin::SetJoints(const std::vector<std::shared_ptr<Bone>>& j)
	{
		joints = j;
	}
}