#include "LobbyRenderer.h"
#include "ClientDeviceState.h"
#include <VrApi_Types.h>

LobbyRenderer::LobbyRenderer(teleport::client::ClientDeviceState *s,ClientAppInterface *c)
:clientDeviceState(s),clientAppInterface (c)
{
}

void LobbyRenderer::Render(const char *server_ip,int server_port)
{
	static avs::vec3 offset={0,0,3.5f};
	static avs::vec4 colour={1.0f,0.7f,0.1f,0.5f};
	clientAppInterface->PrintText(offset,colour,"Discovering: server %s, port %d",server_ip?server_ip:"",server_port);
}
