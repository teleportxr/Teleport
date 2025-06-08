#include "Skeleton.h"
#include "GeometryCache.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
// Using Ozz animation
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

using namespace teleport;
using namespace clientrender;

static std::vector<std::string> name_order={
		"hips",
		"spine",
		"chest",
		"spine1",
		"upperchest",
		"spine2",
		"neck",
		"head",
		"leftshoulder",
		"leftupperarm",
		"leftuparm",
		"leftlowerarm",
		"leftforearm",
		"lefthand",
		"rightshoulder",
		"rightupperarm",
		"rightuparm",
		"rightlowerarm",
		"rightforearm",
		"righthand",
		"leftupperleg",
		"leftupleg",
		"leftlowerleg",
		"leftleg",
		"leftfoot",
		"lefttoes",
		"rightupperleg",
		"rightupleg",
		"rightlowerleg",
		"rightleg",
		"rightfoot",
		"righttoes"};

Skeleton::Skeleton(avs::uid u, const std::string &name) : name(name)
{
	id = u;
}

Skeleton::Skeleton(avs::uid u, const std::string &name, size_t numBones) : name(name)
{
	id = u;
}

void RecurseChildrenIntoOzz(GeometryCache &g, ozz::animation::offline::RawSkeleton::Joint &root, Node &n, const std::vector<avs::uid> &boneIds,std::vector<int> &jointMapping, int &count, int level=0)
{
	if(std::find(boneIds.begin(),boneIds.end(),n.id)!=boneIds.end())
	{
		root.name					=n.name;
		root.transform.translation	= *((ozz::math::Float3*)&n.GetLocalPosition());
		root.transform.rotation		= *((ozz::math::Quaternion*)&n.GetLocalRotation());
		root.transform.scale		= *((ozz::math::Float3*)&n.GetLocalScale());
		std::cout<<std::string(level, '\t')<<n.id<<" "<<n.name<<"\n";
		int idx=(int)(std::find(boneIds.begin(),boneIds.end(),n.id)-boneIds.begin());
		if(idx<0||idx>=jointMapping.size())
		{
			TELEPORT_WARN("Failed to create joint mapping for skeleton.");
		}
		else
			jointMapping[idx]=count++;
	}
	std::vector<avs::uid> sorted_children;
	
	for (auto &child : n.GetChildren())
	{
		sorted_children.push_back(child.lock()->id);
	}
	std::sort(sorted_children.begin(),sorted_children.end(),[&g](avs::uid a,avs::uid b)
	{
		std::string nameA=g.mNodeManager.GetNode(a)->name;
		std::string nameB=g.mNodeManager.GetNode(a)->name;
		std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
		std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
		int A=(int)(std::ranges::find_if(name_order.begin(),name_order.end(),[&nameA](const std::string &str){return nameA.find(str) != std::string::npos;})-name_order.begin());
		int B=(int)(std::ranges::find_if(name_order.begin(),name_order.end(),[&nameB](const std::string &str){return nameB.find(str) != std::string::npos;})-name_order.begin());
		return A<B;
	});
	for (avs::uid u:sorted_children)
	{
		if (auto c=g.mNodeManager.GetNode(u))
		{
			if(std::find(boneIds.begin(),boneIds.end(),c->id)!=boneIds.end())
			{
				// only bones in the list...
				root.children.push_back(ozz::animation::offline::RawSkeleton::Joint());
				ozz::animation::offline::RawSkeleton::Joint &childJoint=root.children.back();
				childJoint.name					 = c->name;
				childJoint.transform.translation = ozz::math::Float3(0.f, 0.f, 0.f);
				childJoint.transform.rotation	 = ozz::math::Quaternion(0.f, 0.f, 0.f, 1.f);
				childJoint.transform.scale		 = ozz::math::Float3(1.f, 1.f, 1.f);
				RecurseChildrenIntoOzz(g,childJoint, *c, boneIds, jointMapping, count, level+1);
			}
			else
			{
				RecurseChildrenIntoOzz(g,root, *c, boneIds, jointMapping, count, level);
			}
		}
	}
}

