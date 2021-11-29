// (C) Copyright 2018-2021 Simul Software Ltd

#pragma once

#include "Input.h"
#include "TeleportCore/ErrorHandling.h"
using namespace teleport;
using namespace client;

void ControllerState::clear()
{
	mButtons = 0;
	mTrackpadStatus = false;
	mTrackpadX = 0.0f;
	mTrackpadY = 0.0f;
	mJoystickAxisX = 0.0f;
	mJoystickAxisY = 0.0f;
	triggerBack = 0.0f;
	triggerGrip = 0.0f;

	binaryEvents.clear();
	analogueEvents.clear();
	motionEvents.clear();
}

void ControllerState::addBinaryEvent(uint32_t eventID, avs::InputId inputID, bool activated)
{
	avs::InputEventBinary binaryEvent;
	binaryEvent.eventID = eventID;
	binaryEvent.inputID = inputID;
	binaryEvent.activated = activated;

	binaryEvents.push_back(binaryEvent);
}

void ControllerState::addAnalogueEvent(uint32_t eventID, avs::InputId inputID, float strength)
{
//	TELEPORT_COUT << "Analogue: " << eventID << " " << (int)inputID << " " << strength << std::endl;
	avs::InputEventAnalogue analogueEvent;
	analogueEvent.eventID = eventID;
	analogueEvent.inputID = inputID;
	analogueEvent.strength = strength;

	analogueEvents.push_back(analogueEvent);
}

void ControllerState::addMotionEvent(uint32_t eventID, avs::InputId inputID, avs::vec2 motion)
{
	avs::InputEventMotion motionEvent;
	motionEvent.eventID = eventID;
	motionEvent.inputID = inputID;
	motionEvent.motion = motion;

	motionEvents.push_back(motionEvent);
}
