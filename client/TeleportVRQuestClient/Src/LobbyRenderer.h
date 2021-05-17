#pragma once
#include <GUI/GuiSys.h>
#include "ClientAppInterface.h"

class ClientDeviceState;
class LobbyRenderer
{
public:
	LobbyRenderer(ClientDeviceState *s,ClientAppInterface *c);
	void Render();

protected:
	ClientDeviceState *clientDeviceState=nullptr;
	ClientAppInterface *clientAppInterface=nullptr;
};
