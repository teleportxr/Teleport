#pragma once

#include "Skeleton.h"

namespace clientrender
{
	//! An instance referencing a Skeleton, containing live data animating the skeleton in the app.
	class SkeletonInstance
	{
	public:
		SkeletonInstance( std::shared_ptr<Skeleton> s);
		virtual ~SkeletonInstance() = default;
		void GetBoneMatrices(const std::vector<mat4> &inverseBindMatrices,const std::vector<int16_t> &jointIndices,std::vector<mat4> &boneMatrices);
		std::shared_ptr<Skeleton> GetSkeleton()
		{
			return skeleton;
		}
		const std::vector<std::shared_ptr<Bone>>& GetBones() const
		{
			return bones;
		}
	protected:
		std::shared_ptr<Skeleton> skeleton;
		// TODO: this is a very crude repro of the mBoneManager locally,
		// containing only copies of the bones/joints that the original Skeleton has.
		// This must be made MUCH more efficient.
		std::unordered_map<avs::uid,std::shared_ptr<Bone>> boneMap;
		std::vector<std::shared_ptr<Bone>> bones;
		mat4 boneMatrices[Skeleton::MAX_BONES];
	};
}
