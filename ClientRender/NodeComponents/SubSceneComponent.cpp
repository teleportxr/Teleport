#include "NodeComponents/SubSceneComponent.h"
#include "GeometryCache.h"
using namespace teleport;
using namespace clientrender;

void SubSceneComponent::PlayAnimation(avs::uid cache_uid, avs::uid anim_uid)
{
	if (!mesh||!mesh->GetMeshCreateInfo().subscene_cache_uid)
		return;
	auto		cache = GeometryCache::GetGeometryCache(mesh->GetMeshCreateInfo().subscene_cache_uid);
	const auto &nodes = cache->mNodeManager.GetRootNodes();
	for (auto n : nodes)
	{
		auto node  = n.lock();
		auto animC = node->GetComponent<AnimationComponent>();
		if(animC)
		{
			animC->PlayAnimation(cache_uid,anim_uid);
		}

	}
}