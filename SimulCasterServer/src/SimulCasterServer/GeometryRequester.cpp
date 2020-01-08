#include "GeometryRequester.h"

#include <iostream>

#include "GeometryStore.h"

using namespace SCServer;

//Remove duplicates, and 0s, from passed vector of UIDs.
void UniqueUIDsOnly(std::vector<avs::uid>& cleanedUIDs)
{
	std::sort(cleanedUIDs.begin(), cleanedUIDs.end());
	cleanedUIDs.erase(std::unique(cleanedUIDs.begin(), cleanedUIDs.end()), cleanedUIDs.end());
	cleanedUIDs.erase(std::remove(cleanedUIDs.begin(), cleanedUIDs.end(), 0), cleanedUIDs.end());
}

bool GeometryRequester::hasResource(avs::uid resource_uid) const
{
	return sentResources.find(resource_uid) != sentResources.end() && sentResources.at(resource_uid) == true;
}

void GeometryRequester::encodedResource(avs::uid resource_uid)
{
	sentResources[resource_uid] = true;
	unconfirmedResourceTimes[resource_uid] = 0;
}

void GeometryRequester::requestResource(avs::uid resource_uid)
{
	sentResources[resource_uid] = false;
	unconfirmedResourceTimes.erase(resource_uid);
}

void GeometryRequester::confirmResource(avs::uid resource_uid)
{
	unconfirmedResourceTimes.erase(resource_uid);
	//Confirm again; incase something just elapsed the timer, but has yet to be sent.
	sentResources[resource_uid] = true;
}

void GeometryRequester::getResourcesToStream(std::vector<avs::MeshNodeResources>& outMeshResources, std::vector<avs::LightNodeResources>& outLightResources)
{
	for(avs::uid actorID : streamedActors)
	{
		GetMeshNodeResources(actorID, outMeshResources);
	}

	outLightResources = geometryStore->getLightNodes();
}

void GeometryRequester::initialise(GeometryStore* geometryStore)
{
	this->geometryStore = geometryStore;
}

void GeometryRequester::startStreamingActor(avs::uid actorID)
{
	streamedActors.insert(actorID);
}

void GeometryRequester::stopStreamingActor(avs::uid actorID)
{
	streamedActors.erase(actorID);
}

void GeometryRequester::addHandsToStream()
{
	const std::vector<avs::uid>& handIDs = geometryStore->getHandIDs();

	for(avs::uid handID : handIDs)
	{
		streamedActors.insert(handID);
	}
}

void GeometryRequester::tick(float deltaTime)
{
	//Increment time for unconfirmed resources, if they pass the max time then they are flagged to be sent again.
	for(auto it = unconfirmedResourceTimes.begin(); it != unconfirmedResourceTimes.end(); it++)
	{
		it->second += deltaTime;

		if(it->second > confirmationWaitTime)
		{
			std::cout << "Resource with UID " << it->first << " was not confirmed within " << confirmationWaitTime << " seconds, and will be resent.\n";

			sentResources[it->first] = false;
			it = unconfirmedResourceTimes.erase(it);
		}
	}
}

void GeometryRequester::reset()
{
	sentResources.clear();
	unconfirmedResourceTimes.clear();
}

void GeometryRequester::GetMeshNodeResources(avs::uid node_uid, std::vector<avs::MeshNodeResources>& outMeshResources)
{
	avs::DataNode thisNode;
	geometryStore->getNode(node_uid, thisNode);
	if(thisNode.data_uid == 0) return;

	avs::MeshNodeResources meshNode;
	meshNode.node_uid = node_uid;
	meshNode.mesh_uid = thisNode.data_uid;

	for(avs::uid material_uid : thisNode.materials)
	{
		avs::Material thisMaterial;
		geometryStore->getMaterial(material_uid, thisMaterial);

		avs::MaterialResources material;
		material.material_uid = material_uid;

		material.texture_uids =
		{
			thisMaterial.pbrMetallicRoughness.baseColorTexture.index,
			thisMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index,
			thisMaterial.normalTexture.index,
			thisMaterial.occlusionTexture.index,
			thisMaterial.emissiveTexture.index
		};

		UniqueUIDsOnly(material.texture_uids);

		meshNode.materials.push_back(material);
	}

	for(avs::uid childNode_uid : thisNode.childrenUids)
	{
		GetMeshNodeResources(childNode_uid, outMeshResources);
	}

	outMeshResources.push_back(meshNode);
}
