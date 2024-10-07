#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "libavstream/common.hpp"

#include "TeleportCore/Profiling.h"
#include "TeleportCore/Logging.h"
#include "TeleportServer/CaptureDelegates.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/DefaultHTTPService.h"
#include "TeleportServer/GeometryStore.h"
#include "TeleportServer/GeometryStreamingService.h"
#include "TeleportServer/AudioEncodePipeline.h"
#include "TeleportServer/VideoEncodePipeline.h"
#include "TeleportServer/ClientManager.h"

#include "TeleportServer/Export.h"
#include "TeleportServer/InteropStructures.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportAudio/CustomAudioStreamTarget.h"
#pragma optimize("", off)

using namespace teleport;
using namespace server;


TELEPORT_EXPORT void Client_SetClientInputDefinitions(avs::uid clientID, int numControls, const char** controlPaths,const InputDefinitionInterop *inputDefinitions)
{
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to set Input definitions to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	if (numControls > 0 && (controlPaths==nullptr))
	{
		TELEPORT_CERR << "Failed to set Input definitions to Client " << clientID << ". Null pointer!\n";
		return;
	}
	if ( inputDefinitions==nullptr)
	{
		TELEPORT_CERR << "Failed to set Input definitions to Client " << clientID << ". Null pointer!\n";
		return;
	}
	if (numControls < 0 || numControls>2000)
	{
		TELEPORT_CERR << "Failed to set Input definitions to Client " << clientID << ". Bad number!\n";
		return;
	}
	std::vector<teleport::core::InputDefinition> inputDefs;
	inputDefs.resize(numControls);
	for (int i = 0; i < numControls; i++)
	{
		inputDefs[i].inputId = inputDefinitions[i].inputId;
		inputDefs[i].inputType = inputDefinitions[i].inputType;
		if(!controlPaths[i])
		{
			TELEPORT_CERR << "Failed to set Input definitions to Client " << clientID << ". Bad path string!\n";
			return;
		}
		inputDefs[i].regexPath = controlPaths[i];
	}
	client->setInputDefinitions(inputDefs);
}

TELEPORT_EXPORT void Client_SetClientSettings(avs::uid clientID,const ClientSettings &clientSettings)
{
	size_t sz=sizeof(ClientSettings);
	TELEPORT_INTERNAL_COUT("sizeof ClientSettings is {0}", sz);
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to set clientSettings to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientManager::instance().SetClientSettings(clientID,clientSettings);
}

TELEPORT_EXPORT void Client_SetClientDynamicLighting(avs::uid clientID, const teleport::core::ClientDynamicLighting &clientDynamicLighting)
{
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to set clientDynamicLighting to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	client->clientDynamicLighting = clientDynamicLighting;
}

TELEPORT_EXPORT void Client_SetGlobalIlluminationTextures(avs::uid clientID,size_t num,const avs::uid * textureIDs)
{
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Client_SetGlobalIlluminationTexture: No client exists with ID " << clientID << "!\n";
		return;
	}
	client->setGlobalIlluminationTextures(num,textureIDs);
}

TELEPORT_EXPORT void Client_StopSession(avs::uid clientID)
{
	// Early-out if a client with this ID doesn't exist.
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to stop session to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	// Shut-down connections to the client.
	if (client->GetConnectionState() != UNCONNECTED)
	{
		// Will add to lost clients and call shutdown command
		ClientManager::instance().stopClient(clientID);
	}

	ClientManager::instance().removeLostClient(clientID);
}
TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID)
{
	ClientManager::instance().stopClient(clientID);
}

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,avs::uid originNode)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to set client origin of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	static uint64_t validCounter = 0;
	validCounter++;
	return client->clientMessaging->setOrigin( originNode);
}

TELEPORT_EXPORT bool Client_IsConnected(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		//TELEPORT_CERR << "Failed to check Client " << clientID << " is connected! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return true;
}

TELEPORT_EXPORT bool Client_HasOrigin(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to check Client " << clientID << " has origin! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return client->clientMessaging->hasOrigin();
}

//! Add the specified texture to be sent to the client.
TELEPORT_EXPORT void Client_AddGenericTexture(avs::uid clientID, avs::uid textureID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to start streaming Texture " << textureID << " to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	client->clientMessaging->GetGeometryStreamingService().addGenericTexture(textureID);
}
	