void Skeleton::InitBones(GeometryCache &g)
{
	root=g.mNodeManager.GetNode(rootId);
	auto r=root.lock();
	if(!r)
		return;
	bones.clear();
	for (auto id : boneIds)
	{
		bones.push_back(g.mNodeManager.GetNode(id));
	}
	if(!bones.size())
		return;
	//////////////////////////////////////////////////////////////////////////////
	// The first section builds a RawSkeleton from custom data.
	//////////////////////////////////////////////////////////////////////////////

	// Creates a RawSkeleton.
	raw_skeleton=ozz::make_unique<ozz::animation::offline::RawSkeleton>();
	// Creates the root joint.
	raw_skeleton->roots.resize(1);
	ozz::animation::offline::RawSkeleton::Joint &ozz_root = raw_skeleton->roots[0];
	std::cout<<"================ SKELETON "<<name<<"\n";
	int count=0;
	jointMapping.clear();
	jointMapping.resize(boneIds.size());
	RecurseChildrenIntoOzz(g, ozz_root, *r, boneIds, jointMapping, count);

	if(raw_skeleton->num_joints()!=bones.size())
	{
		TELEPORT_WARN("Skeleton mismatch in {} {}: {} != {}",this->id,this->name,raw_skeleton->num_joints(),boneIds.size());
	}
	// Setup root joints name.

	// Setup root joints rest pose transformation, in joint local-space.
	// This is the default skeleton posture (most of the time a T-pose). It's
	// used as a fallback when there's no animation for a joint.
	//...and so on with the whole skeleton hierarchy...

	// Test for skeleton validity.
	// The main invalidity reason is the number of joints, which must be lower
	// than ozz::animation::Skeleton::kMaxJoints.
	if (!raw_skeleton->Validate())
	{
		return ;
	}

	//////////////////////////////////////////////////////////////////////////////
	// This final section converts the RawSkeleton to a runtime Skeleton.
	//////////////////////////////////////////////////////////////////////////////

	// Creates a SkeletonBuilder instance.
	ozz::animation::offline::SkeletonBuilder builder;

	// Executes the builder on the previously prepared RawSkeleton, which returns
	// a new runtime skeleton instance.
	// This operation will fail and return an empty unique_ptr if the RawSkeleton
	// isn't valid.
	skeleton = builder(*raw_skeleton);

	// ...use the skeleton as you want...
}

void Skeleton::GetBoneMatrices(std::shared_ptr<GeometryCache> geometryCache,const std::vector<mat4> &inverseBindMatrices, const std::vector<int16_t> &jointIndices, std::vector<mat4> &boneMatrices) const
{
	static bool force_identity = false;
	if(force_identity)
	{
		boneMatrices.resize(Skeleton::MAX_BONES);
		for (size_t i = 0; i < Skeleton::MAX_BONES; i++)
		{
			boneMatrices[i] = mat4::identity();
		}
		return;
	} 
// external skeleton?
	const auto &bones = GetExternalBones();
	if (bones.size() > 0)
	{
		size_t upperBound = std::min(inverseBindMatrices.size(),std::min(bones.size(),std::min<size_t>(jointIndices.size(), Skeleton::MAX_BONES)));
		boneMatrices.resize(upperBound);
		for (size_t i = 0; i < upperBound; i++)
		{
			TELEPORT_ASSERT(i<jointIndices.size());
			TELEPORT_ASSERT(jointIndices[i] < bones.size());
			const auto &node = bones[jointIndices[i]];
			if(!node)
				return;
			const mat4 &joint_matrix = node->GetGlobalTransform().GetTransformMatrix();
			const mat4 &inverse_bind_matrix = inverseBindMatrices[i];
			mat4 bone_matrix = joint_matrix * inverse_bind_matrix;
			boneMatrices[i] = bone_matrix;
		}
		return;
	}
}