//
// Created by Roderick on 05/05/2020.
//

#include "Controllers.h"

#include <TeleportClient/Log.h>

Controllers::Controllers()
{
	mControllerIDs[0]=mControllerIDs[1]=0;
}

Controllers::~Controllers()
{

}

void Controllers::SetCycleShaderModeDelegate(TriggerDelegate delegate)
{
	CycleShaderMode = delegate;
}

void Controllers::SetCycleOSDDelegate(TriggerDelegate delegate)
{
	CycleOSD = delegate;
}
void Controllers::SetCycleOSDSelectionDelegate(TriggerDelegate delegate)
{
	CycleOSDSelection = delegate;
}

void Controllers::SetToggleMenuDelegate(TriggerDelegate delegate)
{
	ToggleMenu = delegate;
}

void Controllers::SetDebugOutputDelegate(TriggerDelegate delegate)
{
	WriteDebugOutput = delegate;
}

void Controllers::SetToggleWebcamDelegate(TriggerDelegate delegate)
{
	ToggleWebcam = delegate;
}

void Controllers::SetSetStickOffsetDelegate(Float2Delegate delegate)
{
	SetStickOffset = delegate;
}

void Controllers::ClearDelegates()
{
	CycleShaderMode = nullptr;
	CycleOSD = nullptr;
	CycleOSDSelection= nullptr;
	SetStickOffset = nullptr;
	WriteDebugOutput=nullptr;
	ToggleWebcam = nullptr;
}

bool Controllers::InitializeController(ovrMobile *ovrmobile,int idx)
{
	ovrInputCapabilityHeader inputCapsHeader;
	ovrControllerCapabilities caps;
	if(idx==1)
	{
		caps=ovrControllerCapabilities::ovrControllerCaps_RightHand;
	}
	if(idx==0)
	{
		caps=ovrControllerCapabilities::ovrControllerCaps_LeftHand;
	}

	for(uint32_t i = 0;i<2; ++i)
	{
		if(vrapi_EnumerateInputDevices(ovrmobile, i, &inputCapsHeader) >= 0)
		if(inputCapsHeader.Type == ovrControllerType_TrackedRemote&&(int)inputCapsHeader.DeviceID != -1)
		{
			ovrInputTrackedRemoteCapabilities trackedInputCaps;
			trackedInputCaps.Header = inputCapsHeader;
			vrapi_GetInputDeviceCapabilities(ovrmobile, &trackedInputCaps.Header);
			if((trackedInputCaps.ControllerCapabilities&caps)!=caps)
				continue;
			TELEPORT_CLIENT_LOG("Found controller (ID: %d)", inputCapsHeader.DeviceID);
			TELEPORT_CLIENT_LOG("Controller Capabilities: %ud", trackedInputCaps.ControllerCapabilities);
			TELEPORT_CLIENT_LOG("Button Capabilities: %ud", trackedInputCaps.ButtonCapabilities);
			TELEPORT_CLIENT_LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
			TELEPORT_CLIENT_LOG("Trackpad range: %ud, %ud", trackedInputCaps.TrackpadMaxX, trackedInputCaps.TrackpadMaxX);
			mTrackpadDim.x = trackedInputCaps.TrackpadMaxX;
			mTrackpadDim.y = trackedInputCaps.TrackpadMaxY;
			mControllerIDs[idx] = inputCapsHeader.DeviceID;
		}
	}

	return (mControllerIDs[idx]>0);
}

void Controllers::Update(ovrMobile *ovrmobile)
{
	// Query controller input state.
	for(int i = 0; i < 2; i++)
	{
		if(mControllerIDs[i] != 0)
		{
#if 0
            teleport::core::Input input = {};
			ovrInputStateTrackedRemote ovrState;
			ovrState.Header.ControllerType = ovrControllerType_TrackedRemote;
			if(vrapi_GetCurrentInputState(ovrmobile, mControllerIDs[i], &ovrState.Header) >= 0)
			{
				if(ovrState.TrackpadStatus)
				{
					float dx = ovrState.TrackpadPosition.x / mTrackpadDim.x - 0.5f;
					float dy = ovrState.TrackpadPosition.y / mTrackpadDim.y - 0.5f;
					SetStickOffset(dx, dy);
				}
				if(lastControllerState.triggerBack != ovrState.IndexTrigger)
				{
					controllerState.addAnalogueEvent(avs::InputId::TRIGGER_BACK, ovrState.IndexTrigger);
				}
				//controllerState.triggerBack = ovrState.IndexTrigger;

				if(lastControllerState.triggerGrip != ovrState.GripTrigger)
				{
					controllerState.addAnalogueEvent(avs::InputId::TRIGGER_GRIP, ovrState.GripTrigger);
				}
				//controllerState.triggerGrip = ovrState.GripTrigger;

				if((controllerState.mReleased & ovrButton::ovrButton_Enter) != 0)
				{
					ToggleMenu();
				}
				if((controllerState.mReleased & ovrButton::ovrButton_A) != 0)
				{
					CycleOSD();
				}
				if((controllerState.mReleased & ovrButton::ovrButton_B) != 0)
				{
					CycleOSDSelection();
				}
				if((controllerState.mReleased & ovrButton::ovrButton_X) != 0)
				{
					WriteDebugOutput();

					// All buttons seem to be taken up so putting it here for now
					ToggleWebcam();
				}
				if( (controllerState.mReleased & ovrButton::ovrButton_Y) != 0)
				{
					CycleShaderMode();
				}
			}
#endif
		}
	}
}

