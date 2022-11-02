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
#include "TeleportServer/DefaultDiscoveryService.h"
#include "TeleportServer/DefaultHTTPService.h"
#include "TeleportServer/GeometryStore.h"
#include "TeleportServer/GeometryStreamingService.h"
#include "TeleportServer/AudioEncodePipeline.h"
#include "TeleportServer/VideoEncodePipeline.h"
#include "TeleportServer/ClientManager.h"

#include "Export.h"
#include "InteropStructures.h"
#include "PluginGraphics.h"
#include "TeleportCore/ErrorHandling.h"
#include "CustomAudioStreamTarget.h"

//#include <OAIdl.h>	// for SAFE_ARRAY

#ifdef _MSC_VER
#include "../VisualStudioDebugOutput.h"
VisualStudioDebugOutput debug_buffer(true, nullptr, 128);
#endif

using namespace teleport;
using namespace server;

TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP);
TELEPORT_EXPORT void Client_StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void Client_StopSession(avs::uid clientID);
TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID);

TELEPORT_EXPORT void ConvertTransform(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::Transform &transform)
{
	avs::ConvertTransform(fromStandard,toStandard,transform);
}
TELEPORT_EXPORT void ConvertRotation(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec4 &rotation)
{
	avs::ConvertRotation(fromStandard,toStandard,rotation);
}
TELEPORT_EXPORT void ConvertPosition(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec3 &position)
{
	avs::ConvertPosition(fromStandard,toStandard,position);
}
TELEPORT_EXPORT void ConvertScale(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, avs::vec3 &scale)
{
	avs::ConvertScale(fromStandard,toStandard,scale);
}
TELEPORT_EXPORT int8_t ConvertAxis(avs::AxesStandard fromStandard, avs::AxesStandard toStandard, int8_t axis)
{
	return avs::ConvertAxis(fromStandard,toStandard,axis);
}

typedef void(__stdcall* ProcessAudioInputFn) (avs::uid uid, const uint8_t* data, size_t dataSize);


static avs::Context avsContext;

static std::shared_ptr<DefaultDiscoveryService> discoveryService = std::make_shared<DefaultDiscoveryService>();
static std::unique_ptr<DefaultHTTPService> httpService = std::make_unique<DefaultHTTPService>();
GeometryStore geometryStore;

std::map<avs::uid, ClientData> clientServices;

teleport::ServerSettings serverSettings; //Engine-side settings are copied into this, so inner-classes can reference this rather than managed code instance.

teleport::AudioSettings audioSettings;

static SetHeadPoseFn setHeadPose;
static SetControllerPoseFn setControllerPose;
static ProcessNewInputFn processNewInput;
static DisconnectFn onDisconnect;
static ProcessAudioInputFn processAudioInput;
static GetUnixTimestampFn getUnixTimestamp;
static ReportHandshakeFn reportHandshake;
static uint32_t connectionTimeout = 60000;
static avs::uid serverID = 0;

static std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.
static std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.

static std::mutex audioMutex;
static std::mutex videoMutex;

static ClientManager clientManager;

// Messages related stuff
avs::MessageHandlerFunc messageHandler = nullptr;
struct LogMessage
{
	avs::LogSeverity severity = avs::LogSeverity::Never;
	std::string msg;
	void* userData = nullptr;
};

static std::vector<LogMessage> messages(100);
static std::mutex messagesMutex;


struct InitialiseState
{
	char* clientIP;
	char* httpMountDirectory;
	char* certDirectory;
	char* privateKeyDirectory;
	uint32_t DISCOVERY_PORT = 10607;
	uint32_t SERVICE_PORT = 10500;

	ClientStoppedRenderingNodeFn clientStoppedRenderingNode;
	ClientStartedRenderingNodeFn clientStartedRenderingNode;
	SetHeadPoseFn headPoseSetter;
	SetControllerPoseFn controllerPoseSetter;
	ProcessNewInputFn newInputProcessing;
	DisconnectFn disconnect;
	avs::MessageHandlerFunc messageHandler;
	ReportHandshakeFn reportHandshake;
	ProcessAudioInputFn processAudioInput;
	GetUnixTimestampFn getUnixTimestamp;
};

