#pragma once

#include <memory>

#include "Bone.h"
#include "Transform.h"

namespace clientrender
{
	class Skin
	{
	public:
		static constexpr size_t MAX_BONES = 64;

		const std::string name;

		Skin(const std::string& name);
		Skin(const std::string& name, const std::vector<mat4>& inverseBindMatrices, size_t numBones, const Transform& skinTransform);

		virtual ~Skin() = default;

		virtual void UpdateBoneMatrices(const mat4& rootTransform);

		mat4* GetBoneMatrices(const mat4& rootTransform)
		{
			UpdateBoneMatrices(rootTransform);
			return boneMatrices;
		}

		void SetInverseBindMatrices(const std::vector<mat4>& vector) { inverseBindMatrices = vector; }
		const std::vector<mat4>& GetInverseBindMatrices() { return inverseBindMatrices; }

		void SetBones(const std::vector<std::shared_ptr<Bone>>& vector) { bones = vector; }
		const std::vector<std::shared_ptr<Bone>>& GetBones() { return bones; }

		void SetNumBones(size_t numBones) { bones.resize(numBones); }
		void SetBone(size_t index, std::shared_ptr<Bone> bone);
		void SetJoints(const std::vector<std::shared_ptr<Bone>>& j);
		const std::vector<std::shared_ptr<Bone>>& GetJoints() { return joints; }
		void SetSkinTransform(const Transform& value) { skinTransform = value; }
		const Transform& GetSkinTransform() { return skinTransform; }
	protected:
		//Internal function for returning the bone matrices without updating them.
		mat4* GetBoneMatrices()
		{
			return boneMatrices;
		}
		std::vector<clientrender::mat4> inverseBindMatrices;
		std::vector<std::shared_ptr<Bone>> bones;
		std::vector<std::shared_ptr<Bone>> joints;
		Transform skinTransform; //Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.

		mat4 boneMatrices[MAX_BONES];
	};
}
