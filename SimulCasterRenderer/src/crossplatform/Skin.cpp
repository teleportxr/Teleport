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
	for(size_t i = 0; i < bones.size(); i++)
	{
		boneMatrices[i] = (rootTransform.GetInverted() * skinTransform.GetTransformMatrix() * bones[i]->GetGlobalTransform().GetTransformMatrix() * inverseBindMatrices[i].GetTransformMatrix());
	}
}

