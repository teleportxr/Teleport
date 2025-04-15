#pragma once

#include "Common.h"
#include "NodeComponents/Component.h"
#include "ClientRender/Mesh.h"
#include <map>
#include <memory>
#include <vector>

namespace teleport
{
	namespace clientrender
	{
		class SubSceneComponent : public Component
		{
		public:
			virtual ~SubSceneComponent() {}
			avs::uid mesh_uid=0;
			//avs::uid sub_scene_cache_uid = 0xFFFFFFFFFFFFFFFF; The Mesh object stores the cache uid.
			std::shared_ptr<Mesh> mesh;
		};
	}
}