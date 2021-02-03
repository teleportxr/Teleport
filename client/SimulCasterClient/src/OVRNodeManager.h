#pragma once

#include "SurfaceRender.h"

#include "libavstream/common.hpp"

#include "NodeManager.h"
#include "GlobalGraphicsResources.h"
#include "OVRNode.h"

class OVRNodeManager : public scr::NodeManager
{
public:

    virtual ~OVRNodeManager() = default;

    virtual std::shared_ptr<scr::Node> CreateActor(avs::uid id, const std::string& name) const override;

	virtual void AddActor(std::shared_ptr<scr::Node> actor, const avs::DataNode& node) override;

	//Changes PBR effect used on actors/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
    void ChangeEffectPass(const char* effectPassName);
};