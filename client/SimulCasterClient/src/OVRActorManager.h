#pragma once

#include "SurfaceRender.h"

#include "libavstream/common.hpp"

#include "ActorManager.h"
#include "GlobalGraphicsResources.h"

class OVRActor : public scr::Node
{
public:
	OVRActor(avs::uid id)
		:Node(id)
	{}

	std::vector<OVR::ovrSurfaceDef> ovrSurfaceDefs;
};

class OVRActorManager : public scr::ActorManager
{
public:

    virtual ~OVRActorManager() = default;

    virtual std::shared_ptr<scr::Node> CreateActor(avs::uid id) const override;

	virtual void AddActor(std::shared_ptr<scr::Node> actor, bool isHand) override;

	//Changes PBR effect used on actors/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
    void ChangeEffectPass(const char* effectPassName);
private:
    GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();

    //Creates a native actor with the passed id, and actor information.
    //  actorInfo : Information to use to create the native actor.
    std::vector<OVR::ovrSurfaceDef> CreateNativeActor(std::shared_ptr<scr::Node> actor);
};