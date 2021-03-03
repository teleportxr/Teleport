#include "Skin.h"

using namespace scr;

Skin::Skin(const std::string& name)
	:name(name)
{}

Skin::Skin(const std::string& name, const std::vector<Transform>& inverseBindMatrices, size_t boneAmount, const Transform& skinTransform)
	:name(name), inverseBindMatrices(inverseBindMatrices), bones(boneAmount), skinTransform(skinTransform)
{}

void Skin::UpdateBoneMatrices(const mat4& rootTransform)
{
	//MAX_BONES may be less than the amount of bones we have.
	size_t upperBound = std::min(bones.size(), MAX_BONES);
	for(size_t i = 0; i < upperBound; i++)
	{
		boneMatrices[i] = (rootTransform.GetInverted() * skinTransform.GetTransformMatrix() * bones[i]->GetGlobalTransform().GetTransformMatrix() * inverseBindMatrices[i].GetTransformMatrix());
	}
}

