#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "TeleportServer/ServerSettings.h"
#include "TeleportServer/CaptureDelegates.h"
#include "TeleportServer/ClientData.h"
#include "TeleportServer/DiscoveryService.h"
#include "TeleportServer/DefaultHTTPService.h"
#include "TeleportServer/GeometryStore.h"
#include "TeleportServer/GeometryStreamingService.h"
#include "TeleportServer/AudioEncodePipeline.h"
#include "TeleportServer/VideoEncodePipeline.h"
#include "TeleportServer/ClientManager.h"

#include "Export.h"
#include "InteropStructures.h"
#include "PluginGraphics.h"
#include "PluginClient.h"
#include "PluginMain.h"
#include "TeleportCore/ErrorHandling.h"
#include "CustomAudioStreamTarget.h"

namespace teleport
{
	namespace server
	{
		std::mutex audioMutex;
		std::mutex videoMutex;
		std::map<avs::uid, ClientData> clientServices;

		ServerSettings serverSettings; //Engine-side settings are copied into this, so inner-classes can reference this rather than managed code instance.

		std::shared_ptr<DiscoveryService> discoveryService = std::make_shared<DiscoveryService>();
		std::unique_ptr<DefaultHTTPService> httpService = std::make_unique<DefaultHTTPService>();
		SetHeadPoseFn setHeadPose;
		SetControllerPoseFn setControllerPose;
		ProcessNewInputFn processNewInput;
		DisconnectFn onDisconnect;
		ProcessAudioInputFn processAudioInput;
		GetUnixTimestampFn getUnixTimestamp;
		ReportHandshakeFn reportHandshake;
		uint32_t connectionTimeout = 60000;

		 ClientManager clientManager;
	}
}

using namespace teleport;
using namespace server;


TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP,int discovery_port)
{
	if (!clientID || clientIP.size() == 0)
		return false;
	TELEPORT_COUT << "Started session for clientID" << clientID << " at IP "<<clientIP.c_str()<< std::endl;
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	//Check if we already have a session for a client with the passed ID.
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		std::shared_ptr<ClientMessaging> clientMessaging = std::make_shared<ClientMessaging>(&serverSettings, discoveryService,setHeadPose,  setControllerPose, processNewInput, onDisconnect, connectionTimeout, reportHandshake, &clientManager);
		ClientData newClientData(  clientMessaging);

		if(newClientData.clientMessaging->startSession(clientID, clientIP))
		{
			clientServices.emplace(clientID, std::move(newClientData));
		}
		else
		{
			TELEPORT_CERR << "Failed to start session for Client " << clientID << "!\n";
			return false;
		}
		clientPair = clientServices.find(clientID);
	}
	else
	{
		if (!clientPair->second.clientMessaging->isStartingSession() || clientPair->second.clientMessaging->timedOutStartingSession())
		{
			clientPair->second.clientMessaging->Disconnect();
			return false;
		}
		return true;
	}

	ClientData& newClient = clientPair->second;
	newClient.SetConnectionState(UNCONNECTED);
	if (enet_address_set_host_ip(&newClient.eNetAddress, clientIP.c_str()))
		return false;
	newClient.eNetAddress.port = discovery_port;
	if(newClient.clientMessaging->isInitialised())
	{
		newClient.clientMessaging->unInitialise();
	}
	newClient.clientMessaging->getClientNetworkContext()->Init(clientID,serverSettings.isReceivingAudio);


	///TODO: Initialize real delegates for capture component.
	CaptureDelegates delegates;
	delegates.startStreaming = [](ClientNetworkContext* context){};
	delegates.requestKeyframe = [&newClient]()
	{
		newClient.videoKeyframeRequired = true;
	};
	delegates.getClientCameraInfo = []()->CameraInfo&
	{
		static CameraInfo c;
		return c;
	};

	newClient.clientMessaging->initialize( delegates);

	discoveryService->sendResponseToClient(clientID);

	return true;
}

