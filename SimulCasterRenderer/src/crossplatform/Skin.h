#pragma once

#include <memory>
#include <vector>

#include "Bone.h"
#include "Transform.h"

namespace scr
{
class Skin
{
public:
	std::vector<Transform> inverseBindMatrices;
	std::vector<std::shared_ptr<Bone>> bones;
	Transform rootTransform;

	std::vector<mat4> GetBoneMatrices()
	{
		std::vector<mat4> boneMatrices(64);

		for(size_t i = 0; i < bones.size(); i++)
		{
			boneMatrices[i] = rootTransform.GetTransformMatrix() * bones[i]->GetGlobalTransform().GetTransformMatrix() * inverseBindMatrices[i].GetTransformMatrix();
		}

		return boneMatrices;
	}
};
}