///PLUGIN-INTERNAL START
void RemoveClient(avs::uid clientID)
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	// Early-out if a client with this ID doesn't exist.
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to remove client from server! No client exists with ID " << clientID << "!\n";
		return;
	}
	clientPair->second.clientMessaging->stopSession();

	// Remove references to lost client.
	clientServices.erase(clientID);
}

void ProcessAudioInput(avs::uid clientID, const uint8_t* data, size_t dataSize)
{
	processAudioInput(clientID, data, dataSize);
}

///PLUGIN-INTERNAL END

///MEMORY-MANAGEMENT START
TELEPORT_EXPORT void DeleteUnmanagedArray(void** unmanagedArray)
{
	delete[] (uint8_t*)*unmanagedArray;
}
///MEMORY-MANAGEMENT END

///PLUGIN-SPECIFIC START
TELEPORT_EXPORT void UpdateServerSettings(const teleport::ServerSettings newSettings)
{
	serverSettings = newSettings;
}

TELEPORT_EXPORT bool SetCachePath(const char* path)
{
	return geometryStore.SetCachePath(path);
}

TELEPORT_EXPORT void SetClientStoppedRenderingNodeDelegate(ClientStoppedRenderingNodeFn clientStoppedRenderingNode)
{
	PluginGeometryStreamingService::callback_clientStoppedRenderingNode = clientStoppedRenderingNode;
}

TELEPORT_EXPORT void SetClientStartedRenderingNodeDelegate(ClientStartedRenderingNodeFn clientStartedRenderingNode)
{
	PluginGeometryStreamingService::callback_clientStartedRenderingNode = clientStartedRenderingNode;
}

TELEPORT_EXPORT void SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
{
	setHeadPose = headPoseSetter;
}

TELEPORT_EXPORT void SetNewInputProcessingDelegate(ProcessNewInputFn newInputProcessing)
{
	processNewInput = newInputProcessing;
}

TELEPORT_EXPORT void SetDisconnectDelegate(DisconnectFn disconnect)
{
	onDisconnect = disconnect;
}

TELEPORT_EXPORT void SetProcessAudioInputDelegate(ProcessAudioInputFn f)
{
	processAudioInput = f;
}

TELEPORT_EXPORT void SetGetUnixTimestampDelegate(GetUnixTimestampFn function)
{
	getUnixTimestamp = function;
}

static void passOnOutput(const char *msg)
{
	if(msg)
		avsContext.log(avs::LogSeverity::Info,msg);
}

static void passOnError(const char *msg)
{
	if(msg)
		avsContext.log(avs::LogSeverity::Error,msg);
}

void AccumulateMessagesFromThreads(avs::LogSeverity severity, const char* msg, void* userData)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if(severity==avs::LogSeverity::Error|| severity==avs::LogSeverity::Critical)
	{
		LogMessage tst={severity,msg,userData};
		// can break here.
	}
	if(messages.size()==99)
	{
		LogMessage logMessage={avs::LogSeverity::Error,"Too many messages since last call to PipeOutMessages()",nullptr};
		messages.push_back(std::move(logMessage));
		return;
	}
	else if(messages.size()>99)
	{
		return;
	}
	LogMessage logMessage={severity,msg,userData};
	messages.push_back(std::move(logMessage));
}

void PipeOutMessages()
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if(messageHandler)
	{
		for(LogMessage& message : messages)
		{
			messageHandler(message.severity, message.msg.c_str(), message.userData);
		}
		messages.clear();
	}
}

TELEPORT_EXPORT void SetMessageHandlerDelegate(avs::MessageHandlerFunc msgh)
{
	if(msgh)
	{
		debug_buffer.setToOutputWindow(false);
		messageHandler=msgh;
		avsContext.setMessageHandler(AccumulateMessagesFromThreads, nullptr); 
		debug_buffer.setOutputCallback(&passOnOutput);
		debug_buffer.setErrorCallback(&passOnError);
	}
	else
	{
		debug_buffer.setToOutputWindow(true);
		messageHandler=nullptr;
		avsContext.setMessageHandler(nullptr, nullptr); 
		debug_buffer.setOutputCallback(nullptr);
		debug_buffer.setErrorCallback(nullptr);
	}
}

