#include "LobbyRenderer.h"
#include "ClientDeviceState.h"
#include <VrApi_Types.h>

LobbyRenderer::LobbyRenderer(teleport::client::ClientDeviceState *s,ClientAppInterface *c)
:clientDeviceState(s),clientAppInterface (c)
{
}

void LobbyRenderer::Render()
{
	static avs::vec3 offset={0,0,3.5f};
	static avs::vec4 colour={1.0f,0.7f,0.1f,0.5f};
	clientAppInterface->PrintText(offset,colour,"Not connected");
}