TELEPORT_EXPORT void Client_StopSession(avs::uid clientID)
{
	// Early-out if a client with this ID doesn't exist.
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop session to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	// Shut-down connections to the client.
	if(clientPair->second.isStreaming)
	{
		// Will add to lost clients and call shutdown command
		Client_StopStreaming(clientID);
	}

	RemoveClient(clientID);

	auto iter = lostClients.begin();
	while(iter != lostClients.end())
	{
		if(*iter == clientID)
		{
			// Continue checking rest of container just in case client ID was added more than once
			iter = lostClients.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

TELEPORT_EXPORT void Client_SetClientInputDefinitions(avs::uid clientID, int numControls, const char** controlPaths,const InputDefinitionInterop *inputDefinitions)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
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
	clientPair->second.setInputDefinitions(inputDefs);
}

TELEPORT_EXPORT void Client_SetClientSettings(avs::uid clientID,const ClientSettings &clientSettings)
{
	size_t sz=sizeof(ClientSettings);
	TELEPORT_INTERNAL_COUT("sizeof ClientSettings is {0}", sz);
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set clientSettings to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientData& clientData = clientPair->second;
	clientData.clientSettings = clientSettings;
	clientData.validClientSettings = true;
}
TELEPORT_EXPORT void Client_SetClientDynamicLighting(avs::uid clientID, const avs::ClientDynamicLighting &clientDynamicLighting)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set clientDynamicLighting to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientData& clientData = clientPair->second;
	clientData.clientDynamicLighting = clientDynamicLighting;
}

TELEPORT_EXPORT void Client_StartStreaming(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start streaming to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientData& clientData = clientPair->second;
	//not ready?
	if(!clientData.validClientSettings)
		return;

	clientData.clientMessaging->ConfirmSessionStarted();

	CasterEncoderSettings encoderSettings{};

	encoderSettings.frameWidth = clientData.clientSettings.videoTextureSize[0];
	encoderSettings.frameHeight = clientData.clientSettings.videoTextureSize[1];

	if (serverSettings.useAlphaLayerEncoding)
	{
		encoderSettings.depthWidth = 0;
		encoderSettings.depthHeight = 0;
	}
	else if (serverSettings.usePerspectiveRendering)
	{
		encoderSettings.depthWidth = static_cast<int32_t>(serverSettings.perspectiveWidth * 0.5f);
		encoderSettings.depthHeight = static_cast<int32_t>(serverSettings.perspectiveHeight * 0.5f);
	}
	else
	{
		encoderSettings.depthWidth = static_cast<int32_t>(serverSettings.captureCubeSize * 1.5f);
		encoderSettings.depthHeight = static_cast<int32_t>(serverSettings.captureCubeSize);
	}

	encoderSettings.wllWriteDepthTexture = false;
	encoderSettings.enableStackDepth = true;
	encoderSettings.enableDecomposeCube = true;
	encoderSettings.maxDepth = 10000;

	clientData.StartStreaming(serverSettings, encoderSettings,connectionTimeout,serverID,getUnixTimestamp, httpService->isUsingSSL());

}

TELEPORT_EXPORT void Client_SetGlobalIlluminationTextures(avs::uid clientID,size_t num,const avs::uid * textureIDs)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Client_SetGlobalIlluminationTexture: No client exists with ID " << clientID << "!\n";
		return;
	}
	ClientData& clientData = clientPair->second;
	clientData.setGlobalIlluminationTextures(num,textureIDs);
}

TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop streaming to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& lostClient = clientPair->second;
	lostClient.clientMessaging->stopSession();
	lostClient.isStreaming = false;

	//Delay deletion of clients.
	lostClients.push_back(clientID);
}

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,avs::uid originNode)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set client origin of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	ClientData& clientData = clientPair->second;
	static uint64_t validCounter = 0;
	validCounter++;
	return clientData.setOrigin(validCounter, originNode);
}

TELEPORT_EXPORT bool Client_IsConnected(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		//TELEPORT_CERR << "Failed to check Client " << clientID << " is connected! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;
	return clientData.isConnected();
}