TELEPORT_EXPORT void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}

TELEPORT_EXPORT bool Teleport_Initialize(const InitialiseState *initialiseState)
{
	unlinkedClientIDs.clear();

	serverID = avs::GenerateUid();

	SetClientStoppedRenderingNodeDelegate(initialiseState->clientStoppedRenderingNode);
	SetClientStartedRenderingNodeDelegate(initialiseState->clientStartedRenderingNode);
	SetHeadPoseSetterDelegate(initialiseState->headPoseSetter);

	setControllerPose = initialiseState->controllerPoseSetter;
	SetNewInputProcessingDelegate(initialiseState->newInputProcessing);
	SetDisconnectDelegate(initialiseState->disconnect);
	SetMessageHandlerDelegate(initialiseState->messageHandler);
	SetProcessAudioInputDelegate(initialiseState->processAudioInput);
	SetGetUnixTimestampDelegate(initialiseState->getUnixTimestamp);

	reportHandshake=initialiseState->reportHandshake;

	if(enet_initialize() != 0)
	{
		TELEPORT_CERR<<"An error occurred while attempting to initalise ENet!\n";
		return false;
	}
	atexit(enet_deinitialize);

	bool result = discoveryService->initialize(initialiseState->DISCOVERY_PORT,initialiseState->SERVICE_PORT, std::string(initialiseState->clientIP));

	if (!result)
	{
		return false;
	}

	result = clientManager.initialize(initialiseState->SERVICE_PORT);

	if (!result)
	{
		return false;
	}

	clientManager.startAsyncNetworkDataProcessing();

	result = httpService->initialize(initialiseState->httpMountDirectory, initialiseState->certDirectory, initialiseState->privateKeyDirectory, initialiseState->SERVICE_PORT + 1);
	return result;
}

TELEPORT_EXPORT void Shutdown()
{
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	clientManager.stopAsyncNetworkDataProcessing(true);

	for(auto& clientPair : clientServices)
	{
		ClientData& clientData = clientPair.second;
		if(clientData.isStreaming)
		{
			// This will add to lost clients and lost clients will be cleared below.
			// That's okay because the session is being stopped in Client_StopStreaming 
			// and the clientServices map is being cleared below too.
			Client_StopStreaming(clientPair.first);
		}
		else
		{
			clientData.clientMessaging->stopSession();
		}
	}

	clientManager.shutdown();
	httpService->shutdown();
	discoveryService->shutdown();

	lostClients.clear();
	unlinkedClientIDs.clear();
	clientServices.clear();

	PluginGeometryStreamingService::callback_clientStoppedRenderingNode = nullptr;
	PluginGeometryStreamingService::callback_clientStartedRenderingNode = nullptr;

	setHeadPose = nullptr;
	setControllerPose = nullptr;
	processNewInput = nullptr;
}