//! How many nodes have been marked for streaming to this client? Including lower priority ones not yet sent.
TELEPORT_EXPORT size_t Client_GetNumNodesToStream(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_WARN("No such client {0}!",clientID);
		return 0;
	}
	return client->clientMessaging->GetGeometryStreamingService().getNodesToStream().size();
}

//! How many nodes are actually being streamed to this client? Includes only those of sufficient priority, so always <= Client_GetNumNodesToStream().
TELEPORT_EXPORT size_t Client_GetNumNodesCurrentlyStreaming(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_WARN("No such client {0}!",clientID);
		return 0;
	}
	return client->clientMessaging->GetGeometryStreamingService().getStreamedNodeIDs().size();
}

TELEPORT_EXPORT bool Client_IsStreamingNodeID(avs::uid clientID, avs::uid nodeID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to check if Node_" << nodeID << "exists! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return client->clientMessaging->GetGeometryStreamingService().isStreamingNode(nodeID);
}

TELEPORT_EXPORT bool Client_IsClientRenderingNodeID(avs::uid clientID, avs::uid nodeID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " is rendering Node_" << nodeID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return true;//client->clientMessaging->GetGeometryStreamingService().isClientRenderingNode(nodeID);
}

bool Client_HasResource(avs::uid clientID, avs::uid resourceID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has Resource_" << resourceID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return client->clientMessaging->GetGeometryStreamingService().hasResource(resourceID);
}
///GeometryStreamingService END

#ifndef CLIENTMESSAGING 

//! Start streaming the node to the client; returns the number of nodes streamed currently after this addition.
TELEPORT_EXPORT bool Client_StreamNode(avs::uid clientID, avs::uid nodeID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_WARN("Failed to start streaming Node_{0} to Client {1}! No such client exists.",nodeID,clientID);
		return false;
	}

	return client->clientMessaging->GetGeometryStreamingService().streamNode(nodeID);
}


TELEPORT_EXPORT bool Client_UnstreamNode(avs::uid clientID, avs::uid nodeID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to stop streaming Node_" << nodeID << " to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return client->clientMessaging->GetGeometryStreamingService().unstreamNode(nodeID);
}

TELEPORT_EXPORT void Client_UpdateNodeMovement(avs::uid clientID, teleport::core::MovementUpdate* updates, int numUpdates)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to update node movement for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::MovementUpdate> updateList(numUpdates);
	auto axesStandard = client->clientMessaging->getClientNetworkContext()->axesStandard;
	if(axesStandard==avs::AxesStandard::NotInitialized)
		return;
	for(int i = 0; i < numUpdates; i++)
	{
		updateList[i] = updates[i];

		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].position);
		avs::ConvertRotation(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].rotation);
		avs::ConvertScale	(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].scale);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].velocity);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].angularVelocityAxis);
	}

	client->clientMessaging->updateNodeMovement(updateList);
}

TELEPORT_EXPORT void Client_UpdateNodeEnabledState(avs::uid clientID, teleport::core::NodeUpdateEnabledState* updates, int numUpdates)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to update enabled state for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::NodeUpdateEnabledState> updateList(updates, updates + numUpdates);
	client->clientMessaging->updateNodeEnabledState(updateList);
}

TELEPORT_EXPORT void Client_UpdateNodeAnimation(avs::uid clientID, teleport::core::ApplyAnimation update)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to update node animation for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	client->clientMessaging->updateNodeAnimation(update);
}


TELEPORT_EXPORT void Client_UpdateNodeRenderState(avs::uid clientID, avs::NodeRenderState update)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to update node animation control for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	client->clientMessaging->updateNodeRenderState(clientID,update);
}

TELEPORT_EXPORT void Client_SetNodeAnimationSpeed(avs::uid clientID, avs::uid nodeID, avs::uid animationID, float speed)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to set node animation speed for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	client->clientMessaging->setNodeAnimationSpeed(nodeID, animationID, speed);
}

TELEPORT_EXPORT void Client_SetNodeHighlighted(avs::uid clientID, avs::uid nodeID, bool isHighlighted)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to set node highlighting for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	client->clientMessaging->setNodeHighlighted(nodeID, isHighlighted);
}

TELEPORT_EXPORT void Client_ReparentNode(avs::uid clientID, avs::uid nodeID, avs::uid newParentNodeID,teleport::core::Pose relPose )
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "No client exists with ID " << clientID << "!\n";
		return;
	}
	// TODO: don't do this in a client function, it's global:
	GeometryStore::GetInstance().setNodeParent(nodeID,newParentNodeID, relPose);
	client->reparentNode(nodeID);
}

