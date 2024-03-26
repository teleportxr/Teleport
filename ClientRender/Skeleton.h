#pragma once

#include <memory>

#include "Transform.h"
#include "Resource.h"

namespace teleport
{
	namespace clientrender
	{
		class Node;
		class GeometryCache;
		//! A resource containing bones to animate a mesh.
		//! A Skeleton is immutable. Specific real-time transformation is done in SkeletonRender.
		class Skeleton:public Resource
		{
		public:
			static constexpr size_t MAX_BONES = 64;

			const std::string name;

			Skeleton(avs::uid u,const std::string &name);
			Skeleton(avs::uid u, const std::string &name, size_t numBones, const Transform &skeletonTransform);
			std::string getName() const
			{
				return name;
			}
			static const char *getTypeName()
			{
				return "Skeleton";
			}

			virtual ~Skeleton() = default;

			const Transform &GetSkeletonTransform() { return skeletonTransform; }

			void SetExternalBoneIds(const std::vector<avs::uid> &ids)
			{
				boneIds = ids;
			}
			const std::vector<avs::uid> &GetExternalBoneIds() const
			{
				return boneIds;
			}
			
			void InitBones(clientrender::GeometryCache &g);
			const std::vector<std::shared_ptr<clientrender::Node>> &GetExternalBones()
			{
				return bones;
			}
		protected:
			std::vector<avs::uid> boneIds;
			std::vector<std::shared_ptr<clientrender::Node>> bones;
			// TODO: do we need this?
			Transform skeletonTransform; // Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.
		};
	}

}