TELEPORT_EXPORT bool Client_StartSession(avs::uid clientID, std::string clientIP)
{
	if (!clientID || clientIP.size() == 0)
		return false;
	std::lock_guard<std::mutex> videoLock(videoMutex);
	std::lock_guard<std::mutex> audioLock(audioMutex);

	//Check if we already have a session for a client with the passed ID.
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		std::shared_ptr<teleport::ClientMessaging> clientMessaging = std::make_shared<teleport::ClientMessaging>(&serverSettings, discoveryService,setHeadPose,  setControllerPose, processNewInput, onDisconnect, connectionTimeout, reportHandshake, &clientManager);
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
	if(newClient.clientMessaging->isInitialised())
	{
		newClient.clientMessaging->unInitialise();
	}

	// Sending
	newClient.casterContext.ColorQueue.reset(new avs::Queue);
	newClient.casterContext.TagDataQueue.reset(new avs::Queue);
	newClient.casterContext.GeometryQueue.reset(new avs::Queue);
	newClient.casterContext.AudioQueue.reset(new avs::Queue);

	newClient.casterContext.ColorQueue->configure(200000, 16,"ColorQueue");
	newClient.casterContext.TagDataQueue->configure(200, 16, "TagDataQueue");
	newClient.casterContext.GeometryQueue->configure(200000, 16, "GeometryQueue");
	newClient.casterContext.AudioQueue->configure(8192, 120, "AudioQueue");

	// Receiving
	if (serverSettings.isReceivingAudio)
	{
		newClient.casterContext.sourceAudioQueue.reset(new avs::Queue);
		newClient.casterContext.audioDecoder.reset(new avs::AudioDecoder); 
		newClient.casterContext.audioTarget.reset(new avs::AudioTarget); 
		newClient.casterContext.audioStreamTarget.reset(new sca::CustomAudioStreamTarget(std::bind(&ProcessAudioInput, clientID, std::placeholders::_1, std::placeholders::_2)));

		newClient.casterContext.sourceAudioQueue->configure( 8192, 120, "SourceAudioQueue");
		newClient.casterContext.audioDecoder->configure(100);
		newClient.casterContext.audioTarget->configure(newClient.casterContext.audioStreamTarget.get());
	}

	///TODO: Initialize real delegates for capture component.
	teleport::CaptureDelegates delegates;
	delegates.startStreaming = [](teleport::CasterContext* context){};
	delegates.requestKeyframe = [&newClient]()
	{
		newClient.videoKeyframeRequired = true;
	};
	delegates.getClientCameraInfo = []()->teleport::CameraInfo&
	{
		static teleport::CameraInfo c;
		return c;
	};

	newClient.clientMessaging->initialise(&newClient.casterContext, delegates);

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

TELEPORT_EXPORT void Client_SetClientSettings(avs::uid clientID, ClientSettings clientSettings)
{
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
TELEPORT_EXPORT void Client_SetClientDynamicLighting(avs::uid clientID, avs::ClientDynamicLighting clientDynamicLighting)
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

	//clientData.geometryStreamingService->startStreaming(&clientData.casterContext,handshake);

	teleport::CasterEncoderSettings encoderSettings{};

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

TELEPORT_EXPORT void Tick(float deltaTime)
{
	//Delete client data for clients who have been lost.
	for(avs::uid clientID : lostClients)
	{
		RemoveClient(clientID);
	}
	lostClients.clear();

	clientManager.tick(deltaTime);

	for(auto& clientPair : clientServices)
	{
		ClientData& clientData = clientPair.second;
		clientData.clientMessaging->handleEvents(deltaTime);

		if(clientData.clientMessaging->hasPeer())
		{
			if (clientData.isStreaming == false)
			{
				Client_StartStreaming(clientPair.first);
			}

			clientData.clientMessaging->tick(deltaTime);
		}
	}

	discoveryService->tick();
	PipeOutMessages();
}

TELEPORT_EXPORT void EditorTick()
{
	PipeOutMessages();
}

TELEPORT_EXPORT bool Client_SetOrigin(avs::uid clientID,uint64_t validCounter,avs::uid originNode, const avs::vec3* pos,const avs::vec4* orient)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to set client origin of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	ClientData& clientData = clientPair->second;
	return clientData.setOrigin(validCounter, originNode,*pos, *orient);
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

TELEPORT_EXPORT bool Client_HasOrigin(avs::uid clientID, avs::vec3* pos)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to check Client " << clientID << " has origin! No client exists with ID " << clientID << "!\n";
		return false;
	}

	ClientData& clientData = clientPair->second;

	bool result = (clientData.hasOrigin());
	if(result && pos)
	{
		*pos = clientData.getOrigin();
	}

	return result;
}

TELEPORT_EXPORT void Reset()
{
	for(auto& clientPair : clientServices)
	{
		clientPair.second.clientMessaging->GetGeometryStreamingService().reset();
	}
}

