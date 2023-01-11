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
		class ClientDeviceState
		{
			std::map<avs::uid,LocalGlobalPose> nodePoses;
		public:
			ClientDeviceState();
			//! Clear the stored data, e.g. node poses.
			void Clear();
			LocalGlobalPose headPose;
			avs::Pose originPose;					// in game absolute space.
			teleport::core::Input input;
			void SetLocalNodePose(avs::uid,const avs::Pose &localPose);
			const avs::Pose &GetGlobalNodePose(avs::uid) const;
			void TransformPose(LocalGlobalPose &p);
			void SetHeadPose_StageSpace(avs::vec3 pos,clientrender::quat q);
			void SetInputs(const teleport::core::Input& st);

			//! From the stored relative poses, update the global ones to correspond.
			void UpdateGlobalPoses();
		};
	}
}