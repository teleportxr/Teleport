#include "GeometryStreamingService.h"

#include <iostream>

#include "TeleportServer/ServerSettings.h"
#include "GeometryStore.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/TextCanvas.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/Profiling.h"
#include "TeleportServer/ClientManager.h"
#include "TeleportServer/ClientMessaging.h"
#pragma optimize("", off)

using namespace teleport;
using namespace server;
static const int32_t UNKNOWN_PRIORITY=-100000;
static const int32_t MAXIMUM_PRIORITY=INT_MAX;
ClientStoppedRenderingNodeFn GeometryStreamingService::callback_clientStoppedRenderingNode=nullptr;
ClientStartedRenderingNodeFn GeometryStreamingService::callback_clientStartedRenderingNode=nullptr;

#if TELEPORT_DEBUG_RESOURCE_STREAMING
#define CHECK_TEXTURE(t)\
	if(!geometryStore->getTexture(t))\
	{\
		TELEPORT_WARN("Missing texture {0}",t);\
		DEBUG_BREAK_ONCE;\
	}
#else
#define CHECK_TEXTURE(t)
#endif
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
	auto &r=GetTrackedResource(resource_uid);
	return r.sent;
}

void GeometryStreamingService::encodedResource(avs::uid resource_uid)
{
	auto &r=GetTrackedResource(resource_uid);
	r.sent= true;
	r.sent_server_time_us=clientMessaging.GetServerTimeUs();
}

void GeometryStreamingService::confirmResource(avs::uid resource_uid)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto &r=GetTrackedResource(resource_uid);
	r.acknowledged=true;
	if(!r.clientNeeds)
	{
		// Confirmed a resource we don't want it to have...
		TELEPORT_WARN("Confirmed an unwanted resource {0}",resource_uid);
		return;
	}
	if(!r.sent)
	{
		// Confirmed a resource we don't want it to have...
		TELEPORT_WARN("Confirmed an unsent resource {0}",resource_uid);
		return;
	}
	
	// Is this resource a node?
	if(auto node=geometryStore->getNode(resource_uid))
	{
		int32_t priority=getPriorityForNode(resource_uid);
		if (streamedNodes.find(resource_uid) == streamedNodes.end())
		{
			TELEPORT_WARN("Confirmed an unwanted node {0}",resource_uid);
			// this node wasn't meant to be sent. Ignore this, it CAN happen e.g. if 
			// the client is out-of-date with the current state.
		}
		else if (unconfirmed_priority_counts.find(priority) == unconfirmed_priority_counts.end())
		{
			TELEPORT_WARN("GeometryStreamingService::confirmResource Trying to decrement Node {0} priority {1} but it's not in the list.", resource_uid,node->priority);
			// This can happen if the node is in nodesToStream, but was not in streamedNodes. But the client already had it, or received it before it was removed.
		}
		else
		{
			unconfirmed_priority_counts[priority]--;
#if TELEPORT_DEBUG_RESOURCE_STREAMING
			TELEPORT_COUT << "GeometryStreamingService::confirmResource Node " << resource_uid << " priority " << node->priority << ", count " << unconfirmed_priority_counts[node->priority] << "\n";
#endif
			if(unconfirmed_priority_counts[priority]==0)
			{
				unconfirmed_priority_counts.erase(priority);
				TELEPORT_COUT<<"Got all nodes of priority "<<priority<<"\n";
			}
		}
	}
}

#define SEND_IF_UNSENT(uid,uid_set)\
	if(!GetTrackedResource(uid).sent)\
		uid_set.insert(uid);
// Get resources that have not yet been sent, or which need to be re-sent.
void GeometryStreamingService::updateResourcesToStream(int32_t minimumPriority)
{
	// Here we maintain the sets and maps of resources that we think the client NEEDS, taking into account priority.

	// If a node from nodesToStream has already been added to streamedNodes, we assume that its resources have already been added.

	// If a node from nodesToStream is not yet in streamedNodes, we check priority to see if it should be added.
	
	if(originNodeId)
		streamNode(originNodeId);
	int32_t lowest_confirmed_node_priority = -100000;
	// What is the lowest priority that has no unconfirmed nodes?
	// This unconfirmed_priority_counts is an ordered list of how many unconfirmed nodes each priority level has.
	// So the last value in that list gives the lowest priority we should stream.
	// When any value reaches zero, it's removed from the list.
	if(unconfirmed_priority_counts.size())
		lowest_confirmed_node_priority = unconfirmed_priority_counts.rbegin()->first;
	for(auto u:nodesToStream)
	{
		avs::Node* node = geometryStore->getNode(u);
		int32_t priority=getPriorityForNode(u);
		if(node==nullptr)
		{
			TELEPORT_WARN_NOSPAM("Null node {0}",u);
			continue;
		}
		if(streamedNodes.find(u)==streamedNodes.end())
		{
			// Not yet added.
			if(priority<lowest_confirmed_node_priority)
				continue;
			if (priority < minimumPriority&&u!=originNodeId)
				continue;
 			AddNodeAndItsResourcesToStreamed(u);
		}
	}
}
void GeometryStreamingService::RemoveNodeAndItsResourcesFromStreamed(avs::uid node_uid)
{
	AddNodeAndItsResourcesToStreamed(node_uid,true);
}

