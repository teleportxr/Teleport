#include "GeometryStreamingService.h"

#include <iostream>

#include "ServerSettings.h"
#include "GeometryStore.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/TextCanvas.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/Profiling.h"
#include "TeleportServer/ClientManager.h"
#pragma optimize("", off)

using namespace teleport;
using namespace server;
ClientStoppedRenderingNodeFn GeometryStreamingService::callback_clientStoppedRenderingNode=nullptr;
ClientStartedRenderingNodeFn GeometryStreamingService::callback_clientStartedRenderingNode=nullptr;
//Remove duplicates, and 0s, from passed vector of UIDs.
void UniqueUIDsOnly(std::vector<avs::uid>& cleanedUIDs)
{
	std::sort(cleanedUIDs.begin(), cleanedUIDs.end());
	cleanedUIDs.erase(std::unique(cleanedUIDs.begin(), cleanedUIDs.end()), cleanedUIDs.end());
	cleanedUIDs.erase(std::remove(cleanedUIDs.begin(), cleanedUIDs.end(), 0), cleanedUIDs.end());
}

GeometryStreamingService::GeometryStreamingService(ClientMessaging &c,avs::uid clid)
	: clientMessaging(c),geometryStore(nullptr), clientNetworkContext(nullptr), geometryEncoder(this, clid), clientId(clid)
{
	this->geometryStore = &GeometryStore::GetInstance();
}

GeometryStreamingService::~GeometryStreamingService()
{
	stopStreaming();
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
	TELEPORT_PROFILE_AUTOZONE;
	if(unconfirmedResourceTimes.find(resource_uid)==unconfirmedResourceTimes.end())
		return;
	unconfirmedResourceTimes.erase(resource_uid);
	// Is this resource a node?
	if(auto node=geometryStore->getNode(resource_uid))
	{
		if (streamedNodeIDs.find(resource_uid) == streamedNodeIDs.end())
		{
			// this node wasn't meant to be sent. Ignore this, it CAN happen e.g. if 
			// the client is out-of-date with the current state.
		}
		else if (unconfirmed_priority_counts.find(node->priority) == unconfirmed_priority_counts.end())
		{
			TELEPORT_INTERNAL_BREAK_ONCE("GeometryStreamingService::confirmResource Trying to decrement Node {0} priority {1} but it's not in the list.", resource_uid,node->priority);
		}
		else
		{
			unconfirmed_priority_counts[node->priority]--;
#if TELEPORT_DEBUG_NODE_STREAMING
			TELEPORT_COUT << "GeometryStreamingService::confirmResource Node " << resource_uid << " priority " << node->priority << ", count " << unconfirmed_priority_counts[node->priority] << "\n";
#endif
			if(unconfirmed_priority_counts[node->priority]==0)
			{
				unconfirmed_priority_counts.erase(node->priority);
				TELEPORT_COUT<<"Got all nodes of priority "<<node->priority<<"\n";
			}
		}
	}
	//Confirm again; in case something just elapsed the timer, but has yet to be sent.
	sentResources[resource_uid] = true;
}

