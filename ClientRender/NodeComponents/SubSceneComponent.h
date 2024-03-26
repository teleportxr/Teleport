#pragma once

#include "Common.h"
#include "NodeComponents/Component.h"
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
			avs::uid sub_scene_uid = 0;
		};
	}
}