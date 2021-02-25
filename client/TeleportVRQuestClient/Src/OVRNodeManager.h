#pragma once

#include "libavstream/common.hpp"
#include "crossplatform/NodeManager.h"

class OVRNodeManager : public scr::NodeManager
{
public:
	virtual ~OVRNodeManager() = default;

	virtual std::shared_ptr<scr::Node> CreateNode(avs::uid id, const std::string& name) const override;

	//Changes PBR effect used on nodes/surfaces to the effect pass with the passed name.
	//Also changes GlobalGraphicsResource::effectPassName.
	void ChangeEffectPass(const char* effectPassName);
};
