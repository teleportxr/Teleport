//
// Created by roder on 06/04/2020.
//

#include "ClientRenderer.h"
#include "OVRActorManager.h"
#include "OVR_GlUtils.h"
#include "OVR_Math.h"

#include <VrApi_Types.h>
#include <VrApi_Input.h>

using namespace OVR;
ClientRenderer::ClientRenderer(ResourceCreator *r,scr::ResourceManagers *rm,SessionCommandInterface *i):oculusOrigin(0,0,0)
		, resourceManagers(rm)
		,resourceCreator(r)
		, mOvrMobile(nullptr)
{
}


ClientRenderer::~ClientRenderer()
{
	ExitedVR();
}

void ClientRenderer::EnteredVR(struct ovrMobile *o)
{
	mOvrMobile = o;
}

void ClientRenderer::ExitedVR()
{
	mOvrMobile=nullptr;
}

void ClientRenderer::UpdateHandObjects()
{
	std::vector<ovrTracking> remoteStates;

	uint32_t deviceIndex = 0;
	ovrInputCapabilityHeader capsHeader;
	//Poll controller state from the Oculus API.
	while( vrapi_EnumerateInputDevices(mOvrMobile, deviceIndex, &capsHeader ) >= 0 )
	{
		if(capsHeader.Type == ovrControllerType_TrackedRemote)
		{
			ovrTracking remoteState;
			if(vrapi_GetInputTrackingState(mOvrMobile, capsHeader.DeviceID, 0, &remoteState) >= 0)
			{
				remoteStates.push_back(remoteState);
				if(deviceIndex < 2)
				{
					scr::vec3 pos=oculusOrigin+*((const scr::vec3*)&remoteState.HeadPose.Pose.Position);

					controllerPoses[deviceIndex].position = *((const avs::vec3*)(&pos));
					controllerPoses[deviceIndex].orientation = *((const avs::vec4*)(&remoteState.HeadPose.Pose.Orientation));
				}
				else
				{
					break;
				}
			}
		}
		++deviceIndex;
	}

	OVRActorManager::LiveOVRActor* leftHand = nullptr;
	OVRActorManager::LiveOVRActor* rightHand = nullptr;
	dynamic_cast<OVRActorManager*>(resourceManagers->mActorManager.get())->GetHands(leftHand, rightHand);

	switch(remoteStates.size())
	{
		case 0:
			return;
		case 1: //Set non-dominant hand away. TODO: Query OVR for which hand is dominant/in-use.
			leftHand = nullptr;
			break;
		default:
			break;
	}

	if(rightHand)
	{
		rightHand->actor.UpdateModelMatrix
				(
						scr::vec3
								{
										remoteStates[0].HeadPose.Pose.Position.x + cameraPosition.x,
										remoteStates[0].HeadPose.Pose.Position.y + cameraPosition.y,
										remoteStates[0].HeadPose.Pose.Position.z + cameraPosition.z
								},
						scr::quat
								{
										remoteStates[0].HeadPose.Pose.Orientation.x,
										remoteStates[0].HeadPose.Pose.Orientation.y,
										remoteStates[0].HeadPose.Pose.Orientation.z,
										remoteStates[0].HeadPose.Pose.Orientation.w
								}
						* HAND_ROTATION_DIFFERENCE,
						rightHand->actor.GetTransform().m_Scale
				);
	}

	if(leftHand)
	{
		leftHand->actor.UpdateModelMatrix
				(
						scr::vec3
								{
										remoteStates[1].HeadPose.Pose.Position.x + cameraPosition.x,
										remoteStates[1].HeadPose.Pose.Position.y + cameraPosition.y,
										remoteStates[1].HeadPose.Pose.Position.z + cameraPosition.z
								},
						scr::quat
								{
										remoteStates[1].HeadPose.Pose.Orientation.x,
										remoteStates[1].HeadPose.Pose.Orientation.y,
										remoteStates[1].HeadPose.Pose.Orientation.z,
										remoteStates[1].HeadPose.Pose.Orientation.w
								}
						* HAND_ROTATION_DIFFERENCE,
						leftHand->actor.GetTransform().m_Scale
				);
	}
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
	if(leftHand)
		RenderLocalActor(leftHand);
	if(rightHand)
		RenderLocalActor(rightHand);
}