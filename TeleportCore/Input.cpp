// (C) Copyright 2018-2021 Simul Software Ltd
#include "Input.h"
#include "TeleportCore/ErrorHandling.h"
using namespace teleport;
using namespace core;
static uint32_t nextEventID = 0;


void Input::clearEvents()
{
	binaryEvents.clear();
	analogueEvents.clear();
	motionEvents.clear();
}

void Input::addBinaryEvent( avs::InputId inputID, bool activated)
{
	avs::InputEventBinary binaryEvent;
	binaryEvent.eventID = nextEventID++;
	binaryEvent.inputID = inputID;
	binaryEvent.activated = activated;

	binaryEvents.push_back(binaryEvent);
}

void Input::addAnalogueEvent( avs::InputId inputID, float strength)
{
//	TELEPORT_COUT << "Analogue: " << eventID << " " << (int)inputID << " " << strength << std::endl;
	avs::InputEventAnalogue analogueEvent;
	analogueEvent.eventID = nextEventID++;
	analogueEvent.inputID = inputID;
	analogueEvent.strength = strength;

	analogueEvents.push_back(analogueEvent);
}

void Input::addMotionEvent( avs::InputId inputID, avs::vec2 motion)
{
	avs::InputEventMotion motionEvent;
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
	if(f<-1.f||f>1.f||isnan(f))
	{
		TELEPORT_CERR<<"Bad analogue state value "<<f<<std::endl;
		strength=0;
	}
	if(index>=analogueStates.size())
		analogueStates.resize(index+1);
	analogueStates[index]=strength;
}
