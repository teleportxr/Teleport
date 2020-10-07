//
// Created by Roderick on 05/05/2020.
//

#include <VrApi_Input.h>
#include <crossplatform/Input.h>
#include "Controllers.h"
#include "Log.h"

Controllers::Controllers():
		mControllerID(0)
{

}
Controllers::~Controllers()
{

}


void Controllers::SetToggleTexturesDelegate(TriggerDelegate d)
{
	ToggleTextures=d;
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
	ToggleTextures=nullptr;
	ToggleShowInfo=nullptr;
	SetStickOffset=nullptr;
}

bool Controllers::InitializeController(ovrMobile *ovrmobile)
{
	ovrInputCapabilityHeader inputCapsHeader;
	for(uint32_t i = 0;
	    vrapi_EnumerateInputDevices(ovrmobile, i, &inputCapsHeader) == 0; ++i) {
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote)
		{
			if ((int) inputCapsHeader.DeviceID != -1)
			{
				mControllerID = inputCapsHeader.DeviceID;
				LOG("Found controller (ID: %d)", mControllerID);

				ovrInputTrackedRemoteCapabilities trackedInputCaps;
				trackedInputCaps.Header = inputCapsHeader;
				vrapi_GetInputDeviceCapabilities(ovrmobile, &trackedInputCaps.Header);
				LOG("Controller Capabilities: %ud", trackedInputCaps.ControllerCapabilities);
				LOG("Button Capabilities: %ud", trackedInputCaps.ButtonCapabilities);
				LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
				mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
				mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
				return true;
			}
		}
	}

	return false;
}

void Controllers::Update(ovrMobile *ovrmobile)
{
	// Query controller input state.
	ControllerState controllerState = {};
	if((int)mControllerID != 0)
	{
		ovrInputStateTrackedRemote ovrState;
		ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
		if(vrapi_GetCurrentInputState(ovrmobile, mControllerID, &ovrState.Header) >= 0)
		{
			controllerState.mButtons = ovrState.Buttons;

			//Flip show debug information, if the grip trigger was released.
			if((mLastPrimaryControllerState.mButtons & ovrButton::ovrButton_GripTrigger) != 0 && (controllerState.mButtons & ovrButton::ovrButton_GripTrigger) == 0)
			{
				ToggleShowInfo();
			}

			//Flip rendering mode when the trigger is held, and the X or A button is released.
			if((mLastPrimaryControllerState.mButtons & ovrButton::ovrButton_Trigger) != 0 &&
			   (((mLastPrimaryControllerState.mButtons & ovrButton::ovrButton_X) != 0 && (controllerState.mButtons & ovrButton::ovrButton_X) == 0) ||
			    ((mLastPrimaryControllerState.mButtons & ovrButton::ovrButton_A) != 0 && (controllerState.mButtons & ovrButton::ovrButton_A) == 0)))
			{
				ToggleTextures();
			}

			controllerState.mTrackpadStatus = ovrState.TrackpadStatus > 0;
			controllerState.mTrackpadX = ovrState.TrackpadPosition.x / mTrackpadDim.x;
			controllerState.mTrackpadY = ovrState.TrackpadPosition.y / mTrackpadDim.y;
			controllerState.mJoystickAxisX=ovrState.Joystick.x;
			controllerState.mJoystickAxisY=ovrState.Joystick.y * -1;

			if(controllerState.mTrackpadStatus)
			{
				float          dx = controllerState.mTrackpadX - 0.5f;
				float          dy = controllerState.mTrackpadY - 0.5f;
				SetStickOffset(dx,dy);
			}
		}
	}
	mLastPrimaryControllerState = controllerState;
}