TELEPORT_EXPORT avs::uid GetUnlinkedClientID()
{
	if(unlinkedClientIDs.size() != 0)
	{
		avs::uid clientID = *unlinkedClientIDs.begin();
		unlinkedClientIDs.erase(unlinkedClientIDs.begin());

		return clientID;
	}
	else
	{
		return 0;
	}
}

TELEPORT_EXPORT void AddUnlinkedClientID(avs::uid clientID)
{
	unlinkedClientIDs.insert(clientID);
}
///PLUGIN-SPECIFC END

///libavstream START
TELEPORT_EXPORT avs::uid GenerateUid()
{
	return avs::GenerateUid();
}
///libavstream END

TELEPORT_EXPORT avs::uid GetOrGenerateUid(BSTR path)
{
	if(!path)
		return 0;
	_bstr_t p=path;
	std::string str(p);
	return geometryStore.GetOrGenerateUid(str);
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


///VideoEncodePipeline START
TELEPORT_EXPORT bool GetVideoEncodeCapabilities(avs::EncodeCapabilities& capabilities)
{
	VideoEncodeParams params;
	params.deviceHandle = GraphicsManager::mGraphicsDevice;

	switch (GraphicsManager::mRendererType)
	{
	case kUnityGfxRendererD3D11:
	{
		params.deviceType = GraphicsDeviceType::Direct3D11;
		break;
	}
	case kUnityGfxRendererD3D12:
	{
		params.deviceType = GraphicsDeviceType::Direct3D12;
		break;
	}
	case kUnityGfxRendererVulkan:
	{
		params.deviceType = GraphicsDeviceType::Vulkan;
		break;
	}
	default:
		return false;
	};

	if (teleport::VideoEncodePipeline::getEncodeCapabilities(serverSettings, params, capabilities))
	{
		return true;
	}

	return false;
}

TELEPORT_EXPORT void InitializeVideoEncoder(avs::uid clientID, teleport::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	avs::Queue* cq = clientData.casterContext.ColorQueue.get();
	avs::Queue* tq = clientData.casterContext.TagDataQueue.get();
	Result result = clientData.videoEncodePipeline->configure(serverSettings,videoEncodeParams, cq, tq);
	if(!result)
	{
		TELEPORT_CERR << "Failed to initialise video encoder for Client " << clientID << "! Error occurred when trying to configure the video encoder pipeline!\n";
	}
}

TELEPORT_EXPORT void ReconfigureVideoEncoder(avs::uid clientID, teleport::VideoEncodeParams& videoEncodeParams)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	Result result = clientData.videoEncodePipeline->reconfigure(serverSettings, videoEncodeParams);
	if (!result)
	{
		TELEPORT_CERR << "Failed to reconfigure video encoder for Client " << clientID << "! Error occurred when trying to reconfigure the video encoder pipeline!\n";
		return;
	}

	///TODO: Need to retrieve encoder settings from unity.
	teleport::CasterEncoderSettings encoderSettings
	{
		videoEncodeParams.encodeWidth,
		videoEncodeParams.encodeHeight,
		0, // not used
		0, // not used
		false,
		true,
		true,
		10000
	};
	teleport::core::ReconfigureVideoCommand cmd;
	avs::VideoConfig& videoConfig = cmd.video_config;
	videoConfig.video_width = encoderSettings.frameWidth;
	videoConfig.video_height = encoderSettings.frameHeight;
	videoConfig.depth_height = encoderSettings.depthHeight;
	videoConfig.depth_width = encoderSettings.depthWidth;
	videoConfig.perspective_width = serverSettings.perspectiveWidth;
	videoConfig.perspective_height = serverSettings.perspectiveHeight;
	videoConfig.perspective_fov = serverSettings.perspectiveFOV;
	videoConfig.use_10_bit_decoding = serverSettings.use10BitEncoding;
	videoConfig.use_yuv_444_decoding = serverSettings.useYUV444Decoding;
	videoConfig.use_alpha_layer_decoding = serverSettings.useAlphaLayerEncoding;
	videoConfig.colour_cubemap_size = serverSettings.captureCubeSize;
	videoConfig.compose_cube = encoderSettings.enableDecomposeCube;
	videoConfig.videoCodec = serverSettings.videoCodec;
	videoConfig.use_cubemap = !serverSettings.usePerspectiveRendering;

	clientData.clientMessaging->sendCommand(cmd);
}