TELEPORT_EXPORT bool Client_HasOrigin(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check Client " << clientID << " has origin! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;
	return clientData.hasOrigin();
}

//! Add the specified texture to be sent to the client.
TELEPORT_EXPORT void Client_AddGenericTexture(avs::uid clientID, avs::uid textureID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start streaming Texture " << textureID << " to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.clientMessaging->GetGeometryStreamingService().addGenericTexture(textureID);
}

//! Start streaming the node to the client; returns the number of nodes streamed currently after this addition.
TELEPORT_EXPORT size_t Client_AddNode(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to start streaming Node_" << nodeID << " to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return 0;
	}

	clientPair->second.clientMessaging->GetGeometryStreamingService().addNode(nodeID);
	return clientPair->second.clientMessaging->GetGeometryStreamingService().getStreamedNodeIDs().size();
}

TELEPORT_EXPORT void Client_RemoveNodeByID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to stop streaming Node_" << nodeID << " to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->GetGeometryStreamingService().removeNode(nodeID);
}

TELEPORT_EXPORT bool Client_IsStreamingNodeID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Node_" << nodeID << "exists! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return clientPair->second.clientMessaging->GetGeometryStreamingService().isStreamingNode(nodeID);
}

TELEPORT_EXPORT bool Client_IsClientRenderingNodeID(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " is rendering Node_" << nodeID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	return clientPair->second.clientMessaging->GetGeometryStreamingService().isClientRenderingNode(nodeID);
}

bool Client_HasResource(avs::uid clientID, avs::uid resourceID)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has Resource_" << resourceID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging->GetGeometryStreamingService().hasResource(resourceID);
}
///GeometryStreamingService END

///ClientMessaging START
TELEPORT_EXPORT void Client_NodeEnteredBounds(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to mark node as entering bounds for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->nodeEnteredBounds(nodeID);
}

TELEPORT_EXPORT void Client_NodeLeftBounds(avs::uid clientID, avs::uid nodeID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to mark node as leaving bounds for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->nodeLeftBounds(nodeID);
}

TELEPORT_EXPORT void Client_UpdateNodeMovement(avs::uid clientID, teleport::core::MovementUpdate* updates, int numUpdates)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node movement for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::MovementUpdate> updateList(numUpdates);
	auto axesStandard = clientPair->second.clientMessaging->getClientNetworkContext()->axesStandard;
	for(int i = 0; i < numUpdates; i++)
	{
		updateList[i] = updates[i];

		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].position);
		avs::ConvertRotation(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].rotation);
		avs::ConvertScale	(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].scale);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].velocity);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, axesStandard, updateList[i].angularVelocityAxis);
	}

	clientPair->second.clientMessaging->updateNodeMovement(updateList);
}

TELEPORT_EXPORT void Client_UpdateNodeEnabledState(avs::uid clientID, teleport::core::NodeUpdateEnabledState* updates, int numUpdates)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update enabled state for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::NodeUpdateEnabledState> updateList(updates, updates + numUpdates);
	clientPair->second.clientMessaging->updateNodeEnabledState(updateList);
}

TELEPORT_EXPORT void Client_UpdateNodeAnimation(avs::uid clientID, teleport::core::ApplyAnimation update)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node animation for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->updateNodeAnimation(update);
}

TELEPORT_EXPORT void Client_UpdateNodeAnimationControl(avs::uid clientID, teleport::core::NodeUpdateAnimationControl update)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node animation control for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->updateNodeAnimationControl(update);
}

TELEPORT_EXPORT void Client_UpdateNodeRenderState(avs::uid clientID, avs::NodeRenderState update)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node animation control for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.clientMessaging->updateNodeRenderState(clientID,update);
}

TELEPORT_EXPORT void Client_SetNodeAnimationSpeed(avs::uid clientID, avs::uid nodeID, avs::uid animationID, float speed)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set node animation speed for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.clientMessaging->setNodeAnimationSpeed(nodeID, animationID, speed);
}

