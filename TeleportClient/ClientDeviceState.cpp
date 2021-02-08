#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

ClientDeviceState::ClientDeviceState():
//localOriginPos(0,0,0),
relativeHeadPos(0,0,0)
//, transformToLocalOrigin(scr::mat4::Translation(-localOriginPos))
{
}

void ClientDeviceState::UpdateOriginPose()
{
	// Footspace is related to local space as follows:
	// The orientation of footspace is identical to the orientation of localspace.
	// Footspace is offset from local space by LocalFootPos, which is measured in game space.
	originPose.position=localFootPos;
	originPose.orientation=scr::quat(stickYaw,avs::vec3(0,1.0f,0));
}

void ClientDeviceState::TransformPose(avs::Pose &p)
{
	scr::quat stickRotateQ(stickYaw,avs::vec3(0,1.0f,0));
	scr::quat localQ=*((scr::quat*)(&p.orientation));
	scr::quat globalQ=(stickRotateQ*localQ);//*(stickRotateQ.Conjugate());
	p.orientation=globalQ;

	avs::vec3 relp=p.position;
	scr::quat localP=*((scr::quat*)(&(relp)));
	localP.s=0;
	scr::quat globalP=(stickRotateQ*localP)*(stickRotateQ.Conjugate());
	p.position=avs::vec3(globalP.i,globalP.j,globalP.k);
}

void ClientDeviceState::SetHeadPose(avs::vec3 pos,scr::quat q)
{
	headPose.orientation=*((const avs::vec4 *)(&q));
	headPose.position=pos;
	TransformPose(headPose);
	relativeHeadPos=headPose.position;
	headPose.position+=localFootPos;
}

void ClientDeviceState::SetControllerPose(int index,avs::vec3 pos,scr::quat q)
{
	controllerPoses[index].position = pos;
	controllerPoses[index].orientation = *((const avs::vec4 *)(&q));
	TransformPose(controllerPoses[index]);
	controllerPoses[index].position+=localFootPos;
}