void GeometryStreamingService::AddNodeAndItsResourcesToStreamed(avs::uid node_uid,bool remove)
{
	avs::Node* node = geometryStore->getNode(node_uid);
	if (!node)
		return;
	int diff=remove?-1:1;
	TELEPORT_PROFILE_AUTOZONE;
	streamedNodes[node_uid]+=diff;
	
	std::vector<avs::MeshNodeResources> meshResources;
	switch (node->data_type)
	{
		case avs::NodeDataType::None:
		case avs::NodeDataType::Light:
			break;
		case avs::NodeDataType::Skeleton:
		{
			GetSkeletonNodeResources(node_uid, *node, meshResources);
		}
		break;
		case avs::NodeDataType::Mesh:
			{
				GetMeshNodeResources(node_uid, *node, meshResources);
				if(node->renderState.globalIlluminationUid>0)
				{
					streamedTextures[node->renderState.globalIlluminationUid]+=diff;
#if TELEPORT_DEBUG_RESOURCE_STREAMING
					if(!geometryStore->getTexture(node->renderState.globalIlluminationUid))
					{
						TELEPORT_WARN("Missing GI texture {0}",node->renderState.globalIlluminationUid);
					}
#endif
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
						streamedNodes[node->skeletonNodeID]+=diff;
						GetSkeletonNodeResources(node->skeletonNodeID, *skeletonnode, meshResources);
						for(auto r:meshResources)
						{
							for(auto b:r.boneIDs)
							{
								if(b)
									streamedNodes[b]+=diff;
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
						if(node->data_uid)
							streamedTextCanvases[node->data_uid]+=diff;
						if(c->font_uid)
							streamedFontAtlases[c->font_uid]+=diff;
						if(f->font_texture_uid)
							streamedTextures[f->font_texture_uid]+=diff;
					}
				}
			}
			break;
		default:
			break;
	}
	for(auto m:meshResources)
	{
		for(auto u:m.animationIDs)
		{
			streamedAnimations[u]+=diff;
		}
		for(auto u:m.boneIDs)
		{
			streamedBones[u]+=diff;
		}
		for(auto u:m.materials)
		{
			streamedMaterials[u.material_uid]+=diff;
			for(auto t:u.texture_uids)
			{
				streamedTextures[t]+=diff;
			}
		}
		if(m.mesh_uid)
		{
			streamedMeshes[m.mesh_uid]+=diff;
		}
		if(m.skeletonAssetID)
		{
			streamedSkeletons[m.skeletonAssetID]+=diff;
		}
	}
}

// Get resources that have not yet been sent, or which need to be re-sent.
void GeometryStreamingService::getResourcesToStream(std::set<avs::uid>& outNodeIDs
				,std::set<avs::uid>& genericTextureUids
				,std::set<avs::uid>& meshes
				,std::set<avs::uid>& materials
				,std::set<avs::uid>& textures
				,std::set<avs::uid>& skeletons
				,std::set<avs::uid>& bones
				,std::set<avs::uid>& animations
				,std::set<avs::uid>& textCanvases
				,std::set<avs::uid>& fontAtlases
				) const
{
// We have sets/maps of what the client SHOULD have, but some of these may have been sent already.
	int64_t time_now_us=clientMessaging.GetServerTimeUs();
	// ten seconds for timeout. Tweak this.
	static int64_t timeout_us=10000000;
// Start with nodes. The set of ALL the nodes of sufficient priority that the client NEEDS is
// streamedNodes.
	for(auto r:streamedNodes)
	{
		const TrackedResource &tr=GetTrackedResource(r.first);
		if(tr.acknowledged)
			continue;
		if(!tr.sent||time_now_us-tr.sent_server_time_us>timeout_us)
		{
			outNodeIDs.insert(r.first);
		}
	}
	auto stream=[&](const std::map<avs::uid,int> &resources,std::set<avs::uid>&targ){
		for(auto r:resources)
		{
			const TrackedResource &tr=GetTrackedResource(r.first);
			if(r.second<=0)
				continue;
			if(tr.acknowledged)
				continue;
			if(!tr.sent||time_now_us-tr.sent_server_time_us>timeout_us)
			{
				targ.insert(r.first);
			}
		}
	};
	stream(streamedMeshes,meshes);
	stream(streamedMaterials,materials);
	stream(streamedTextures,textures);
	stream(streamedSkeletons,skeletons);
	stream(streamedBones,bones);
	stream(streamedAnimations,animations);
	stream(streamedTextCanvases,textCanvases);
	stream(streamedFontAtlases,fontAtlases);
}

void GeometryStreamingService::getNodesToUpdateMovement(std::set<avs::uid>& nodes_to_update_movement,int64_t timestamp)
{
	// The nodes we have sent is nodesToStream.
	nodes_to_update_movement.clear();
	for(avs::uid u:nodesToStream)
	{
		avs::Node *avsNode=geometryStore->getNode(u);
		if(!avsNode->stationary)
			nodes_to_update_movement.insert(u);
	}
	// of these, which are non-stationary, and have been updated since the last timestamp?
	
	movement_update_timestamp=timestamp;
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
	auto n = streamedNodes.find(nodeID);
	if (n != streamedNodes.end())
	{
		bool result = clientStartedRenderingNode_Internal(clientId, nodeID);
		//if (result)
		//	clientRenderingNodes.insert(nodeID);
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
	//auto nodePair = clientRenderingNodes.find(nodeID);
	//if (nodePair != clientRenderingNodes.end())
	{
		bool res=clientStoppedRenderingNode_Internal(clientId, nodeID);
		//clientRenderingNodes.erase(nodeID);
		return res;
	}
	//else
	//{
	//	TELEPORT_COUT << "Client stopped rendering node with ID of " << nodeID << " - didn't know it was rendering this!\n";
	//	return false;
	//}
}

void GeometryStreamingService::tick(float deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
	// Might not be initialized... YET
	if (!avsPipeline || !serverSettings.enableGeometryStreaming)
		return;

	// For this client's POSITION and OTHER PROPERTIES,
	// Use the Geometry Source to determine which PipelineNode uid's are relevant.

	avsPipeline->process();
}

void GeometryStreamingService::reset()
{
	streamedNodes.clear();
	nodesToStream.clear();
	streamedNodes.clear();
	streamedGenericTextures.clear(); 
	streamedMeshes.clear();		
	streamedMaterials.clear();
	streamedTextures.clear();
	streamedSkeletons.clear();
	streamedBones.clear();
	streamedAnimations.clear();
	streamedTextCanvases.clear();
	streamedFontAtlases.clear();

	unconfirmed_priority_counts.clear();
}
const std::set<avs::uid>& GeometryStreamingService::getNodesToStream()
{
	return nodesToStream;
}
const std::set<avs::uid>& GeometryStreamingService::getStreamedNodeIDs()
{
	streamed_node_uids.clear();
	for(auto r:streamedNodes)
	{
		streamed_node_uids.insert(r.first);
	}
	return streamed_node_uids;
}

void GeometryStreamingService::setOriginNode(avs::uid nodeID)
{
	// The old origin node:  was it in nodesToStream? Was it unconfirmed? Then it affected unconfirmedPriorityCounts.
	originNodeId = nodeID;
	recalculateUnconfirmedPriorityCounts();
}

void GeometryStreamingService::updateNodePriority(avs::uid nodeID)
{
// For now, just surrender and recalculate all the priorities.
	recalculateUnconfirmedPriorityCounts();
}

void GeometryStreamingService::recalculateUnconfirmedPriorityCounts()
{
	unconfirmed_priority_counts.clear();
	for(auto nodeID:nodesToStream)
	{
		const auto &tr=trackedResources.find(nodeID);
		const auto &r=GetTrackedResource(nodeID);
		if(!r.acknowledged)
			unconfirmed_priority_counts[getPriorityForNode(nodeID)]++;
	}
}

int32_t GeometryStreamingService::getPriorityForNode(avs::uid nodeID) const
{
	if(nodeID==originNodeId)
		return MAXIMUM_PRIORITY;
	auto node = geometryStore->getNode(nodeID);
	if(!node)
	{
		TELEPORT_WARN("Node {0} not found in GeometryStore.",nodeID);
		return UNKNOWN_PRIORITY;
	}
	return node->priority;
}

bool GeometryStreamingService::streamNode(avs::uid nodeID)
{ 
	TELEPORT_LOG_INTERNAL("streamNode {0}",nodeID);
	if (nodeID != 0)
	{
		if(nodesToStream.find(nodeID)==nodesToStream.end())
		{
			nodesToStream.insert(nodeID);
			int priority=getPriorityForNode(nodeID);
			unconfirmed_priority_counts[getPriorityForNode(nodeID)]++;
			#if TELEPORT_DEBUG_RESOURCE_STREAMING
			//TELEPORT_COUT << "AddNode " << nodeID << " priority " << priority << ", count " << unconfirmed_priority_counts[priority]<<"\n";
			#endif
			return true;
		}
	}
	return false;
}

bool GeometryStreamingService::unstreamNode(avs::uid nodeID)
{
	TELEPORT_LOG_INTERNAL("unstreamNode {0}",nodeID);
	if (nodesToStream.find(nodeID) != nodesToStream.end())
	{
		nodesToStream.erase(nodeID);
		unconfirmed_priority_counts[getPriorityForNode(nodeID)]--;
		if(streamedNodes.find(nodeID)!=streamedNodes.end())
		{
			RemoveNodeAndItsResourcesFromStreamed(nodeID);
			return true;
		}
	}
	return false;
}

bool GeometryStreamingService::isStreamingNode(avs::uid nodeID)
{
	return streamedNodes.find(nodeID) != streamedNodes.end()||nodeID==originNodeId;
}

void GeometryStreamingService::addGenericTexture(avs::uid id)
{
#if TELEPORT_DEBUG_RESOURCE_STREAMING
	if(!geometryStore->getTexture(id))
	{
		TELEPORT_WARN("Missing generic texture {0}",id);
	}
#endif
	streamedGenericTextures.insert(id);
	streamedTextures[id]++;
}
void GeometryStreamingService::removeGenericTexture(avs::uid id)
{
	streamedGenericTextures.erase(id);
	streamedTextures[id]--;
}

const TrackedResource &GeometryStreamingService::GetTrackedResource(avs::uid u) const
{
	auto r=trackedResources.find(u);
	if(r==trackedResources.end())
	{
		static TrackedResource dummy;
		return dummy;
	}
	return *((r->second).get());
}

TrackedResource &GeometryStreamingService::GetTrackedResource(avs::uid u)
{
	auto r=trackedResources.find(u);
	if(r==trackedResources.end())
	{
		trackedResources[u]=std::make_shared<TrackedResource>();
		r=trackedResources.find(u);
	}
	return *((r->second).get());
}

void GeometryStreamingService::GetMeshNodeResources(avs::uid nodeID, const avs::Node& node, std::vector<avs::MeshNodeResources>& outMeshResources) const
{
	TELEPORT_PROFILE_AUTOZONE;
	if (node.data_type != avs::NodeDataType::Mesh)
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
		if(thisMaterial->pbrMetallicRoughness.baseColorTexture.index)
		{
			CHECK_TEXTURE(thisMaterial->pbrMetallicRoughness.baseColorTexture.index)
		}
		if(thisMaterial->pbrMetallicRoughness.metallicRoughnessTexture.index)
		{
			CHECK_TEXTURE(thisMaterial->pbrMetallicRoughness.metallicRoughnessTexture.index)
		}
		if(thisMaterial->emissiveTexture.index)
		{
			CHECK_TEXTURE(thisMaterial->emissiveTexture.index)
		}
		if(handshake.renderingFeatures.normals&&thisMaterial->normalTexture.index)
		{
			material.texture_uids.push_back(thisMaterial->normalTexture.index);
			CHECK_TEXTURE(thisMaterial->normalTexture.index)
		}
		if (handshake.renderingFeatures.ambientOcclusion&&thisMaterial->occlusionTexture.index)
		{
			material.texture_uids.push_back(thisMaterial->occlusionTexture.index);
			CHECK_TEXTURE(thisMaterial->occlusionTexture.index)
		}

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
//Clear a passed vector of UIDs that are believed to have already been sent to the client.
//	outUIDs : Vector of all UIDs of resources that could potentially need to be sent across.
//	req : Object that defines what needs to transfered across.
//Returns the size of the vector after having UIDs of already sent resources removed, and puts the new UIDs in the outUIDs vector.
size_t GeometryStreamingService::GetNewUIDs(std::vector<avs::uid>& outUIDs)
{
	//Remove uids the requester has.
	for (auto it = outUIDs.begin(); it != outUIDs.end();)
	{
		if (hasResource(*it))
		{
			it = outUIDs.erase(it);
		}
		else
		{
			++it;
		}
	}

	return outUIDs.size();
}