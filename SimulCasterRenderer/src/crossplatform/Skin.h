#pragma once

#include <memory>

#include "Bone.h"
#include "Transform.h"

namespace scr
{
class Skin
{
public:
	static constexpr size_t MAX_BONES = 64;

	std::vector<Transform> inverseBindMatrices;
	std::vector<std::shared_ptr<Bone>> bones;
	Transform skinTransform; //Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.

	virtual ~Skin() = default;

	virtual void UpdateBoneMatrices(const mat4& rootTransform);

	mat4* GetBoneMatrices(const mat4& rootTransform)
	{
		UpdateBoneMatrices(rootTransform);
		return boneMatrices;
	}
protected:
	//Internal function for returning the bone matrices without updating them.
	mat4* GetBoneMatrices()
	{
		return boneMatrices;
	}
private:
	mat4 boneMatrices[MAX_BONES];
};
}
