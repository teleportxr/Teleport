//
// Created by Roderick on 05/05/2020.
//

#include <VrApi_Input.h>
#include <crossplatform/Input.h>
#include "Controllers.h"
#include <crossplatform/Log.h>

Controllers::Controllers()
{
	mControllerIDs[0]=mControllerIDs[1]=0;
}

Controllers::~Controllers()
{

}

void Controllers::SetCycleShaderModeDelegate(TriggerDelegate d)
{
	CycleShaderMode=d;
}

void Controllers::SetToggleShowInfoDelegate(TriggerDelegate d)
{
	ToggleShowInfo=d;
}

void Controllers::SetSetStickOffsetDelegate(Float2Delegate d)
{
	SetStickOffset=d;
}

void Controllers::ClearDelegates()
{
	CycleShaderMode=nullptr;
	ToggleShowInfo=nullptr;
	SetStickOffset=nullptr;
}

bool Controllers::InitializeController(ovrMobile *ovrmobile)
{
	ovrInputCapabilityHeader inputCapsHeader;
	int idx=0;
	for(uint32_t i = 0;
	    vrapi_EnumerateInputDevices(ovrmobile, i, &inputCapsHeader) == 0; ++i) {
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote)
		{
			if ((int) inputCapsHeader.DeviceID != -1)
			{
				mControllerIDs[idx] = inputCapsHeader.DeviceID;
				LOG("Found controller (ID: %d)", mControllerIDs[idx]);
				idx++;
				ovrInputTrackedRemoteCapabilities trackedInputCaps;
				trackedInputCaps.Header = inputCapsHeader;
				vrapi_GetInputDeviceCapabilities(ovrmobile, &trackedInputCaps.Header);
				LOG("Controller Capabilities: %ud", trackedInputCaps.ControllerCapabilities);
				LOG("Button Capabilities: %ud", trackedInputCaps.ButtonCapabilities);
				LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
				mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
			}
		}
	}

	return (idx>0);
}

static avs::uid eventId=0;
void Controllers::Update(ovrMobile *ovrmobile)
{
	// Query controller input state.
	ControllerState controllerState = {};
	for(int i=0;i<2;i++)
	if((int)mControllerIDs[i] != 0)
	{
		ControllerState &lastControllerState=mLastControllerStates[i];
		ovrInputStateTrackedRemote ovrState;
		ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
		if(vrapi_GetCurrentInputState(ovrmobile, mControllerIDs[i], &ovrState.Header) >= 0)
		{
			controllerState.mButtons = ovrState.Buttons;

			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
			controllerState.mJoystickAxisX=ovrState.Joystick.x;
			controllerState.mJoystickAxisY=ovrState.Joystick.y;

			if(controllerState.mTrackpadStatus)
			{
				float          dx = controllerState.mTrackpadX - 0.5f;
				float          dy = controllerState.mTrackpadY - 0.5f;
				SetStickOffset(dx,dy);
			}
			uint32_t clicked=(~((uint32_t)controllerState.mButtons))&((uint32_t)lastControllerState.mButtons);
			mLastControllerStates[i] = controllerState;

			//Flip rendering mode when the trigger is held, and the X or A button is released.
			if((clicked & ovrButton::ovrButton_Trigger) != 0 )
			{
				avs::InputEvent evt;
				evt.eventId=eventId++;		 //< A monotonically increasing event identifier.
				evt.inputUid=1;//i*32+ovrButton::ovrButton_Trigger;		 //< e.g. the uniqe identifier for this button or control.
				evt.intValue=0;
				mLastControllerStates[i].inputEvents.push_back(evt);
			}
			else if((clicked & ovrButton::ovrButton_A) != 0 || (clicked & ovrButton::ovrButton_X) != 0)
			{
				ToggleShowInfo();
			}
			else if((clicked& ovrButton::ovrButton_B) != 0 || (clicked & ovrButton::ovrButton_Y) != 0)
			{
				CycleShaderMode();
			}
		}
	}
}
