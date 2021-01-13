#include "GeometryStreamingService.h"

#include <iostream>

#include "CasterSettings.h"
#include "GeometryStore.h"
#include "ErrorHandling.h"

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
	//Confirm again; in case something just elapsed the timer, but has yet to be sent.
	sentResources[resource_uid] = true;
}

void GeometryStreamingService::getResourcesToStream(std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources) const
{
	for(auto n : streamedNodeUids)
	{
		GetMeshNodeResources(n, outMeshResources);
	}

	outLightResources = geometryStore->getLightNodes();
}

void GeometryStreamingService::startStreaming(SCServer::CasterContext* context)
{
	if(casterContext == context)
		return;
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
	if(avsPipeline)
		avsPipeline->deconfigure();
	if(avsGeometrySource)
		avsGeometrySource->deconfigure();
	if(avsGeometryEncoder)
		avsGeometryEncoder->deconfigure();
	avsPipeline.reset();
	casterContext = nullptr;

	reset();
}

void GeometryStreamingService::hideNode(avs::uid clientID,avs::uid nodeID)
{
	auto actorPair = streamedNodeUids.find(nodeID);
	if(actorPair != streamedNodeUids.end())
	{
		hideActor_Internal(clientID,nodeID);
		hiddenNodes.insert(nodeID);
	}
	else
	{
		std::cout << "Tried to hide non-streamed actor with ID of " << nodeID <<"!\n";
	}
}

void GeometryStreamingService::showNode(avs::uid clientID,avs::uid nodeID)
{
	auto actorPair = hiddenNodes.find(nodeID);
	if(actorPair != hiddenNodes.end())
	{
		showActor_Internal(clientID,nodeID);
		hiddenNodes.erase(nodeID);
	}
	else
	{
		std::cout << "Tried to show non-hidden actor with ID of " << nodeID << "!\n";
	}
}

void GeometryStreamingService::setNodeVisible(avs::uid clientID,avs::uid nodeID, bool isVisible)
{
	if(isVisible)
		showNode(clientID,nodeID);
	else
		hideNode(clientID,nodeID);
}

bool SCServer::GeometryStreamingService::isClientRenderingNode(avs::uid nodeID)
{
	return hiddenNodes.find(nodeID) != hiddenNodes.end();
}

void GeometryStreamingService::tick(float deltaTime)
{
	// Might not be initialized... YET
	if(!avsPipeline || !settings->enableGeometryStreaming)
		return;
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
	streamedNodeUids.clear();
	hiddenNodes.clear();
}

void GeometryStreamingService::addActor( avs::uid nodeID)
{
	if(nodeID != 0)
	{
		streamedNodeUids.insert(nodeID);
	}
}

avs::uid GeometryStreamingService::removeActorByID(avs::uid nodeID)
{
	streamedNodeUids.erase(nodeID);

	return nodeID;
}

bool GeometryStreamingService::isStreamingActorID(avs::uid actorID)
{
	return streamedNodeUids.find(actorID) != streamedNodeUids.end();
}

void GeometryStreamingService::GetMeshNodeResources(avs::uid node_uid, std::vector<avs::MeshNodeResources>& outMeshResources) const
{
	avs::DataNode* thisNode = geometryStore->getNode(node_uid);
	if(!thisNode || thisNode->data_uid == 0)
		return;

	avs::MeshNodeResources meshNode;
	meshNode.node_uid = node_uid;
	meshNode.mesh_uid = thisNode->data_uid;
	meshNode.skinID = thisNode->skinID;

	//Get joint/bone IDs, if the skinID is not zero.
	if(meshNode.skinID != 0)
	{
		avs::Skin* skin = geometryStore->getSkin(thisNode->skinID, getAxesStandard());
		meshNode.jointIDs = skin->jointIDs;
	}
	
	meshNode.animationIDs = thisNode->animations;

	for(avs::uid material_uid : thisNode->materials)
	{
		avs::Material* thisMaterial = geometryStore->getMaterial(material_uid);
		if(!thisMaterial)
		{
			TELEPORT_CERR<<"Material not found in store: "<<(unsigned long long)material_uid<<std::endl;
			continue;
		}
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

	for(avs::uid childNode_uid : thisNode->childrenIDs)
	{
		GetMeshNodeResources(childNode_uid, outMeshResources);
	}

	outMeshResources.push_back(meshNode);
}
