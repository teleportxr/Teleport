#include "LobbyRenderer.h"
#include "ClientDeviceState.h"
#include <VrApi_Types.h>

LobbyRenderer::LobbyRenderer(ClientDeviceState *s)
:clientDeviceState(s)
{
}
void LobbyRenderer::Render(OVRFW::OvrGuiSys *mGuiSys)
{
	static ovrVector3f offset={0,0,3.5f};
	static ovrVector4f colour={1.0f,0.7f,0.1f,0.5f};
	mGuiSys->ShowInfoText(0.001f,offset,colour,"Not connected\n"
							  "Head pos: %1.3f, %1.3f, %1.3f\n"
								"Foot pos: %1.3f, %1.3f, %1.3f\n"
								 "eye height: %1.3f\n"
								 "eye yaw: %1.3f\n"
			,clientDeviceState->relativeHeadPos.x,clientDeviceState->relativeHeadPos.y,clientDeviceState->relativeHeadPos.z
			,clientDeviceState->originPose.position.x,clientDeviceState->originPose.position.y,clientDeviceState->originPose.position.z
			,clientDeviceState->eyeHeight
			,clientDeviceState->stickYaw);
}
