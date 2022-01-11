#include "OVRNodeManager.h"

#include <Render/GlGeometry.h>
#include "OVR_LogUtils.h"

#include "GlobalGraphicsResources.h"
#include "OVRNode.h"

std::shared_ptr<scr::Node> OVRNodeManager::CreateNode(avs::uid id, const avs::Node &avsNode)
{
	std::shared_ptr<scr::Node> node= std::make_shared<scr::Node>(id, avsNode.name);

	//Create MeshNode even if it is missing resources
	AddNode(node, avsNode);
	return node;
}

void OVRNodeManager::ChangeEffectPass(const char* effectPassName)
{
	GlobalGraphicsResources& globalGraphicsResources = GlobalGraphicsResources::GetInstance();

	//Early-out if this we aren't actually changing the effect pass.
	if(globalGraphicsResources.effectPassName== effectPassName)
	{
		return;
	}

	return;
	//TODO: Why does this crash?
	globalGraphicsResources.effectPassName = effectPassName;

	//Change effect used by all nodes/surfaces.
	for(auto& nodePair : nodeLookup)
	{
		std::static_pointer_cast<OVRNode>(nodePair.second)->ChangeEffectPass(effectPassName);
	}
}
