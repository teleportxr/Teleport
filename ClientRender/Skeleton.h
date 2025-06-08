#pragma once

#include <memory>

#include "Transform.h"
#include "Resource.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/offline/raw_skeleton.h"

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
			static constexpr size_t MAX_BONES = 128;

			const std::string name;

			Skeleton(avs::uid u,const std::string &name);
			Skeleton(avs::uid u, const std::string &name, size_t numBones);
			std::string getName() const
			{
				return name;
			}
			static const char *getTypeName()
			{
				return "Skeleton";
			}

			virtual ~Skeleton() = default;

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
			ozz::animation::offline::RawSkeleton *GetRawSkeleton()
			{
				return raw_skeleton.get();
			}
			const std::vector<int> &GetJointMapping() const
			{
				return jointMapping;
			}
		protected:
			avs::uid rootId=0;
			std::vector<avs::uid> boneIds;
			std::weak_ptr<clientrender::Node> root;
			std::vector<std::shared_ptr<clientrender::Node>> bones;
			std::vector<mat4> inverseBindMatrices;
			std::vector<int> jointMapping;
			ozz::unique_ptr<ozz::animation::Skeleton> skeleton;;
			ozz::unique_ptr<ozz::animation::offline::RawSkeleton> raw_skeleton;
		};
	}

}