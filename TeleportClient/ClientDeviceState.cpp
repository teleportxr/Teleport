#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

using namespace teleport;
using namespace client;

ClientDeviceState::ClientDeviceState()
{
}

void ClientDeviceState::TransformPose(LocalGlobalPose &p)
{
	clientrender::quat stickRotateQ = originPose.orientation;// (stickYaw, avs::vec3(0, 1.0f, 0));
	clientrender::quat localQ=*((clientrender::quat*)(&p.localPose.orientation));
	clientrender::quat globalQ=(stickRotateQ*localQ);
	p.globalPose.orientation=globalQ;

	avs::vec3 relp=p.localPose.position;
	clientrender::quat localP=*((clientrender::quat*)(&(relp)));
	localP.s=0;
	clientrender::quat globalP=(stickRotateQ*localP)*(stickRotateQ.Conjugate());
	p.globalPose.position=avs::vec3(globalP.i,globalP.j,globalP.k);
	p.globalPose.position+=originPose.position;
}

void ClientDeviceState::SetHeadPose(avs::vec3 pos,clientrender::quat q)
{
	headPose.localPose.orientation=*((const avs::vec4 *)(&q));
	headPose.localPose.position=pos;
	TransformPose(headPose);
}

void ClientDeviceState::SetControllerPose(int index,avs::vec3 pos,clientrender::quat q)
{
	controllerPoses[index].localPose.position = pos;
	controllerPoses[index].localPose.orientation = *((const avs::vec4 *)(&q));
	TransformPose(controllerPoses[index]);
}

void ClientDeviceState::SetInputs( const teleport::client::Input& st)
{
	input =st;
}

void ClientDeviceState::UpdateGlobalPoses()
{
	TransformPose(headPose);
	TransformPose(controllerPoses[0]);
	TransformPose(controllerPoses[1]);
}