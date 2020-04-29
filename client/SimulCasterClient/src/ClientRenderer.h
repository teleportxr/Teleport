//
// (C) Copyright 2018 Simul
#pragma once
#include "App.h"
#include "basic_linear_algebra.h"
#include "ResourceCreator.h"
#include "crossplatform/SessionClient.h"


class ClientRenderer
{
public:
	ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i);
	~ClientRenderer();

	void EnteredVR(struct ovrMobile *ovrMobile);
	void ExitedVR();
	void UpdateHandObjects();
	void RenderLocalActors(OVR::ovrFrameResult& res);
	scr::vec3 oculusOrigin;		// in metres. The headPose will be relative to this.

	scr::ResourceManagers *resourceManagers;
	ResourceCreator        *resourceCreator;
	ovrMobile                *mOvrMobile;
	avs::HeadPose headPose;
	avs::HeadPose controllerPoses[2];
	scr::vec3 cameraPosition;	// in real space.
	const scr::quat HAND_ROTATION_DIFFERENCE {0.0000000456194194, 0.923879385, -0.382683367, 0.000000110135019}; //Adjustment to the controller's rotation to get the desired rotation.

};
