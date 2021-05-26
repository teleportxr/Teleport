#ifndef CLIENT_CONTROLLERS_H
#define CLIENT_CONTROLLERS_H

#include <functional>

#include <VrApi_Input.h>

#include <TeleportClient/Input.h>
#include "libavstream/common_input.h"

typedef std::function<void()> TriggerDelegate;
typedef std::function<void(float,float)> Float2Delegate;

class Controllers
{
public :
	Controllers();
	~Controllers();

	void SetCycleShaderModeDelegate(TriggerDelegate d);
	void SetCycleOSDDelegate(TriggerDelegate d);;
	void SetDebugOutputDelegate(TriggerDelegate d);
	void SetSetStickOffsetDelegate(Float2Delegate d);
	void ClearDelegates();

	bool InitializeController(ovrMobile *pMobile,int idx);
	void Update(ovrMobile *ovrmobile);
	ovrDeviceID mControllerIDs[2];

	ovrVector2f mTrackpadDim;
	ControllerState mLastControllerStates[2]; //State of the primary controller on the last frame.
private:
	void AddButtonPressEvent(uint32_t pressedButtons, uint32_t releasedButtons, ControllerState& controllerState, ovrButton buttonID, avs::InputList inputID);

	TriggerDelegate CycleShaderMode;
	TriggerDelegate CycleOSD;
	TriggerDelegate WriteDebugOutput;
	Float2Delegate SetStickOffset;

	uint32_t nextEventID = 0;
};


#endif //CLIENT_CONTROLLERS_H
