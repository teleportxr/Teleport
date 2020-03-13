#pragma once

#include "SurfaceRender.h"

#include "libavstream/common.hpp"

#include "ActorManager.h"
#include "GlobalGraphicsResources.h"

class OVRActorManager : public scr::ActorManager
{
public:
    struct LiveOVRActor : public LiveActor
    {
        LiveOVRActor(LiveActor&& baseActor, std::vector<OVR::ovrSurfaceDef> ovrSurfaceDefs)
        :LiveActor(baseActor), ovrSurfaceDefs(ovrSurfaceDefs)
        {}

        std::vector<OVR::ovrSurfaceDef> ovrSurfaceDefs;
    };

    virtual ~OVRActorManager() = default;

    virtual void CreateActor(avs::uid actorID, const scr::Actor::ActorCreateInfo& pActorCreateInfo) override;
    virtual void CreateHand(avs::uid handID, const scr::Actor::ActorCreateInfo& handCreateInfo) override;

	//Changes PBR effect used on actors/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
    void ChangeEffectPass(const char* effectPassName);

    void GetHands(LiveOVRActor*& leftHand, LiveOVRActor*& rightHand);
private:
    GlobalGraphicsResources& GlobalGraphicsResources = GlobalGraphicsResources::GetInstance();

    //Creates a native actor with the passed id, and actor information.
    //  actorID : ID of the native actor to be created; same as SCR actor.
    //  actorInfo : Information to use to create the native actor.
    std::vector<OVR::ovrSurfaceDef> CreateNativeActor(avs::uid actorID, const scr::Actor::ActorCreateInfo& actorCreateInfo);
};