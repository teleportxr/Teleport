// (C) Copyright 2018-2020 Simul Software Ltd

#pragma once

#include <stdint.h>
#include <vector>
#include <libavstream/common.hpp>

struct ControllerState
{
	uint32_t mButtons;
	bool  mTrackpadStatus;
	float mTrackpadX;
	float mTrackpadY;
	float mJoystickAxisX;
	float mJoystickAxisY;
	std::vector<avs::InputEvent> inputEvents;
};
