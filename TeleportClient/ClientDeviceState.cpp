#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

using namespace teleport;
using namespace client;
static std::map<avs::uid,std::shared_ptr<ClientServerState>> clientServerStates;
ClientServerState &ClientServerState::GetClientServerState(avs::uid u)
{
	auto i=clientServerStates.find(u);
	if(i==clientServerStates.end())
	{
		std::shared_ptr<ClientServerState> s=std::make_shared<ClientServerState>();
		clientServerStates[u]=s;
		return *(s.get());
	}
	return *(i->second);
}

ClientServerState::ClientServerState()
{
}


void ClientServerState::TransformPose(LocalGlobalPose &p)
{
/*	clientrender::quat stickRotateQ = originPose.orientation;
	clientrender::quat localQ=*((clientrender::quat*)(&p.localPose.orientation));
	clientrender::quat globalQ=(stickRotateQ*localQ);
	p.globalPose.orientation=*((avs::vec4*)&globalQ);

	avs::vec3 relp=p.localPose.position;
	clientrender::quat localP=*((clientrender::quat*)(&(relp)));
	localP.s=0;
	clientrender::quat globalP=(stickRotateQ*localP)*(stickRotateQ.Conjugate());
	p.globalPose.position=avs::vec3(globalP.i,globalP.j,globalP.k);
	p.globalPose.position+=originPose.position;*/
}

void ClientServerState::SetHeadPose_StageSpace(avs::vec3 pos,clientrender::quat q)
{
	headPose.localPose.orientation=*((const avs::vec4 *)(&q));
	headPose.localPose.position=pos;
	//TransformPose(headPose);
}

void ClientServerState::SetInputs( const teleport::core::Input& st)
{
	input =st;
}

void ClientServerState::UpdateGlobalPoses()
{
	TransformPose(headPose);
}