TELEPORT_EXPORT void Client_SetNodeHighlighted(avs::uid clientID, avs::uid nodeID, bool isHighlighted)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set node highlighting for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->setNodeHighlighted(nodeID, isHighlighted);
}

TELEPORT_EXPORT void Client_ReparentNode(avs::uid clientID, avs::uid nodeID, avs::uid newParentNodeID,avs::Pose relPose )
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "No client exists with ID " << clientID << "!\n";
		return;
	}

	clientPair->second.clientMessaging->reparentNode(nodeID, newParentNodeID,relPose);
}

TELEPORT_EXPORT void Client_SetNodePosePath(avs::uid clientID, avs::uid nodeID, const char* regexPath)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.setNodePosePath(nodeID,regexPath?regexPath:"");
}

TELEPORT_EXPORT bool Client_HasHost(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has host! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return !clientManager.hasHost() && clientManager.hasClient(clientID);
}

TELEPORT_EXPORT bool Client_HasPeer(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check if Client " << clientID << " has peer! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging->hasPeer();
}

TELEPORT_EXPORT bool Client_SendCommand(avs::uid clientID, const teleport::core::Command& avsCommand)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to send command to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging->sendCommand(avsCommand);
}

TELEPORT_EXPORT bool Client_SendCommandWithList(avs::uid clientID, const teleport::core::Command& avsCommand, std::vector<avs::uid>& appendedList)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to send command to Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	return clientPair->second.clientMessaging->sendCommand(avsCommand, appendedList);
}

TELEPORT_EXPORT unsigned int Client_GetClientIP(avs::uid clientID, unsigned int bufferLength,  char* lpBuffer)
{
	static std::string str;

	auto clientPair = clientServices.find(clientID);
	if(clientPair != clientServices.end())
	{
		str = clientPair->second.clientMessaging->getClientIP();
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

TELEPORT_EXPORT uint16_t Client_GetClientPort(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve client port of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return 0;
	}
	return clientPair->second.clientMessaging->getClientPort();
}

TELEPORT_EXPORT uint16_t Client_GetServerPort(avs::uid clientID)
{
	return clientManager.getServerPort();
}

TELEPORT_EXPORT bool Client_GetClientNetworkStats(avs::uid clientID, avs::NetworkSinkCounters& counters)
{
	auto clientPair = clientServices.find(clientID);
	static bool failed=false;
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	
	ClientData& clientData = clientPair->second;
	if (!clientData.clientMessaging->hasPeer())
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client " << clientID << "! Client has no peer!\n";
		return false;
	}

	if(failed)
	{
		TELEPORT_COUT << "Retrieved network stats of Client " << clientID << ".\n";
		failed=false;
	}
	// Thread safe
	clientData.clientMessaging->getClientNetworkContext()->NetworkPipeline.getCounters(counters);

	return true;
}

TELEPORT_EXPORT bool Client_GetClientVideoEncoderStats(avs::uid clientID, avs::EncoderStats& stats)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve video encoder stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;
	if (!clientData.clientMessaging->hasPeer())
	{
		TELEPORT_CERR << "Failed to retrieve video encoder stats of Client " << clientID << "! Client has no peer!\n";
		return false;
	}

	if (!clientData.videoEncodePipeline)
	{
		TELEPORT_CERR << "Failed to retrieve video encoder stats of Client " << clientID << "! VideoEncoderPipeline is null!\n";
		return false;
	}

	// Thread safe
	stats = clientData.videoEncodePipeline->getEncoderStats();

	return true;
}
///ClientMessaging END

void Client_ProcessAudioInput(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	processAudioInput(clientID, data, dataSize);
}

TELEPORT_EXPORT avs::ConnectionState Client_GetConnectionState(avs::uid clientID)
{
	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve connection state of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return avs::ConnectionState::ERROR_STATE;
	}
	if(!clientPair->second.clientMessaging)
		return avs::ConnectionState::ERROR_STATE;
	return clientPair->second.clientMessaging->getConnectionState();
}