TELEPORT_EXPORT void EncodeVideoFrame(avs::uid clientID, const uint8_t* tagData, size_t tagDataSize)
{
	std::lock_guard<std::mutex> lock(videoMutex);

	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	ClientData& clientData = clientPair->second;
	if(!clientData.clientMessaging->hasPeer())
	{
		TELEPORT_COUT << "Failed to encode video frame for Client " << clientID << "! Client has no peer!\n";
		return;
	}

	Result result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
	if(result)
	{
		clientData.videoKeyframeRequired = false;
	}
	else
	{
		TELEPORT_CERR << "Failed to encode video frame for Client " << clientID << "! Error occurred when trying to encode video!\n";

		// repeat the attempt for debugging purposes.
		result = clientData.videoEncodePipeline->encode(tagData, tagDataSize, clientData.videoKeyframeRequired);
		if(result)
		{
			clientData.videoKeyframeRequired = false;
		}
	}
}

struct EncodeVideoParamsWrapper
{
	avs::uid clientID;
	teleport::VideoEncodeParams videoEncodeParams;
};

static void UNITY_INTERFACE_API OnRenderEventWithData(int eventID, void* data)
{
	if (eventID == 0)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		InitializeVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 1)
	{
		auto wrapper = (EncodeVideoParamsWrapper*)data;
		ReconfigureVideoEncoder(wrapper->clientID, wrapper->videoEncodeParams);
	}
	else if (eventID == 2)
	{
		const auto buffer = (uint8_t*)data;

		avs::uid clientID;
		memcpy(&clientID, buffer, sizeof(avs::uid));

		uint32_t tagDataSize;
		memcpy(&tagDataSize, buffer + sizeof(avs::uid), sizeof(tagDataSize));

		const uint8_t* tagData = buffer + sizeof(avs::uid) + sizeof(size_t);
		
		EncodeVideoFrame(clientID, tagData, tagDataSize);
	}
	else
	{
		TELEPORT_CERR << "Unknown event id" << "\n";
	}
}

TELEPORT_EXPORT UnityRenderingEventAndData GetRenderEventWithDataCallback()
{
	return OnRenderEventWithData;
}
///VideoEncodePipeline END

///AudioEncodePipeline START
TELEPORT_EXPORT void SetAudioSettings(const teleport::AudioSettings& newAudioSettings)
{
	audioSettings = newAudioSettings;
}

TELEPORT_EXPORT void SendAudio(const uint8_t* data, size_t dataSize)
{
	// Only continue processing if the main thread hasn't hung.
	double elapsedTime = avs::PlatformWindows::getTimeElapsedInSeconds(clientManager.getLastTickTimestamp(), avs::PlatformWindows::getTimestamp());
	if (elapsedTime > 0.15f)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(audioMutex);

	for (auto& clientPair : clientServices)
	{
		const avs::uid& clientID = clientPair.first;
		ClientData& clientData = clientPair.second;
		if (!clientData.clientMessaging->hasPeer())
		{
			continue;
		}

		Result result = Result(Result::Code::OK);
		if (!clientData.audioEncodePipeline->isConfigured())
		{
			result = clientData.audioEncodePipeline->configure(serverSettings, audioSettings, clientData.casterContext.AudioQueue.get());
			if (!result)
			{
				TELEPORT_CERR << "Failed to configure audio encoder pipeline for Client " << clientID << "!\n";
				continue;
			}
		}

		result = clientData.audioEncodePipeline->sendAudio(data, dataSize);
		if (!result)
		{
			TELEPORT_CERR << "Failed to send audio to Client " << clientID << "! Error occurred when trying to send audio" << "\n";
		}
	}
}
///AudioEncodePipeline END

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

