// (C) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <stdint.h>

struct ControllerState {
    uint32_t mButtons;
    bool  mTrackpadStatus;
    float mTrackpadX;
    float mTrackpadY;
	float mJoystickX;
	float mJoystickY;
};
