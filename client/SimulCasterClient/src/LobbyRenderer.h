#pragma once
#include <GuiSys.h>
class ClientDeviceState;
class LobbyRenderer
{
public:
	LobbyRenderer(ClientDeviceState *s);
	void Render(OVR::OvrGuiSys *mGuiSys);

protected:
	ClientDeviceState *clientDeviceState=nullptr;
};
