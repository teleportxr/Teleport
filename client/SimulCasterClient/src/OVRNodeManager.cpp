#include "OVRNodeManager.h"

#include "GlGeometry.h"
#include "OVR_LogUtils.h"

#include "SCR_Class_GL_Impl/GL_Effect.h"
#include "SCR_Class_GL_Impl/GL_IndexBuffer.h"
#include "SCR_Class_GL_Impl/GL_Texture.h"
#include "SCR_Class_GL_Impl/GL_UniformBuffer.h"
#include "SCR_Class_GL_Impl/GL_VertexBuffer.h"

#include "GlobalGraphicsResources.h"

using namespace OVR;
using namespace scc;
using namespace scr;

std::shared_ptr<scr::Node> OVRNodeManager::CreateActor(avs::uid id, const std::string& name) const
{
    return std::make_shared<OVRNode>(id, name);
}

void OVRNodeManager::AddActor(std::shared_ptr<Node> actor, const avs::DataNode& node)
{
	NodeManager::AddActor(actor, node);
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

    //Change effect used by all actors/surfaces.
    for(auto& actorPair : actorLookup)
    {
        std::static_pointer_cast<OVRNode>(actorPair.second)->ChangeEffectPass(effectPassName);
    }
}
