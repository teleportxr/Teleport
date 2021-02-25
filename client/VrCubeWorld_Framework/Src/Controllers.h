#ifndef CLIENT_CONTROLLERS_H
#define CLIENT_CONTROLLERS_H

#include <functional>

typedef std::function<void()> TriggerDelegate;
typedef std::function<void(float,float)> Float2Delegate;

class Controllers
{
public :
	Controllers();
	~Controllers();

	void SetToggleTexturesDelegate(TriggerDelegate d);
	void SetToggleShowInfoDelegate(TriggerDelegate d);
	void SetSetStickOffsetDelegate(Float2Delegate d);
	void ClearDelegates();

	bool InitializeController(ovrMobile *pMobile);
	void Update(ovrMobile *ovrmobile);
	ovrDeviceID mControllerIDs[2];

	ovrVector2f mTrackpadDim;
	ControllerState mLastControllerStates[2]; //State of the primary controller on the last frame.
private:
	TriggerDelegate ToggleTextures;
	TriggerDelegate ToggleShowInfo;
	Float2Delegate SetStickOffset;
};


#endif //CLIENT_CONTROLLERS_H
