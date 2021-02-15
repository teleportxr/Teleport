// (C) Copyright 2018-2020 Simul Software Ltd

#pragma once

#include <stdint.h>
#include <vector>
#include <libavstream/common.hpp>

struct ControllerState
{
	uint32_t mButtons=0;
	bool  mTrackpadStatus=false;
	float mTrackpadX=0.0f;
	float mTrackpadY=0.0f;
	float mJoystickAxisX=0.0f;
	float mJoystickAxisY=0.0f;
	std::vector<avs::InputEvent> inputEvents;
};
