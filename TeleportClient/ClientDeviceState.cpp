#include "ClientDeviceState.h"

using namespace teleport;
using namespace client;


void ClientServerState::SetHeadPose_StageSpace(vec3 pos,clientrender::quat q)
{
	headPose.localPose.orientation=*((const vec4 *)(&q));
	headPose.localPose.position=pos;
}

void ClientServerState::SetInputs( const teleport::core::Input& st)
{
	input =st;
}
