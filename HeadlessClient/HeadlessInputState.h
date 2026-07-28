#pragma once

#include <mutex>
#include <map>
#include "TeleportCore/Input.h"
#include "TeleportClient/basic_linear_algebra.h"

class HeadlessInputState
{
public:
	HeadlessInputState();

	struct Snapshot
	{
		avs::DisplayInfo displayInfo;
		teleport::core::Pose headPose;
		std::map<avs::uid, teleport::core::PoseDynamic> controllerPoses;
		uint64_t originValidCounter = 0;
		teleport::core::Input input;
		double time = 0.0;
	};

	Snapshot GetSnapshot();

	void SetPose(float x, float y, float z, float qx, float qy, float qz, float qw);
	void SetOrientation(float qx, float qy, float qz, float qw);

	void AddBinaryEvent(teleport::core::InputId id, bool value);
	void AddAnalogueEvent(teleport::core::InputId id, float value);
	void AddMotionEvent(teleport::core::InputId id, float x, float y);

	void SetOriginValidCounter(uint64_t counter);
	void UpdateTime(double newTime);

private:
	mutable std::mutex stateMutex;
	teleport::core::Pose headPose;
	std::map<avs::uid, teleport::core::PoseDynamic> controllerPoses;
	uint64_t originValidCounter = 0;
	teleport::core::Input input;
	double currentTime = 0.0;
};
