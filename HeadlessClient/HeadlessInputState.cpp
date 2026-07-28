#include "HeadlessInputState.h"
#include <cmath>

HeadlessInputState::HeadlessInputState()
{
	headPose.position = {0.0f, 0.0f, 0.0f};
	headPose.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
}

HeadlessInputState::Snapshot HeadlessInputState::GetSnapshot()
{
	std::lock_guard<std::mutex> lock(stateMutex);

	Snapshot snapshot;
	snapshot.displayInfo.width = 1;
	snapshot.displayInfo.height = 1;
	snapshot.headPose = headPose;
	snapshot.controllerPoses = controllerPoses;
	snapshot.originValidCounter = originValidCounter;
	snapshot.input = input;
	snapshot.time = currentTime;

	input.clearEvents();
	return snapshot;
}

void HeadlessInputState::SetPose(float x, float y, float z, float qx, float qy, float qz, float qw)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	headPose.position = {x, y, z};
	headPose.orientation = {qx, qy, qz, qw};
}

void HeadlessInputState::SetOrientation(float qx, float qy, float qz, float qw)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	headPose.orientation = {qx, qy, qz, qw};
}

void HeadlessInputState::AddBinaryEvent(teleport::core::InputId id, bool value)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	input.addBinaryEvent(id, value);
}

void HeadlessInputState::AddAnalogueEvent(teleport::core::InputId id, float value)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	input.addAnalogueEvent(id, value);
}

void HeadlessInputState::AddMotionEvent(teleport::core::InputId id, float x, float y)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	input.addMotionEvent(id, {x, y});
}

void HeadlessInputState::SetOriginValidCounter(uint64_t counter)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	originValidCounter = counter;
}

void HeadlessInputState::UpdateTime(double newTime)
{
	std::lock_guard<std::mutex> lock(stateMutex);
	currentTime = newTime;
}