TELEPORT_EXPORT void Client_UpdateNodeMovement(avs::uid clientID, teleport::core::MovementUpdate* updates, int updateAmount)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update node movement for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::MovementUpdate> updateList(updateAmount);
	for(int i = 0; i < updateAmount; i++)
	{
		updateList[i] = updates[i];

		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].position);
		avs::ConvertRotation(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].rotation);
		avs::ConvertScale(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].scale);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].velocity);
		avs::ConvertPosition(avs::AxesStandard::UnityStyle, clientPair->second.casterContext.axesStandard, updateList[i].angularVelocityAxis);
	}

	clientPair->second.clientMessaging->updateNodeMovement(updateList);
}

TELEPORT_EXPORT void Client_UpdateNodeEnabledState(avs::uid clientID, teleport::core::NodeUpdateEnabledState* updates, int updateAmount)
{
	auto clientPair = clientServices.find(clientID);
	if(clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to update enabled state for Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return;
	}

	std::vector<teleport::core::NodeUpdateEnabledState> updateList(updates, updates + updateAmount);
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

TELEPORT_EXPORT const DWORD WINAPI Client_GetClientIP(avs::uid clientID, __in DWORD bufferLength, __out char* lpBuffer)
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
	return static_cast<DWORD>(final_len);
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
	if (clientPair == clientServices.end())
	{
		TELEPORT_CERR << "Failed to retrieve network stats of Client " << clientID << "! No client exists with ID " << clientID << "!\n";
		return false;
	}
	
	ClientData& clientData = clientPair->second;
	if (!clientData.clientMessaging->hasPeer())
	{
		TELEPORT_COUT << "Failed to retrieve network stats of Client " << clientID << "! Client has no peer!\n";
		return false;
	}

	if (!clientData.casterContext.NetworkPipeline)
	{
		TELEPORT_COUT << "Failed to retrieve network stats of Client " << clientID << "! NetworkPipeline is null!\n";
		return false;
	}
	
	// Thread safe
	clientData.casterContext.NetworkPipeline->getCounters(counters);

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
		TELEPORT_COUT << "Failed to retrieve video encoder stats of Client " << clientID << "! Client has no peer!\n";
		return false;
	}

	if (!clientData.videoEncodePipeline)
	{
		TELEPORT_COUT << "Failed to retrieve video encoder stats of Client " << clientID << "! VideoEncoderPipeline is null!\n";
		return false;
	}

	// Thread safe
	stats = clientData.videoEncodePipeline->getEncoderStats();

	return true;
}
///ClientMessaging END

///GeometryStore START
TELEPORT_EXPORT void SaveGeometryStore()
{
	geometryStore.saveToDisk();
	geometryStore.verify();
}

TELEPORT_EXPORT bool CheckGeometryStoreForErrors()
{
	return geometryStore.CheckForErrors();
}
TELEPORT_EXPORT void LoadGeometryStore(size_t* meshAmount, LoadedResource** meshes, size_t* textureAmount, LoadedResource** textures, size_t* materialAmount, LoadedResource** materials)
{
	geometryStore.loadFromDisk(*meshAmount, *meshes, *textureAmount, *textures, *materialAmount, *materials);
}

TELEPORT_EXPORT void ClearGeometryStore()
{
	geometryStore.clear(true);
}

TELEPORT_EXPORT void SetDelayTextureCompression(bool willDelay)
{
	geometryStore.willDelayTextureCompression = willDelay;
}

TELEPORT_EXPORT void SetCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
{
	geometryStore.setCompressionLevels(compressionStrength, compressionQuality);
}

TELEPORT_EXPORT void StoreNode(avs::uid id, InteropNode node)
{
	geometryStore.storeNode(id, avs::Node(node));
}

