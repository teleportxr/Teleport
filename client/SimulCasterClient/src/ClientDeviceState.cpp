#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

ClientDeviceState::ClientDeviceState():
//localOriginPos(0,0,0),
relativeHeadPos(0,0,0)
//, transformToLocalOrigin(scr::mat4::Translation(-localOriginPos))
{
}

void ClientDeviceState::UpdateLocalOrigin()
{
	// Footspace is related to local space as follows:
	// The orientation of footspace is identical to the orientation of localspace.
	// Footspace is offset from local space by LocalFootPos, which is measured in game space.

	// Therefore: Any time
}

void ClientDeviceState::TransformPose(avs::Pose &p)
{
	scr::quat stickRotateQ(stickYaw,avs::vec3(0,1.0f,0));
	scr::quat localQ=*((scr::quat*)(&p.orientation));
	scr::quat globalQ=(stickRotateQ*localQ);//*(stickRotateQ.Conjugate());
	p.orientation=globalQ;

	avs::vec3 relp=p.position-relativeHeadPos;
	scr::quat localP=*((scr::quat*)(&(relp)));
	scr::quat globalP=(stickRotateQ*localP)*(stickRotateQ.Conjugate());
	p.position=localFootPos+relativeHeadPos+avs::vec3(globalP.i,globalP.j,globalP.k);
}

void ClientDeviceState::SetHeadPose(ovrVector3f pos,ovrQuatf q)
{
	headPose.orientation=*((const avs::vec4 *)(&q));
	headPose.position=*((const avs::vec3 *)(&pos));
	TransformPose(headPose);
}

void ClientDeviceState::SetControllerPose(int index,ovrVector3f pos,ovrQuatf q)
{
	controllerPoses[index].position = *((const avs::vec3 *)(&pos));
	controllerPoses[index].orientation = *((const avs::vec4 *)(&q));
	TransformPose(controllerPoses[index]);
}