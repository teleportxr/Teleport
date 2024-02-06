#pragma once

#include "Skeleton.h"

#include "client/Shaders/pbr_constants.sl"

namespace teleport
{
	namespace clientrender
	{
		class GeometryCache;
		//! An instance referencing a Skeleton, containing live data animating the skeleton in the app.
		class SkeletonInstance
		{
		public:
			SkeletonInstance( std::shared_ptr<Skeleton> s);
			virtual ~SkeletonInstance();
			void GetBoneMatrices(std::shared_ptr<GeometryCache> geometryCache, const std::vector<mat4> &inverseBindMatrices, const std::vector<int16_t> &jointIndices, std::vector<mat4> &boneMatrices);
			std::shared_ptr<Skeleton> GetSkeleton()
			{
				return skeleton;
			}

		protected:
			std::shared_ptr<Skeleton> skeleton;
		};
	}
}