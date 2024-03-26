#pragma once

#include <map>
#include <memory>
#include <vector>
#include "NodeComponents/Component.h"
#include "Common.h"

namespace clientrender
{
	class SubSceneComponent:public Component
	{
	public:
		virtual ~SubSceneComponent(){}
		avs::uid sub_scene_uid=0;
	};
}