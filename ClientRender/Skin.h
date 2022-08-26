#pragma once

#include <memory>

#include "Bone.h"
#include "Transform.h"

namespace clientrender
{
	//! A resource containing bones and joints to animate a mesh.
	class Skin
	{
	public:
		static constexpr size_t MAX_BONES = 64;

		const std::string name;

		Skin(const std::string& name);
		Skin(const std::string& name, const std::vector<mat4>& inverseBindMatrices, size_t numBones, const Transform& skinTransform);

		virtual ~Skin() = default;

		void SetInverseBindMatrices(const std::vector<mat4>& vector) { inverseBindMatrices = vector; }
		const std::vector<mat4>& GetInverseBindMatrices() const { return inverseBindMatrices; }

		void SetBones(const std::vector<std::shared_ptr<Bone>>& vector) { bones = vector; }
		const std::vector<std::shared_ptr<Bone>>& GetBones() const { return bones; }
		std::shared_ptr<Bone> GetBoneByName(const char *txt);

		void SetNumBones(size_t numBones) { bones.resize(numBones); }
		void SetBone(size_t index, std::shared_ptr<Bone> bone);
		void SetJoints(const std::vector<std::shared_ptr<Bone>>& j);
		const std::vector<std::shared_ptr<Bone>>& GetJoints() const { return joints; }
		const Transform& GetSkinTransform() { return skinTransform; }
	protected:
		std::vector<mat4> inverseBindMatrices;
		std::vector<std::shared_ptr<Bone>> bones;
		std::vector<std::shared_ptr<Bone>> joints;
		// TODO: do we need this?
		Transform skinTransform; //Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.

	};
}