TELEPORT_EXPORT void StoreSkin(avs::uid id, InteropSkin skin)
{
	geometryStore.storeSkin(id, avs::Skin(skin), avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreTransformAnimation(avs::uid animationID, InteropTransformAnimation* animation)
{
	geometryStore.storeAnimation(animationID, avs::Animation(*animation), avs::AxesStandard::UnityStyle);
}

TELEPORT_EXPORT void StoreMesh(avs::uid id, BSTR guid, BSTR path, std::time_t lastModified, const InteropMesh* mesh, avs::AxesStandard extractToStandard, bool compress,bool verify)
{
	geometryStore.storeMesh(id, guid, path, lastModified, avs::Mesh(*mesh), extractToStandard,compress,verify);
}

TELEPORT_EXPORT void StoreMaterial(avs::uid id, BSTR guid, BSTR path, std::time_t lastModified, InteropMaterial material)
{
	geometryStore.storeMaterial(id, guid, path, lastModified, avs::Material(material));
}

TELEPORT_EXPORT void StoreTexture(avs::uid id, BSTR guid, BSTR path, std::time_t lastModified, InteropTexture texture, char* basisFileLocation,  bool genMips, bool highQualityUASTC, bool forceOverwrite)
{
	geometryStore.storeTexture(id, guid, path, lastModified, avs::Texture(texture), basisFileLocation,  genMips,  highQualityUASTC, forceOverwrite);
}

TELEPORT_EXPORT void StoreShadowMap(avs::uid id, BSTR guid, BSTR path, std::time_t lastModified, InteropTexture shadowMap)
{
	geometryStore.storeShadowMap(id, guid, path, lastModified, avs::Texture(shadowMap));
}

TELEPORT_EXPORT bool IsNodeStored(avs::uid id)
{
	const avs::Node* node = geometryStore.getNode(id);
	return node != nullptr;
}

TELEPORT_EXPORT bool IsSkinStored(avs::uid id)
{
	//NOTE: Assumes we always are storing animations in the engineering axes standard.
	const avs::Skin* skin = geometryStore.getSkin(id, avs::AxesStandard::EngineeringStyle);
	return skin != nullptr;
}

TELEPORT_EXPORT bool IsMeshStored(avs::uid id)
{
	//NOTE: Assumes we always are storing meshes in the engineering axes standard.
	const avs::Mesh* mesh = geometryStore.getMesh(id, avs::AxesStandard::EngineeringStyle);
	return mesh != nullptr;
}

TELEPORT_EXPORT bool IsMaterialStored(avs::uid id)
{
	const avs::Material* material = geometryStore.getMaterial(id);
	return material != nullptr;
}

TELEPORT_EXPORT bool IsTextureStored(avs::uid id)
{
	const avs::Texture* texture = geometryStore.getTexture(id);
	return texture != nullptr;
}

TELEPORT_EXPORT void RemoveNode(avs::uid nodeID)
{
	geometryStore.removeNode(nodeID);
}

TELEPORT_EXPORT avs::Node* getNode(avs::uid nodeID)
{
	return geometryStore.getNode(nodeID);
}

TELEPORT_EXPORT uint64_t GetNumberOfTexturesWaitingForCompression()
{
	return static_cast<int64_t>(geometryStore.getNumberOfTexturesWaitingForCompression());
}

///TODO: Free memory of allocated string, or use passed in string to return message.
TELEPORT_EXPORT BSTR GetMessageForNextCompressedTexture(uint64_t textureIndex, uint64_t totalTextures)
{
	const avs::Texture* texture = geometryStore.getNextCompressedTexture();

	std::wstringstream messageStream;
	//Write compression message to wide string stream.
	messageStream << "Compressing texture " << textureIndex << "/" << totalTextures << " (" << texture->name.data() << " [" << texture->width << " x " << texture->height << "])";

	//Convert to binary string.
	return SysAllocString(messageStream.str().data());
}

TELEPORT_EXPORT void CompressNextTexture()
{
	geometryStore.compressNextTexture();
}
///GeometryStore END

TELEPORT_EXPORT size_t SizeOf(const char *str)
{
	if(strcmp(str,"ServerSettings")==0)
	{
		return sizeof(ServerSettings);
	}
	TELEPORT_CERR<<"Unknown type for SizeOf: "<<str<<"\n";
	return 0;
}