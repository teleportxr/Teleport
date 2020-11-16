#include "Skin.h"

namespace scr
{
void Skin::UpdateBoneMatrices(const mat4& rootTransform)
{
	for(size_t i = 0; i < bones.size(); i++)
	{
		boneMatrices[i] = (rootTransform.GetInverted() * skinTransform.GetTransformMatrix() * bones[i]->GetGlobalTransform().GetTransformMatrix() * inverseBindMatrices[i].GetTransformMatrix());
	}
}
}
