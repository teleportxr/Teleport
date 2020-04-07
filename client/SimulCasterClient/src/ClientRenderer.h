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

	void RenderLocalActors(OVR::ovrFrameResult& res);
	scr::vec3 oculusOrigin;		// in metres. The headPose will be relative to this.

	scr::ResourceManagers *resourceManagers;
	ResourceCreator        *resourceCreator;
};
