#include "GeometryStreamingService.h"

#include <iostream>

#include "ServerSettings.h"
#include "GeometryStore.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/TextCanvas.h"

using namespace teleport;
using namespace server;
ClientStoppedRenderingNodeFn PluginGeometryStreamingService::callback_clientStoppedRenderingNode=nullptr;
ClientStartedRenderingNodeFn PluginGeometryStreamingService::callback_clientStartedRenderingNode=nullptr;
//Remove duplicates, and 0s, from passed vector of UIDs.
void UniqueUIDsOnly(std::vector<avs::uid>& cleanedUIDs)
{
	std::sort(cleanedUIDs.begin(), cleanedUIDs.end());
	cleanedUIDs.erase(std::unique(cleanedUIDs.begin(), cleanedUIDs.end()), cleanedUIDs.end());
	cleanedUIDs.erase(std::remove(cleanedUIDs.begin(), cleanedUIDs.end(), 0), cleanedUIDs.end());
}

GeometryStreamingService::GeometryStreamingService(const ServerSettings* settings)
	:geometryStore(nullptr), settings(settings), clientNetworkContext(nullptr), geometryEncoder(settings,this)
{
}

GeometryStreamingService::~GeometryStreamingService()
{
	stopStreaming();
	if (avsPipeline)
		avsPipeline->deconfigure();
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

void GeometryStreamingService::getResourcesToStream(std::vector<avs::uid>& outNodeIDs
		,std::vector<avs::MeshNodeResources>& outMeshResources
		,std::vector<avs::LightNodeResources>& outLightResources
		,std::set<avs::uid>& genericTextureUids
		,std::vector<avs::uid> &textCanvases
		,std::vector<avs::uid> &fontAtlases
		,int32_t minimumPriority) const
{
	for(const auto &r:streamedGenericTextureUids)
	{
		genericTextureUids.insert(r);
	}
	for (avs::uid nodeID : streamedNodeIDs)
	{
		avs::Node* node = geometryStore->getNode(nodeID);
		if (!node)
		{
			continue;
		}

		switch (node->data_type)
		{
		case avs::NodeDataType::None:
		case avs::NodeDataType::Light:
			outNodeIDs.push_back(nodeID);
			break;
		case avs::NodeDataType::Mesh:
			if(node->priority>=minimumPriority)
			{
				GetMeshNodeResources(nodeID, *node, outMeshResources, minimumPriority);
				if(node->renderState.globalIlluminationUid>0)
				{
					genericTextureUids.insert(node->renderState.globalIlluminationUid);
				}
			}
			break;
		case avs::NodeDataType::TextCanvas:
			if(node->data_uid)
			{
				const teleport::core::TextCanvas *c=geometryStore->getTextCanvas(node->data_uid);
				if(c&&c->font_uid)
				{
					const teleport::core::FontAtlas *f=geometryStore->getFontAtlas(c->font_uid);
					if(f)
					{
						textCanvases.push_back(node->data_uid);
						fontAtlases.push_back(c->font_uid);
						avs::uid texture_uid=f->font_texture_uid;
						genericTextureUids.insert(texture_uid);
						outNodeIDs.push_back(nodeID);
					}
				}
			}
			break;
		default:
			break;
		}
	}
}


avs::AxesStandard GeometryStreamingService::getClientAxesStandard() const
{
	return clientNetworkContext->axesStandard;
}

avs::RenderingFeatures GeometryStreamingService::getClientRenderingFeatures() const
{
	return handshake.renderingFeatures;
}

void GeometryStreamingService::startStreaming(server::ClientNetworkContext* context, const teleport::core::Handshake& h)
{
	if (clientNetworkContext == context)
	{
		return;
	}
	handshake=h;
	clientNetworkContext = context;

 	avsPipeline.reset(new avs::Pipeline);
	avsGeometrySource.reset(new avs::GeometrySource);
	avsGeometryEncoder.reset(new avs::GeometryEncoder);
	avsGeometrySource->configure( this);
	avsGeometryEncoder->configure(&geometryEncoder);

	avsPipeline->link({ avsGeometrySource.get(), avsGeometryEncoder.get(), clientNetworkContext->GeometryQueue.get() });
}

void GeometryStreamingService::stopStreaming()
{
	if (avsPipeline)
	{
		avsPipeline->deconfigure();
	}
	if (avsGeometrySource)
	{
		avsGeometrySource->deconfigure();
	}
	if (avsGeometryEncoder)
	{
		avsGeometryEncoder->deconfigure();
	}
	avsPipeline.reset();
	clientNetworkContext = nullptr;

	reset();
}

void GeometryStreamingService::clientStartedRenderingNode(avs::uid clientID, avs::uid nodeID)
{
	auto nodePair = streamedNodeIDs.find(nodeID);
	if (nodePair != streamedNodeIDs.end())
	{
		bool result=clientStartedRenderingNode_Internal(clientID, nodeID);
		if(result)
			clientRenderingNodes.insert(nodeID);
	}
	else
	{
		TELEPORT_COUT << "Client started rendering non-streamed node with ID of " << nodeID << "!\n";
	}
}

void GeometryStreamingService::clientStoppedRenderingNode(avs::uid clientID, avs::uid nodeID)
{
	auto nodePair = clientRenderingNodes.find(nodeID);
	if (nodePair != clientRenderingNodes.end())
	{
		clientStoppedRenderingNode_Internal(clientID, nodeID);
		clientRenderingNodes.erase(nodeID);
	}
	else
	{
		TELEPORT_COUT << "Client stopped rendering node with ID of " << nodeID << " - didn't know it was rendering this!\n";
	}
}

void GeometryStreamingService::setNodeVisible(avs::uid clientID, avs::uid nodeID, bool isVisible)
{
	if (isVisible)
	{
		clientStoppedRenderingNode(clientID, nodeID);
	}
	else
	{
		clientStartedRenderingNode(clientID, nodeID);
	}
}

bool GeometryStreamingService::isClientRenderingNode(avs::uid nodeID)
{
	return clientRenderingNodes.find(nodeID) != clientRenderingNodes.end();
}

void GeometryStreamingService::tick(float deltaTime)
{
	// Might not be initialized... YET
	if (!avsPipeline || !settings->enableGeometryStreaming)
		return;
	// We can now be confident that all streamable geometries have been initialized, so we will do internal setup.
	// Each frame we manage a view of which streamable geometries should or shouldn't be rendered on our client.

	//Increment time for unconfirmed resources, if they pass the max time then they are flagged to be sent again.
	for (auto it = unconfirmedResourceTimes.begin(); it != unconfirmedResourceTimes.end(); it++)
	{
		it->second += deltaTime;

		if (it->second > settings->confirmationWaitTime)
		{
			TELEPORT_COUT << "Resource " << it->first << " was not confirmed within " << settings->confirmationWaitTime << " seconds, and will be resent.\n";

			sentResources[it->first] = false;
			it = unconfirmedResourceTimes.erase(it);
		}
	}

	// For this client's POSITION and OTHER PROPERTIES,
	// Use the Geometry Source to determine which PipelineNode uid's are relevant.

	avsPipeline->process();
}

void GeometryStreamingService::reset()
{
	sentResources.clear();

	unconfirmedResourceTimes.clear();
	streamedNodeIDs.clear();
	clientRenderingNodes.clear();
}

void GeometryStreamingService::addNode(avs::uid nodeID)
{
	if (nodeID != 0)
	{
		streamedNodeIDs.insert(nodeID);
	}
}

void GeometryStreamingService::removeNode(avs::uid nodeID)
{
	streamedNodeIDs.erase(nodeID);
}

bool GeometryStreamingService::isStreamingNode(avs::uid nodeID)
{
	return streamedNodeIDs.find(nodeID) != streamedNodeIDs.end();
}

void GeometryStreamingService::addGenericTexture(avs::uid id)
{
	streamedGenericTextureUids.insert(id);
}

void GeometryStreamingService::GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources,int32_t minimumPriority) const
{
	if (node.data_type != avs::NodeDataType::Mesh)
	{
		return;
	}
	// Don't send if not needed by this particular client.
	if(node.priority<minimumPriority)
	{
		return;
	}
	avs::MeshNodeResources meshNode;
	meshNode.node_uid = nodeID;
	meshNode.mesh_uid = node.data_uid;
	meshNode.skinID = node.skinID;

	//Get joint/bone IDs, if the skinID is not zero.
	if (meshNode.skinID != 0)
	{
		avs::Skin* skin = geometryStore->getSkin(node.skinID, getClientAxesStandard());
		meshNode.boneIDs = skin->boneIDs;
	}

	meshNode.animationIDs = node.animations;

	for (avs::uid material_uid : node.materials)
	{
		avs::Material* thisMaterial = geometryStore->getMaterial(material_uid);
		if (!thisMaterial)
		{
			TELEPORT_CERR << "Error when locating materials for encoding! Material " << material_uid << " was not found in the Geometry Store!\n";
			continue;
		}

		avs::MaterialResources material;
		material.material_uid = material_uid;

		material.texture_uids =
		{
			thisMaterial->pbrMetallicRoughness.baseColorTexture.index,
			thisMaterial->pbrMetallicRoughness.metallicRoughnessTexture.index,
			thisMaterial->emissiveTexture.index
		};
		if(handshake.renderingFeatures.normals)
			material.texture_uids.push_back(thisMaterial->normalTexture.index);
		if (handshake.renderingFeatures.ambientOcclusion)
			material.texture_uids.push_back(thisMaterial->occlusionTexture.index);

		UniqueUIDsOnly(material.texture_uids);

		meshNode.materials.push_back(material);
	}

	outMeshResources.push_back(meshNode);
}