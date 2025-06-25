#include "NodeComponents/SubSceneComponent.h"
#include "GeometryCache.h"
using namespace teleport;
using namespace clientrender;

void SubSceneComponent::PlayAnimation(avs::uid cache_uid, avs::uid anim_uid)
{
	if (!mesh || !mesh->GetMeshCreateInfo().subscene_cache_uid || !anim_uid)
		return;
	auto		cache = GeometryCache::GetGeometryCache(mesh->GetMeshCreateInfo().subscene_cache_uid);
	const auto &sk_ids = cache->mSkeletonManager.GetAllIDs();
	std::set<avs::uid> root_uids;
	for (auto sk_id : sk_ids)
	{
		auto sk=cache->mSkeletonManager.Get(sk_id);
		if(!sk)
			continue;
		avs::uid root_uid = sk->GetRootId();
		root_uids.insert(root_uid);
	}
	for(auto root_uid:root_uids)
	{
		auto node = cache->mNodeManager.GetNode(root_uid);
		if(node)
		{
			auto animC = node->GetOrCreateComponent<AnimationComponent>();
			if(animC)
			{
				animC->PlayAnimation(cache_uid,anim_uid,owner.id,0, 1.0f);
			}
		}
	}
}