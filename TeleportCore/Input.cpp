// (C) Copyright 2018-2021 Simul Software Ltd
#include "Input.h"
#include "TeleportCore/ErrorHandling.h"
using namespace teleport;
using namespace core;
static uint32_t nextEventID = 0;
#ifdef _MSC_VER
#define isnanf isnan
#endif

void Input::clearEvents()
{
	binaryEvents.clear();
	analogueEvents.clear();
	motionEvents.clear();
}
const InputEventAnalogue& Input::getLastAnalogueEvent(uint16_t inputID) const
{
	if (inputID >= lastAnalogueEvents.size())
		lastAnalogueEvents.resize((size_t)inputID + 1);
	return lastAnalogueEvents[inputID];
}

const InputEventBinary& Input::getLastBinaryEvent(uint16_t inputID) const
{
	if (inputID >= lastBinaryEvents.size())
		lastBinaryEvents.resize((size_t)inputID + 1);
	return lastBinaryEvents[inputID];
}

void Input::addBinaryEvent( InputId inputID, bool activated)
{
	InputEventBinary binaryEvent;
	binaryEvent.eventID = nextEventID++;
	binaryEvent.inputID = inputID;
	binaryEvent.activated = activated;

	binaryEvents.push_back(binaryEvent);

	if(inputID>= lastBinaryEvents.size())
		lastBinaryEvents.resize((size_t)inputID+1);
	lastBinaryEvents[inputID]=binaryEvent;
}

void Input::addAnalogueEvent( InputId inputID, float strength)
{
//	TELEPORT_COUT << "Analogue: " << eventID << " " << (int)inputID << " " << strength << std::endl;
	InputEventAnalogue analogueEvent;
	analogueEvent.eventID = nextEventID++;
	analogueEvent.inputID = inputID;
	analogueEvent.strength = strength;

	analogueEvents.push_back(analogueEvent);
	if (inputID >= lastAnalogueEvents.size())
		lastAnalogueEvents.resize((size_t)inputID + 1);
	lastAnalogueEvents[inputID]=analogueEvent;
}

void Input::addMotionEvent( InputId inputID, vec2 motion)
{
	InputEventMotion motionEvent;
	motionEvent.eventID = nextEventID++;
	motionEvent.inputID = inputID;
	motionEvent.motion = motion;

	motionEvents.push_back(motionEvent);
}


void Input::setBinaryState(uint16_t index, bool activated)
{
	if(index>=binaryStates.size())
		binaryStates.resize(index+1);
	binaryStates[index]=activated;
}

void Input::setAnalogueState(uint16_t index, float strength)
{
	float &f=strength;
	if(f<-1.f||f>1.f||isnanf(f))
	{
		TELEPORT_CERR<<"Bad analogue state value "<<f<<std::endl;
		strength=0;
	}
	if(index>=analogueStates.size())
		analogueStates.resize(index+1);
	analogueStates[index]=strength;
}
