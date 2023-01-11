#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

using namespace teleport;
using namespace client;

ClientDeviceState::ClientDeviceState()
{
}
void ClientDeviceState::Clear()
{
	nodePoses.clear();
}

void ClientDeviceState::SetLocalNodePose(avs::uid uid,const avs::Pose &localPose)
{
	nodePoses[uid].localPose=localPose;
	TransformPose(nodePoses[uid]);
}

const avs::Pose &ClientDeviceState::GetGlobalNodePose(avs::uid uid) const
{
	const auto &p=nodePoses.find(uid);
	if(p==nodePoses.end())
	{
	static avs::Pose p;
		return p;
	}
	return p->second.globalPose;
}

void ClientDeviceState::TransformPose(LocalGlobalPose &p)
{
	clientrender::quat stickRotateQ = originPose.orientation;// (stickYaw, avs::vec3(0, 1.0f, 0));
	clientrender::quat localQ=*((clientrender::quat*)(&p.localPose.orientation));
	clientrender::quat globalQ=(stickRotateQ*localQ);
	p.globalPose.orientation=*((avs::vec4*)&globalQ);

	avs::vec3 relp=p.localPose.position;
	clientrender::quat localP=*((clientrender::quat*)(&(relp)));
	localP.s=0;
	clientrender::quat globalP=(stickRotateQ*localP)*(stickRotateQ.Conjugate());
	p.globalPose.position=avs::vec3(globalP.i,globalP.j,globalP.k);
	p.globalPose.position+=originPose.position;
}

void ClientDeviceState::SetHeadPose_StageSpace(avs::vec3 pos,clientrender::quat q)
{
	headPose.localPose.orientation=*((const avs::vec4 *)(&q));
	headPose.localPose.position=pos;
	TransformPose(headPose);
}

void ClientDeviceState::SetInputs( const teleport::core::Input& st)
{
	input =st;
}

void ClientDeviceState::UpdateGlobalPoses()
{
	TransformPose(headPose);
}