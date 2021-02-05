#pragma once
#include <libavstream/common.hpp>
#include "basic_linear_algebra.h"
#include <VrApi_Types.h>

class ClientDeviceState
{
public:
	ClientDeviceState();

	//avs::vec3 localOriginPos;		// in metres. The headPose will be relative to this.
	avs::vec3 localFootPos;			// in metres. From the device SDK
	avs::vec3 relativeHeadPos;
	scr::mat4 transformToLocalOrigin; // Because we're using OVR's rendering, we must position the actors relative to the oculus origin.
	float eyeHeight=0.5f;
	float stickYaw=0.0f;

	avs::Pose headPose;
	avs::Pose controllerPoses[2];
	avs::vec3 cameraPosition;	// in game absolute space.

	void TransformPose(avs::Pose &p);
	void SetHeadPose(ovrVector3f pos,ovrQuatf q);
	void SetControllerPose(int index,ovrVector3f pos,ovrQuatf q);
	void UpdateLocalOrigin();
};


