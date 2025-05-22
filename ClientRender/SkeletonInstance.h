#pragma once

#include "Skeleton.h"

#include "client/Shaders/pbr_constants.sl"

namespace ozz
{
	namespace animation
	{
		class Animation;
	}
}
namespace teleport
{
	namespace clientrender
	{
		class GeometryCache;
		//! An instance referencing a Skeleton, containing live data animating the skeleton in the app.
		class SkeletonInstance1
		{
		public:
			SkeletonInstance1( std::shared_ptr<Skeleton> s);
			virtual ~SkeletonInstance1();
			std::shared_ptr<Skeleton> GetSkeleton()
			{
				return skeleton;
			}
		protected:
			std::shared_ptr<Skeleton> skeleton;
		};
	}
}