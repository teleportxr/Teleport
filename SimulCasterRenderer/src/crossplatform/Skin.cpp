#include "Skin.h"

namespace scr
{
	Skin::Skin(const std::string& name)
		:name(name)
	{}

	Skin::Skin(const std::string& name, const std::vector<mat4>& ibm, size_t boneAmount, const Transform& skinTransform)
		: name(name), inverseBindMatrices(ibm), bones(boneAmount), skinTransform(skinTransform)
	{}

	void Skin::UpdateBoneMatrices(const mat4& rootTransform)
	{
		//MAX_BONES may be less than the amount of bones we have.
		size_t upperBound = std::min<size_t>(bones.size(), MAX_BONES);
		//mat4 rir=rootTransform.GetInverted() * rootTransform;
		mat4 common=skinTransform.GetTransformMatrix();
		for (size_t i = 0; i < upperBound; i++)
		{
			mat4 g=bones[i]->GetGlobalTransform().GetTransformMatrix();
			mat4 ib=inverseBindMatrices[i];
			boneMatrices[i] = common * g* ib;
		}
	}
}

