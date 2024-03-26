#pragma once

#include <memory>

#include "Bone.h"
#include "Transform.h"

namespace clientrender
{
	//! A resource containing bones and joints to animate a mesh. 
	//! A Skeleton is immutable. Specific real-time transformation is done in SkeletonInstance.
	class Skeleton
	{
	public:
		static constexpr size_t MAX_BONES = 64;

		const std::string name;

		Skeleton(const std::string& name);
		Skeleton(const std::string& name, size_t numBones, const Transform& skeletonTransform);

		virtual ~Skeleton() = default;

		void SetBones(const std::vector<std::shared_ptr<Bone>>& vector) { bones = vector; }
		const std::vector<std::shared_ptr<Bone>>& GetBones() const { return bones; }
		std::shared_ptr<Bone> GetBoneByName(const char *txt);

		void SetNumBones(size_t numBones) { bones.resize(numBones); }
		void SetBone(size_t index, std::shared_ptr<Bone> bone);
		const Transform& GetSkeletonTransform() { return skeletonTransform; }

		void SetExternalBoneIds(const std::vector<avs::uid> &ids)
		{
			boneIds=ids;
		}
		const std::vector<avs::uid> &GetExternalBoneIds() const
		{
			return boneIds;
		}
	protected:
		std::vector<std::shared_ptr<Bone>> bones;
		std::vector<avs::uid> boneIds;
		// TODO: do we need this?
		Transform skeletonTransform; //Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.

	};
}
