//
// Created by roder on 06/04/2020.
//

#include "ClientRenderer.h"
#include "OVRActorManager.h"

using namespace OVR;
ClientRenderer::ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i):oculusOrigin(0,0,0)
		, resourceManagers(rm)
		,resourceCreator(r)
{
}


ClientRenderer::~ClientRenderer()
{
}

void ClientRenderer::RenderLocalActors(ovrFrameResult& res)
{
	// Because we're using OVR's rendering, we must position the actor's relative to the oculus origin.
	OVR::Matrix4f transform;
	scr::mat4 transformToOculusOrigin = scr::mat4::Translation(-oculusOrigin);

	auto RenderLocalActor = [&](OVRActorManager::LiveOVRActor* ovrActor)
	{
		const scr::Actor& actor = ovrActor->actor;

		//----OVR Actor Set Transforms----//
		scr::mat4 scr_Transform = transformToOculusOrigin * actor.GetTransform().GetTransformMatrix();
		memcpy(&transform.M[0][0], &scr_Transform.a, 16 * sizeof(float));

		for(size_t matIndex = 0; matIndex < actor.GetMaterials().size(); matIndex++)
		{
			if(matIndex >= ovrActor->ovrSurfaceDefs.size())
			{
				//OVR_LOG("Skipping empty element in ovrSurfaceDefs.");
				break;
			}

			res.Surfaces.emplace_back(transform, &ovrActor->ovrSurfaceDefs[matIndex]);
		}
	};

	//Render local actors.
	const std::vector<std::unique_ptr<scr::ActorManager::LiveActor>>& actorList = resourceManagers->mActorManager->GetActorList();
	for(size_t actorIndex = 0; actorIndex < resourceManagers->mActorManager->getVisibleActorAmount(); actorIndex++)
	{
		OVRActorManager::LiveOVRActor* ovrActor = static_cast<OVRActorManager::LiveOVRActor*>(actorList[actorIndex].get());
		RenderLocalActor(ovrActor);
	}

	//Retrieve hands.
	OVRActorManager::LiveOVRActor *leftHand = nullptr, *rightHand = nullptr;
	dynamic_cast<OVRActorManager*>(resourceManagers->mActorManager.get())->GetHands(leftHand, rightHand);

	//Render hands, if they exist.
	if(leftHand) RenderLocalActor(leftHand);
	if(rightHand) RenderLocalActor(rightHand);
}