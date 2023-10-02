#pragma once
#include <libavstream/common.hpp>
#include "basic_linear_algebra.h"
#include <TeleportCore/Input.h>

#include "Platform/CrossPlatform/Shaders/CppSl.sl"

namespace teleport
{
	namespace client
	{
		struct LocalGlobalPose
		{
			avs::Pose localPose;
		};
		//! The generic state of the client hardware device e.g. headset, controllers etc.
		//! There exists one of these for each server, plus one for the null server (local state).
		struct ClientServerState
		{
		public:
			avs::uid origin_node_uid;
			LocalGlobalPose headPose;
			teleport::core::Input input;
			void SetHeadPose_StageSpace(vec3 pos,clientrender::quat q);
			void SetInputs(const teleport::core::Input& st);
		};
	}
}