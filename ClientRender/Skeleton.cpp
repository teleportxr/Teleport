#include "Skeleton.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "GeometryCache.h"

using namespace teleport;
using namespace clientrender;

Skeleton::Skeleton(avs::uid u, const std::string &name) : name(name)
{
	id=u;
}

Skeleton::Skeleton(avs::uid u, const std::string &name, size_t numBones, const Transform &skeletonTransform)
			: name(name),  skeletonTransform(skeletonTransform)
{
	id = u;
}

void Skeleton::InitBones(GeometryCache &g)
{
	bones.clear();
	for(auto id:boneIds)
	{
		bones.push_back(g.mNodeManager.GetNode(id));
	}
}