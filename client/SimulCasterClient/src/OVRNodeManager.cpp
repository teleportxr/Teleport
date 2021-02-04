#include "OVRNodeManager.h"

#include "GlGeometry.h"
#include "OVR_LogUtils.h"

#include "GlobalGraphicsResources.h"
#include "OVRNode.h"

std::shared_ptr<scr::Node> OVRNodeManager::CreateNode(avs::uid id, const std::string& name) const
{
	return std::make_shared<OVRNode>(id, name);
}

void OVRNodeManager::ChangeEffectPass(const char* effectPassName)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

	//Early-out if this we aren't actually changing the effect pass.
	if(strcmp(globalGraphicsResources.effectPassName, effectPassName) == 0)
	{
		return;
	}

	globalGraphicsResources.effectPassName = const_cast<char*>(effectPassName);

	//Change effect used by all nodes/surfaces.
	for(auto& nodePair : nodeLookup)
	{
		std::static_pointer_cast<OVRNode>(nodePair.second)->ChangeEffectPass(effectPassName);
	}
}
