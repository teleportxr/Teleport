#pragma once
#include <libavstream/common.hpp>
#include "basic_linear_algebra.h"
#include <TeleportCore/Input.h>

#include "Platform/Shaders/SL/CppSl.sl"

namespace teleport
{
	namespace client
	{
		struct LocalGlobalPose
		{
			avs::Pose localPose;
			avs::Pose globalPose;
		};
		//! The generic state of the client hardware device e.g. headset, controllers etc.
		//! There exists one of these for each server, plus one for the null server (local state).
		class ClientServerState
		{
		public:
			// TODO: these are never deleted, only added.
			static ClientServerState &GetClientServerState(avs::uid u);
			ClientServerState();
			avs::uid origin_node_uid;
			LocalGlobalPose headPose;
			teleport::core::Input input;
			void TransformPose(LocalGlobalPose &p);
			void SetHeadPose_StageSpace(vec3 pos,clientrender::quat q);
			void SetInputs(const teleport::core::Input& st);

			//! From the stored relative poses, update the global ones to correspond.
			void UpdateGlobalPoses();
		};
	}
}