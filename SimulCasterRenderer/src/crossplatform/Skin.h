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

	const std::string name;

	Skin(const std::string& name);
	Skin(const std::string& name, const std::vector<Transform>& inverseBindMatrices, size_t boneAmount, const Transform& skinTransform);

	virtual ~Skin() = default;

	virtual void UpdateBoneMatrices(const mat4& rootTransform);

	mat4* GetBoneMatrices(const mat4& rootTransform)
	{
		UpdateBoneMatrices(rootTransform);
		return boneMatrices;
	}

	void SetInverseBindMatrices(const std::vector<Transform>& vector) { inverseBindMatrices = vector; }
	const std::vector<Transform>& GetInverseBindMatrices() { return inverseBindMatrices; }

	void SetBones(const std::vector<std::shared_ptr<Bone>>& vector) { bones = vector; }
	const std::vector<std::shared_ptr<Bone>>& GetBones() { return bones; }

	void SetBoneAmount(size_t boneAmount) { bones.resize(boneAmount); }
	void SetBone(size_t index, std::shared_ptr<Bone> bone)
	{
		if(index < bones.size())
		{
			bones[index] = bone;
		}
		else
		{
			SCR_COUT << "ERROR: Attempted to add bone to skin (" << name << ") at index " << index << " greater than size " << bones.size() << "!\n";
		}
	}

	void SetSkinTransform(const Transform& value) { skinTransform = value; }
	const Transform& GetSkinTransform() { return skinTransform; }
protected:
	//Internal function for returning the bone matrices without updating them.
	mat4* GetBoneMatrices()
	{
		return boneMatrices;
	}
private:
	std::vector<Transform> inverseBindMatrices;
	std::vector<std::shared_ptr<Bone>> bones;
	Transform skinTransform; //Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.

	mat4 boneMatrices[MAX_BONES];
};
}
