// (C) Copyright 2018-2020 Simul Software Ltd

#pragma once

#include <map>
#include <stdint.h>
#include <vector>

#include "libavstream/common_input.h"

struct ControllerState
{
	uint32_t mButtons = 0;
	uint32_t mReleased = 0;
	bool  mTrackpadStatus = false;
	float mTrackpadX = 0.0f;
	float mTrackpadY = 0.0f;
	float mJoystickAxisX = 0.0f;
	float mJoystickAxisY = 0.0f;

	//We are using hard-set values as the Android compiler didn't like reading from referenced memory in a dictionary; every other frame it would evaluate to zero.
	float triggerBack = 0.0f;
	float triggerGrip = 0.0f;

	//These are split for simplicity, and we can't marshal polymorphic types to the managed C# code.
	std::vector<avs::InputEventBinary> binaryEvents;
	std::vector<avs::InputEventAnalogue> analogueEvents;
	std::vector<avs::InputEventMotion> motionEvents;

	void clear()
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

	void addBinaryEvent(uint32_t eventID, avs::InputList inputID, bool activated)
	{
		avs::InputEventBinary binaryEvent;
		binaryEvent.eventID = eventID;
		binaryEvent.inputID = inputID;
		binaryEvent.activated = activated;

		binaryEvents.push_back(binaryEvent);
	}

	void addAnalogueEvent(uint32_t eventID, avs::InputList inputID, float strength)
	{
		avs::InputEventAnalogue analogueEvent;
		analogueEvent.eventID = eventID;
		analogueEvent.inputID = inputID;
		analogueEvent.strength = strength;

		analogueEvents.push_back(analogueEvent);
	}

	void addMotionEvent(uint32_t eventID, avs::InputList inputID, avs::vec2 motion)
	{
		avs::InputEventMotion motionEvent;
		motionEvent.eventID = eventID;
		motionEvent.inputID = inputID;
		motionEvent.motion = motion;

		motionEvents.push_back(motionEvent);
	}
};
