#include "LobbyRenderer.h"
#include "ClientDeviceState.h"
#include <VrApi_Types.h>

LobbyRenderer::LobbyRenderer(ClientDeviceState *s,ClientAppInterface *c)
:clientDeviceState(s),clientAppInterface (c)
{
}
void LobbyRenderer::Render()
{
	static avs::vec3 offset={0,0,3.5f};
	static avs::vec4 colour={1.0f,0.7f,0.1f,0.5f};
	clientAppInterface->PrintText(offset,colour,"Not connected\n"
							  "Head pos: %1.3f, %1.3f, %1.3f\n"
								"Foot pos: %1.3f, %1.3f, %1.3f\n"
								 "eye height: %1.3f\n"
								 "eye yaw: %1.3f\n"
			,clientDeviceState->relativeHeadPos.x,clientDeviceState->relativeHeadPos.y,clientDeviceState->relativeHeadPos.z
			,clientDeviceState->originPose.position.x,clientDeviceState->originPose.position.y,clientDeviceState->originPose.position.z
			,clientDeviceState->eyeHeight
			,clientDeviceState->stickYaw);
}
