#include "GeometryStreamingService.h"

#include <iostream>

#include "CasterSettings.h"
#include "GeometryStore.h"

using namespace SCServer;

//Remove duplicates, and 0s, from passed vector of UIDs.
void UniqueUIDsOnly(std::vector<avs::uid>& cleanedUIDs)
{
	std::sort(cleanedUIDs.begin(), cleanedUIDs.end());
	cleanedUIDs.erase(std::unique(cleanedUIDs.begin(), cleanedUIDs.end()), cleanedUIDs.end());
	cleanedUIDs.erase(std::remove(cleanedUIDs.begin(), cleanedUIDs.end(), 0), cleanedUIDs.end());
}

GeometryStreamingService::GeometryStreamingService(const CasterSettings* settings)
	:settings(settings), geometryEncoder(settings)
{}

GeometryStreamingService::~GeometryStreamingService()
{
	stopStreaming();
	if(avsPipeline) avsPipeline->deconfigure();
}

bool GeometryStreamingService::hasResource(avs::uid resource_uid) const
{
	return sentResources.find(resource_uid) != sentResources.end() && sentResources.at(resource_uid) == true;
}

void GeometryStreamingService::encodedResource(avs::uid resource_uid)
{
	sentResources[resource_uid] = true;
	unconfirmedResourceTimes[resource_uid] = 0;
}

void GeometryStreamingService::requestResource(avs::uid resource_uid)
{
	sentResources[resource_uid] = false;
	unconfirmedResourceTimes.erase(resource_uid);
}

void GeometryStreamingService::confirmResource(avs::uid resource_uid)
{
	unconfirmedResourceTimes.erase(resource_uid);
	//Confirm again; incase something just elapsed the timer, but has yet to be sent.
	sentResources[resource_uid] = true;
}

void GeometryStreamingService::getResourcesToStream(std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources) const
{
	for(auto actorPair : streamedActorIDs)
	{
		GetMeshNodeResources(actorPair.second, outMeshResources);
	}

	outLightResources = geometryStore->getLightNodes();
}

void GeometryStreamingService::startStreaming(SCServer::CasterContext* context)
{
	if(casterContext == context) return;
	casterContext = context;

	avsPipeline.reset(new avs::Pipeline);
	avsGeometrySource.reset(new avs::GeometrySource);
	avsGeometryEncoder.reset(new avs::GeometryEncoder);
	avsGeometrySource->configure(geometryStore, this);
	avsGeometryEncoder->configure(&geometryEncoder);

	avsPipeline->link({avsGeometrySource.get(), avsGeometryEncoder.get(), casterContext->GeometryQueue.get()});
}

void GeometryStreamingService::stopStreaming()
{
	if(avsPipeline) avsPipeline->deconfigure();
	if(avsGeometrySource) avsGeometrySource->deconfigure();
	if(avsGeometryEncoder) avsGeometryEncoder->deconfigure();
	avsPipeline.reset();
	casterContext = nullptr;

	reset();
}

void GeometryStreamingService::hideActor(avs::uid clientID,avs::uid actorID)
{
	auto actorPair = streamedActors.find(actorID);
	if(actorPair != streamedActors.end())
	{
		hideActor_Internal(clientID,actorPair->second);
		hiddenActors[actorID] = actorPair->second;
	}
	else
	{
		std::cout << "Tried to hide non-streamed actor with ID of " << actorID <<"!\n";
	}
}

void GeometryStreamingService::showActor(avs::uid clientID,avs::uid actorID)
{
	auto actorPair = hiddenActors.find(actorID);
	if(actorPair != hiddenActors.end())
	{
		showActor_Internal(clientID,actorPair->second);
		hiddenActors.erase(actorPair);
	}
	else
	{
		std::cout << "Tried to show non-hidden actor with ID of " << actorID << "!\n";
	}
}

void GeometryStreamingService::setActorVisible(avs::uid clientID,avs::uid actorID, bool isVisible)
{
	if(isVisible)
		showActor(clientID,actorID);
	else
		hideActor(clientID,actorID);
}

void GeometryStreamingService::addHandsToStream()
{
	const std::vector<std::pair<void*, avs::uid>>& hands = geometryStore->getHands();

	for(std::pair<void*, avs::uid> pointerIDHand : hands)
	{
		streamedActorIDs.emplace(pointerIDHand);
	}
}

void GeometryStreamingService::tick(float deltaTime)
{
	// Might not be initialized... YET
	if(!avsPipeline || !settings->enableGeometryStreaming) return;

	// We can now be confident that all streamable geometries have been initialized, so we will do internal setup.
	// Each frame we manage a view of which streamable geometries should or shouldn't be rendered on our client.

	//Increment time for unconfirmed resources, if they pass the max time then they are flagged to be sent again.
	for(auto it = unconfirmedResourceTimes.begin(); it != unconfirmedResourceTimes.end(); it++)
	{
		it->second += deltaTime;

		if(it->second > settings->confirmationWaitTime)
		{
			std::cout << "Resource with ID " << it->first << " was not confirmed within " << settings->confirmationWaitTime << " seconds, and will be resent.\n";

			sentResources[it->first] = false;
			it = unconfirmedResourceTimes.erase(it);
		}
	}

	// For this client's POSITION and OTHER PROPERTIES,
	// Use the Geometry Source to determine which Node uid's are relevant.

	avsPipeline->process();
}

void GeometryStreamingService::reset()
{
	sentResources.clear();
	unconfirmedResourceTimes.clear();
	streamedActorIDs.clear();
	streamedActors.clear();
	hiddenActors.clear();
}

void GeometryStreamingService::addActor(void* newActor, avs::uid actorID)
{
	if(actorID != 0)
	{
		streamedActorIDs[newActor] = actorID;
		streamedActors[actorID] = newActor;
	}
}

avs::uid GeometryStreamingService::removeActor(void* oldActor)
{
	avs::uid actorID = streamedActorIDs[oldActor];
	streamedActorIDs.erase(oldActor);
	streamedActors.erase(actorID);

	return actorID;
}

avs::uid GeometryStreamingService::getActorID(void* actor)
{
	auto idPair = streamedActorIDs.find(actor);

	return idPair != streamedActorIDs.end() ? idPair->second : 0;
}

bool GeometryStreamingService::isStreamingActor(void* actor)
{
	return streamedActorIDs.find(actor) != streamedActorIDs.end();
}

void GeometryStreamingService::GetMeshNodeResources(avs::uid node_uid, std::vector<avs::MeshNodeResources>& outMeshResources) const
{
	avs::DataNode* thisNode = geometryStore->getNode(node_uid);
	if(!thisNode || thisNode->data_uid == 0) return;

	avs::MeshNodeResources meshNode;
	meshNode.node_uid = node_uid;
	meshNode.mesh_uid = thisNode->data_uid;

	for(avs::uid material_uid : thisNode->materials)
	{
		avs::Material* thisMaterial = geometryStore->getMaterial(material_uid);

		avs::MaterialResources material;
		material.material_uid = material_uid;

		material.texture_uids =
		{
			thisMaterial->pbrMetallicRoughness.baseColorTexture.index,
			thisMaterial->pbrMetallicRoughness.metallicRoughnessTexture.index,
			thisMaterial->normalTexture.index,
			thisMaterial->occlusionTexture.index,
			thisMaterial->emissiveTexture.index
		};

		UniqueUIDsOnly(material.texture_uids);

		meshNode.materials.push_back(material);
	}

	for(avs::uid childNode_uid : thisNode->childrenUids)
	{
		GetMeshNodeResources(childNode_uid, outMeshResources);
	}

	outMeshResources.push_back(meshNode);
}
