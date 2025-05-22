#pragma once

#include <memory>

#include "Transform.h"
#include "Resource.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/animation/runtime/skeleton.h"

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
			void SetRootId(avs::uid r)
			{
				rootId=r;
			}
			avs::uid GetRootId() const
			{
				return rootId;
			}
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
			};
			const std::vector<std::shared_ptr<clientrender::Node>> &GetExternalBones() const
			{
				return bones;
			};
			const std::vector<mat4> &GetInverseBindMatrices() const
			{
				return inverseBindMatrices;
			}
			void SetInverseBindMatrices(const std::vector<mat4>& i)
			{
				inverseBindMatrices=i;
			}
			void GetBoneMatrices(std::shared_ptr<GeometryCache> geometryCache, const std::vector<mat4> &inverseBindMatrices, const std::vector<int16_t> &jointIndices, std::vector<mat4> &boneMatrices) const;
			ozz::animation::Skeleton *GetOzzSkeleton()
			{
				return skeleton.get();
			}
		protected:
			avs::uid rootId=0;
			std::vector<avs::uid> boneIds;
			std::weak_ptr<clientrender::Node> root;
			std::vector<std::shared_ptr<clientrender::Node>> bones;
			std::vector<mat4> inverseBindMatrices;
			ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
			// TODO: do we need this?
			Transform skeletonTransform; // Transform of the parent node of the bone hierarchy; i.e there may be multiple top-level bones, but their parent is not the root.
		};
	}

}