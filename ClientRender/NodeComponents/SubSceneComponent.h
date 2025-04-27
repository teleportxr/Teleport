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
			std::shared_ptr<Mesh> mesh;
		};
	}
}