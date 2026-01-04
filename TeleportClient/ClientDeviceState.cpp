#include "ClientDeviceState.h"

using namespace teleport;
using namespace client;


void ClientServerState::SetHeadPose_StageSpace(vec3 pos,clientrender::quat q)
{
	headPose.orientation=*((const vec4_packed *)(&q));
	headPose.position=packed(pos);
}

void ClientServerState::SetInputs( const teleport::core::Input& st)
{
	input =st;
}
