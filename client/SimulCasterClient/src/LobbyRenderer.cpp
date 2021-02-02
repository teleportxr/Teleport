#include "LobbyRenderer.h"
#include "ClientDeviceState.h"

LobbyRenderer::LobbyRenderer(ClientDeviceState *s)
:clientDeviceState(s)
{
}
void LobbyRenderer::Render(OVR::OvrGuiSys *mGuiSys)
{
	mGuiSys->ShowInfoText(0.001f,"Not connected\n"
							  "Head pos: %1.3f, %1.3f, %1.3f\n"
								"Foot pos: %1.3f, %1.3f, %1.3f\n"
								 "eye height: %1.3f\n"
								 "eye yaw: %1.3f\n"
			,clientDeviceState->relativeHeadPos.x,clientDeviceState->relativeHeadPos.y,clientDeviceState->relativeHeadPos.z
			,clientDeviceState->localFootPos.x,clientDeviceState->localFootPos.y,clientDeviceState->localFootPos.z
			,clientDeviceState->eyeHeight
			,clientDeviceState->eyeYaw);
}
