// (C) Copyright 2018 Simul.co

#pragma once

#include <stdint.h>

struct ControllerState {
    uint32_t mButtons;
    bool  mTrackpadStatus;
    float mTrackpadX;
    float mTrackpadY;
};
