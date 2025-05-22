#include "SkeletonInstance.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "GeometryCache.h"

using namespace teleport;
using namespace clientrender;

#pragma optimize("",off)
SkeletonInstance1::SkeletonInstance1(std::shared_ptr<Skeleton> s)
	:  skeleton(s)
{
}

SkeletonInstance1::~SkeletonInstance1()
{
}
