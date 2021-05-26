#pragma once
#include <GUI/GuiSys.h>
#include "ClientAppInterface.h"
namespace teleport
{
	namespace client
	{
		class ClientDeviceState;
	}
}
class LobbyRenderer
{
public:
	LobbyRenderer(teleport::client::ClientDeviceState *s,ClientAppInterface *c);
	void Render(const char *server_ip= nullptr,int server_port=0);

protected:
	teleport::client::ClientDeviceState *clientDeviceState=nullptr;
	ClientAppInterface *clientAppInterface=nullptr;
};