void GeometryStreamingService::getResourcesToStream(std::set<avs::uid>& outNodeIDs
		,std::vector<avs::MeshNodeResources>& outMeshResources
		,std::vector<avs::LightNodeResources>& outLightResources
		,std::set<avs::uid>& genericTextureUids
		,std::vector<avs::uid> &textCanvases
		,std::vector<avs::uid> &fontAtlases
		,int32_t minimumPriority) const
{
	TELEPORT_PROFILE_AUTOZONE;
	for(const auto &r:streamedGenericTextureUids)
	{
		genericTextureUids.insert(r);
	}
	if(originNodeId)
		outNodeIDs.insert(originNodeId);

	int32_t lowest_confirmed_node_priority = -100000;
	// What is the lowest priority that has no unconfirmed nodes?
	// This unconfirmed_priority_counts is an ordered list of how many unconfirmed nodes each priority level has.
	// So the last value in that list gives the lowest priority we should stream.
	// When any value reaches zero, it's removed from the list.
	if(unconfirmed_priority_counts.size())
		lowest_confirmed_node_priority = unconfirmed_priority_counts.rbegin()->first;
	for (avs::uid nodeID : streamedNodeIDs)
	{
		avs::Node* node = geometryStore->getNode(nodeID);
		if (!node)
		{
			continue;
		}
		if(node->priority<lowest_confirmed_node_priority)
			continue;
		if (node->priority < minimumPriority)
			continue;
		outNodeIDs.insert(nodeID);

		switch (node->data_type)
		{
		case avs::NodeDataType::None:
		case avs::NodeDataType::Light:
			break;
		case avs::NodeDataType::Skeleton:
			GetSkeletonNodeResources(nodeID, *node, outMeshResources);
		break;
		case avs::NodeDataType::Mesh:
			if(node->priority>=minimumPriority)
			{
				GetMeshNodeResources(nodeID, *node, outMeshResources, minimumPriority);
				if(node->renderState.globalIlluminationUid>0)
				{
					genericTextureUids.insert(node->renderState.globalIlluminationUid);
				}
				if(node->skeletonNodeID!=0)
				{
					avs::Node* skeletonnode = geometryStore->getNode(node->skeletonNodeID);
					if(!skeletonnode)
					{
						TELEPORT_CERR<<"Missing skeleton node "<<node->skeletonNodeID<<std::endl;
					}
					else
					{
						outNodeIDs.insert(node->skeletonNodeID);
						GetSkeletonNodeResources(node->skeletonNodeID, *skeletonnode, outMeshResources);
						for(auto r:outMeshResources)
						{
							for(auto b:r.boneIDs)
							{
								outNodeIDs.insert(b);
							}
						}
					}
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

	avsPipeline->link({ avsGeometrySource.get(), avsGeometryEncoder.get(), &clientNetworkContext->NetworkPipeline.GeometryQueue });
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


bool GeometryStreamingService::startedRenderingNode( avs::uid nodeID)
{
	auto n = streamedNodeIDs.find(nodeID);
	if (n != streamedNodeIDs.end())
	{
		bool result = clientStartedRenderingNode_Internal(clientId, nodeID);
		if (result)
			clientRenderingNodes.insert(nodeID);
		return result;
	}
	else
	{
		TELEPORT_COUT << "Client started rendering non-streamed node with ID of " << nodeID << "!\n";
		return false;
	}
}

bool GeometryStreamingService::stoppedRenderingNode(avs::uid nodeID)
{
	auto nodePair = clientRenderingNodes.find(nodeID);
	if (nodePair != clientRenderingNodes.end())
	{
		bool res=clientStoppedRenderingNode_Internal(clientId, nodeID);
		clientRenderingNodes.erase(nodeID);
		return res;
	}
	else
	{
		TELEPORT_COUT << "Client stopped rendering node with ID of " << nodeID << " - didn't know it was rendering this!\n";
		return false;
	}
}


bool GeometryStreamingService::isClientRenderingNode(avs::uid nodeID)
{
	TELEPORT_PROFILE_AUTOZONE;
	return clientRenderingNodes.find(nodeID) != clientRenderingNodes.end();
}

void GeometryStreamingService::tick(float deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
	// Might not be initialized... YET
	if (!avsPipeline || !serverSettings.enableGeometryStreaming)
		return;
	// We can now be confident that all streamable geometries have been initialized, so we will do internal setup.
	// Each frame we manage a view of which streamable geometries should or shouldn't be rendered on our client.

	//Increment time for unconfirmed resources, if they pass the max time then they are flagged to be sent again.
	for (auto it = unconfirmedResourceTimes.begin(); it != unconfirmedResourceTimes.end(); )
	{
		it->second += deltaTime;

		if (it->second > serverSettings.confirmationWaitTime)
		{
		//	TELEPORT_COUT << "Resource " << it->first << " was not confirmed within " << settings->confirmationWaitTime << " seconds, and will be resent.\n";

			sentResources[it->first] = false;
			auto next_it=it;
			next_it++;
			it = unconfirmedResourceTimes.erase(it);
			it=next_it;
			continue;
		}
		it++;
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

	unconfirmed_priority_counts.clear();
}

void GeometryStreamingService::setOriginNode(avs::uid nodeID)
{
	originNodeId = nodeID;
}

bool GeometryStreamingService::addNode(avs::uid nodeID)
{
	if (nodeID != 0)
	{
		if(streamedNodeIDs.find(nodeID)==streamedNodeIDs.end())
		{
			streamedNodeIDs.insert(nodeID);
			auto node = geometryStore->getNode(nodeID);
			if(!node)
			{
				TELEPORT_WARN("Node {0} not found in GeometryStore.",nodeID);
				return false;
			}
			unconfirmed_priority_counts[node->priority]++;
			#if TELEPORT_DEBUG_NODE_STREAMING
			TELEPORT_COUT << "AddNode " << nodeID << " priority " << node->priority << ", count " << unconfirmed_priority_counts[node->priority]<<"\n";
			#endif
		}
	}
	return true;
}

void GeometryStreamingService::removeNode(avs::uid nodeID)
{
	if (streamedNodeIDs.find(nodeID) != streamedNodeIDs.end())
	{
		streamedNodeIDs.erase(nodeID);
		if(unconfirmedResourceTimes.find(nodeID)!=unconfirmedResourceTimes.end())
		{
			unconfirmedResourceTimes.erase(nodeID);
		
			auto node = geometryStore->getNode(nodeID);
			if(unconfirmed_priority_counts.find(node->priority)==unconfirmed_priority_counts.end())
			{
				TELEPORT_INTERNAL_BREAK_ONCE( "Trying to decrement Node {0} priority {1} but it's not in the list.",nodeID,node->priority);
			}
			else
			{
				unconfirmed_priority_counts[node->priority]--;
	#if TELEPORT_DEBUG_NODE_STREAMING
					TELEPORT_COUT << "removeNode " << nodeID << " priority " << node->priority << ", count " << unconfirmed_priority_counts[node->priority] << "\n";
	#endif
				if (unconfirmed_priority_counts[node->priority] == 0)
				{
					unconfirmed_priority_counts.erase(node->priority);
					TELEPORT_COUT << "Got all nodes of priority " << node->priority << "\n";
				}
			}
		}
	}
}

bool GeometryStreamingService::isStreamingNode(avs::uid nodeID)
{
	return streamedNodeIDs.find(nodeID) != streamedNodeIDs.end()||nodeID==originNodeId;
}

void GeometryStreamingService::addGenericTexture(avs::uid id)
{
	streamedGenericTextureUids.insert(id);
}

void GeometryStreamingService::GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources,int32_t minimumPriority) const
{
	TELEPORT_PROFILE_AUTOZONE;
	if (node.data_type != avs::NodeDataType::Mesh)
	{
		return;
	}
	// Don't send if not needed by this particular client.
	if(node.priority<minimumPriority)
	{
		return;
	}
	if(node.data_uid==0)
	{
		TELEPORT_WARN_NOSPAM("No mesh uid for node {0} {1}",nodeID,node.name);
	}
	avs::MeshNodeResources meshNode;
	meshNode.node_uid = nodeID;
	meshNode.mesh_uid = node.data_uid;
	//meshNode.skeletonID = node.skeletonNodeID;

	//Get joint/bone IDs, if the skeletonID is not zero.
	if (node.data_uid != 0&&node.data_type==avs::NodeDataType::Skeleton)
	{
		avs::Skeleton* skeleton = geometryStore->getSkeleton(node.data_uid, getClientAxesStandard());
		meshNode.boneIDs = skeleton->boneIDs;
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

void GeometryStreamingService::GetSkeletonNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources> &outMeshNodeResources) const
{
	TELEPORT_PROFILE_AUTOZONE;
	if (node.data_type != avs::NodeDataType::Skeleton)
	{
		return;
	}
	avs::MeshNodeResources sk;
	
	sk.node_uid = nodeID;
	sk.skeletonAssetID = node.data_uid;

	// Get joint/bone IDs, if the skeletonID is not zero.
	if (node.data_uid != 0)
	{
		avs::Skeleton* skeleton = geometryStore->getSkeleton(node.data_uid, getClientAxesStandard());
		sk.boneIDs = skeleton->boneIDs;
	}

	sk.animationIDs = node.animations;
	outMeshNodeResources.push_back(sk);
}