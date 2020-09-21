#pragma once

#include "SurfaceRender.h"

#include "libavstream/common.hpp"

#include "ActorManager.h"
#include "GlobalGraphicsResources.h"

class OVRActor : public scr::Actor
{
public:
	OVRActor(avs::uid id)
			:Actor(id)
	{}
	void Init(const scr::Actor::ActorCreateInfo& actorCreateInfo);
	std::vector<OVR::ovrSurfaceDef> ovrSurfaceDefs;
};

class OVRActorManager : public scr::ActorManager
{
public:

    virtual ~OVRActorManager() = default;


	//Changes PBR effect used on actors/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
    void ChangeEffectPass(const char* effectPassName);
private:
    GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();

    //Creates a native actor with the passed id, and actor information.
    //  actorID : ID of the native actor to be created; same as SCR actor.
    //  actorInfo : Information to use to create the native actor.
    std::vector<OVR::ovrSurfaceDef> CreateNativeActor(avs::uid actorID, const scr::Actor::ActorCreateInfo& actorCreateInfo);
};