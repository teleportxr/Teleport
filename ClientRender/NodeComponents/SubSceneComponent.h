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
			SubSceneComponent(Node &n) : Component(n)
			{
			}
			virtual ~SubSceneComponent() {}

			//! Shortcut function: find a matching animationcomponent 
			void PlayAnimation(avs::uid cache_uid, avs::uid anim_uid);
			avs::uid mesh_uid=0;
			std::shared_ptr<Mesh> mesh;

		};
	}
}