TELEPORT_EXPORT void Client_SetNodePosePath(avs::uid clientID, avs::uid nodeID, const char* regexPath)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "No client exists with ID " << clientID << "!\n";
		return;
	}
	client->setNodePosePath(nodeID,regexPath?regexPath:"");
}

TELEPORT_EXPORT bool Client_HasHost(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has host! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return !ClientManager::instance().hasHost() && ClientManager::instance().hasClient(clientID);
}

TELEPORT_EXPORT bool Client_HasPeer(avs::uid clientID)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if(!client)
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has peer! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return true;
}

TELEPORT_EXPORT unsigned int Client_GetSignalingPath(avs::uid clientID, unsigned int bufferLength, char *lpBuffer)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto c = ClientManager::instance().signalingService.getSignalingClient(clientID);
	if(c)
	{
		size_t final_len = std::min(static_cast<size_t>(bufferLength), c->path.length());
		if (final_len == c->path.length())
		{
			memcpy(reinterpret_cast<void *>(lpBuffer), c->path.c_str(), final_len);
			lpBuffer[final_len] = 0;
		}
		return static_cast<unsigned int>(c->path.length());
	}
	if(lpBuffer)
		lpBuffer[0] = 0;
	return 0;
}

TELEPORT_EXPORT unsigned int Client_GetClientIP(avs::uid clientID, unsigned int bufferLength,  char* lpBuffer)
{
	TELEPORT_PROFILE_AUTOZONE;
	static std::string str;

	auto client = ClientManager::instance().GetClient(clientID);
	if(client)
	{
		str = client->clientMessaging->getClientIP();
	}
	else
	{
		TELEPORT_CERR << "Failed to retrieve IP of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		str = "";
	}

	size_t final_len = std::min(static_cast<size_t>(bufferLength), str.length());
	if(final_len > 0)
	{
		memcpy(reinterpret_cast<void*>(lpBuffer), str.c_str(), final_len);
		lpBuffer[final_len] = 0;
	}
	return static_cast<unsigned int>(final_len);
}

TELEPORT_EXPORT bool Client_GetClientNetworkStats(avs::uid clientID, avs::NetworkSinkCounters& counters)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	static bool failed=false;
	if (!client)
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	if(failed)
	{
		TELEPORT_COUT << "Retrieved network stats of Client " << clientID << ".\n";
		failed=false;
	}
	// Thread safe
	client->clientMessaging->getClientNetworkContext()->NetworkPipeline.getCounters(counters);

	return true;
}
TELEPORT_EXPORT bool Client_GetClientDisplayInfo(avs::uid clientID, avs::DisplayInfo& displayInfo)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	static bool failed = false;
	if (!client)
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	if (failed)
	{
		TELEPORT_COUT << "Retrieved network stats of Client " << clientID << ".\n";
		failed = false;
	}
	// Thread safe
	displayInfo=client->clientMessaging->getDisplayInfo();

	return true;
}

TELEPORT_EXPORT bool Client_GetClientVideoEncoderStats(avs::uid clientID, avs::EncoderStats& stats)
{
	TELEPORT_PROFILE_AUTOZONE;
	auto client = ClientManager::instance().GetClient(clientID);
	if (!client)
	{
		TELEPORT_CERR << "Failed to retrieve video encoder stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	if (!client->videoEncodePipeline)
	{
		TELEPORT_CERR << "Failed to retrieve video encoder stats of Client " << clientID << "! VideoEncoderPipeline is null!\n";
		return false;
	}

	// Thread safe
	stats = client->videoEncodePipeline->getEncoderStats();

	return true;
}
#endif //ClientMessaging END

void Client_ProcessAudioInput(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	TELEPORT_PROFILE_AUTOZONE;
	processAudioInput(clientID, data, dataSize);
}


TELEPORT_EXPORT bool Client_GetNetworkState(avs::uid clientID,core::ClientNetworkState &st)
{
	TELEPORT_PROFILE_AUTOZONE;
	st = {};
	auto client = ClientManager::instance().GetClient(clientID);
	auto c= ClientManager::instance().signalingService.getSignalingClient(clientID);
	if (!c)
	{
		TELEPORT_CERR << "Failed to retrieve state of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	st.signalingState = c->signalingState;
	st.streamingConnectionState=client->clientMessaging->getStreamingState();
	return true;
}