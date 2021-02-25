#pragma once
#include <GUI/GuiSys.h>
class ClientDeviceState;
class LobbyRenderer
{
public:
	LobbyRenderer(ClientDeviceState *s);
	void Render(OVRFW::OvrGuiSys *mGuiSys);

protected:
	ClientDeviceState *clientDeviceState=nullptr;
};
