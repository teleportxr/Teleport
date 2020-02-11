#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <vector>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/CasterSettings.h"
#include "SimulCasterServer/CaptureDelegates.h"
#include "SimulCasterServer/DiscoveryService.h"
#include "SimulCasterServer/GeometryStore.h"
#include "SimulCasterServer/GeometryStreamingService.h"

#include "Export.h"
#include "InteropStructures.h"

using namespace SCServer;
TELEPORT_EXPORT void StartSession(avs::uid clientID, int32_t listenPort);
TELEPORT_EXPORT void StopStreaming(avs::uid clientID);
TELEPORT_EXPORT void StopSession(avs::uid clientID);


#include "SimulCasterServer/ClientData.h"
namespace
{
	static const uint16_t DISCOVERY_PORT = 10607;
	static const uint16_t SERVICE_PORT = 10500;

	static avs::Context avsContext;

	static std::shared_ptr<PluginDiscoveryService> discoveryService = std::make_unique<PluginDiscoveryService>();
	static GeometryStore geometryStore;

	static std::map<avs::uid, ClientData> clientServices;

	static SCServer::CasterSettings casterSettings; //Unity side settings are copied into this, so inner-classes can reference this rather than managed code instance.

	static std::function<void(void** actorPtr)> onShowActor;
	static std::function<void(void** actorPtr)> onHideActor;

	static SetHeadPoseFn setHeadPose;
	static SetControllerPoseFn setControllerPose;
	static ProcessNewInputFn processNewInput;
	static int32_t connectionTimeout = 5;
	static avs::uid serverID = 0;

	static std::set<avs::uid> unlinkedClientIDs; //Client IDs that haven't been linked to a session component.
	static std::vector<avs::uid> lostClients; //Clients who have been lost, and are awaiting deletion.
}

class PluginDiscoveryService: public SCServer::DiscoveryService
{
public:
	virtual ~PluginDiscoveryService()
	{
		shutdown();
	}

	virtual bool initialise(uint16_t discoveryPort = 0, uint16_t servicePort = 0) override
	{
		if(discoveryPort == 0) discoveryPort = this->discoveryPort;
		else this->discoveryPort = discoveryPort;

		if(discoveryPort == 0)
		{
			printf_s("Discovery port is not set.\n");
			return false;
		}

		if(servicePort == 0) servicePort = this->servicePort;
		else this->servicePort = servicePort;

		if(servicePort == 0)
		{
			printf_s("Service port is not set.\n");
			return false;
		}
		
		discoverySocket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);
		if(discoverySocket <= 0)
		{
			printf_s("Failed to create discovery socket.\n");
			return false;
		}

		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_BROADCAST, 1);
		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_REUSEADDR, 1);

		address = {ENET_HOST_ANY, discoveryPort};
		if(enet_socket_bind(discoverySocket, &address) != 0)
		{
			printf_s("Failed to bind discovery socket on port: %d\n", address.port);
			enet_socket_destroy(discoverySocket);
			discoverySocket = 0;
			return false;
		}

		return true;
	}

	virtual void shutdown() override
	{
		enet_socket_destroy(discoverySocket);
		discoverySocket = 0;
	}

	virtual void tick() override
	{
		if(!discoverySocket || discoveryPort == 0 || servicePort == 0)
		{
			printf_s("Attempted to call tick on client discovery service without initalising!");
			return;
		}

		//List of clientIDs we want to attempt to connect to.
		std::set<uint32_t> newClients;

		uint32_t clientID = 0; //Newly received ID.
		ENetBuffer buffer = {sizeof(clientID), &clientID}; //Buffer to retrieve client ID with.

		//Retrieve all packets received since last call, and add any new clients.
		while(enet_socket_receive(discoverySocket, &address, &buffer, 1) != 0)
		{
			//Skip clients we have already added.
			if(clientServices.find(clientID) != clientServices.end()) continue;

			std::wstring desiredIP(casterSettings.clientIP);

			//Ignore connections from clients with the wrong IP, if a desired IP has been set.
			if(desiredIP.length() != 0)
			{
				//Retrieve IP of client that sent message, and covert to string.
				char clientIPRaw[20];
				enet_address_get_host_ip(&address, clientIPRaw, 20);

				//Trying to use the pointer to the string's data results in an incorrect size, and incorrect iterators.
				std::string clientIP = clientIPRaw;

				//Create new wide-string with clientIP, and add new client if there is no difference between the new client's IP and the desired IP.
				if(desiredIP.compare(0, clientIP.size(), {clientIP.begin(), clientIP.end()}) == 0)
				{
					newClients.insert(clientID);
				}
			}
			else
			{
				newClients.insert(clientID);
			}
		}

		//Send response, containing port to connect on, to all clients we want to host.
		for(uint32_t id : newClients)
		{
			ServiceDiscoveryResponse response = {clientID, servicePort};

			buffer = {sizeof(ServiceDiscoveryResponse), &response};
			enet_socket_send(discoverySocket, &address, &buffer, 1);

			StartSession(clientID, servicePort);
			lastFoundClientID=clientID;
		}
	}
	virtual uint64_t getNewClientID()
	{
		return lastFoundClientID;
		lastFoundClientID=0;
	}
private:
	uint32_t lastFoundClientID=0;
#pragma pack(push, 1) 
	struct ServiceDiscoveryResponse
	{
		uint32_t clientID;
		uint16_t remotePort;
	};
#pragma pack(pop)

	ENetSocket discoverySocket;
	ENetAddress address;

	uint16_t discoveryPort = 0;
	uint16_t servicePort = 0;
};

class PluginGeometryStreamingService : public SCServer::GeometryStreamingService
{
public:
	PluginGeometryStreamingService()
		:SCServer::GeometryStreamingService(&casterSettings)
	{
		this->geometryStore = &::geometryStore;
	}

	void addActor(void* newActor, avs::uid actorID)
	{
		SCServer::GeometryStreamingService::addActor(newActor, actorID);
	}

	avs::uid removeActor(void* oldActor)
	{
		return SCServer::GeometryStreamingService::removeActor(oldActor);
	}

	avs::uid getActorID(void* actor)
	{
		return SCServer::GeometryStreamingService::getActorID(actor);
	}

	bool isStreamingActor(void* actor)
	{
		return SCServer::GeometryStreamingService::isStreamingActor(actor);
	}

private:
	virtual void showActor_Internal(void* actorPtr)
	{
		if(onShowActor) onShowActor(&actorPtr);
	}

	virtual void hideActor_Internal(void* actorPtr)
	{
		if(onHideActor) onHideActor(&actorPtr);
	}
};

///PLUGIN-SPECIFIC START
TELEPORT_EXPORT
void UpdateCasterSettings(const SCServer::CasterSettings newSettings)
{
	casterSettings = newSettings;
}

TELEPORT_EXPORT
void SetShowActorDelegate(void(*showActor)(void*))
{
	onShowActor = showActor;
}

TELEPORT_EXPORT
void SetHideActorDelegate(void(*hideActor)(void*))
{
	onHideActor = hideActor;
}

TELEPORT_EXPORT
void SetHeadPoseSetterDelegate(SetHeadPoseFn headPoseSetter)
{
	setHeadPose = headPoseSetter;
}

TELEPORT_EXPORT
void SetControllerPoseSetterDelegate(SetControllerPoseFn f)
{
	setControllerPose = f;
}

TELEPORT_EXPORT
void SetNewInputProcessingDelegate(ProcessNewInputFn newInputProcessing)
{
	processNewInput = newInputProcessing;
}

TELEPORT_EXPORT
void SetMessageHandlerDelegate(avs::MessageHandlerFunc messageHandler)
{
	avsContext.setMessageHandler(messageHandler, nullptr);
}

TELEPORT_EXPORT
void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}

TELEPORT_EXPORT
void Initialise(void(*showActor)(void*), void(*hideActor)(void*),
				void(*headPoseSetter)(avs::uid clientID, const avs::HeadPose*), void(*controllerPoseSetter)(avs::uid uid,int index,const avs::HeadPose*), void(*newInputProcessing)(avs::uid clientID, const avs::InputState*),
				avs::MessageHandlerFunc messageHandler)
{
	serverID = avs::GenerateUid();

	SetShowActorDelegate(showActor);
	SetHideActorDelegate(hideActor);
	SetHeadPoseSetterDelegate(headPoseSetter);
	SetControllerPoseSetterDelegate(controllerPoseSetter);
	SetNewInputProcessingDelegate(newInputProcessing);
	SetMessageHandlerDelegate(messageHandler);

	if(enet_initialize() != 0)
	{
		printf_s("An error occurred while attempting to initalise ENet!\n");
		return;
	}
	atexit(enet_deinitialize);

	discoveryService->initialise(DISCOVERY_PORT, SERVICE_PORT);
}

TELEPORT_EXPORT
void Shutdown()
{
	discoveryService->shutdown();

	for(auto& clientService : clientServices)
	{
		clientService.second.clientMessaging.stopSession();
	}
	clientServices.clear();

	onShowActor = nullptr;
	onHideActor = nullptr;

	setHeadPose = nullptr;
	setControllerPose=nullptr;
	processNewInput = nullptr;
}

ClientData::ClientData(std::shared_ptr<PluginGeometryStreamingService> gs, std::function<void(void)> disconnect)
	:geometryStreamingService(gs),
	clientMessaging(&casterSettings, discoveryService, geometryStreamingService, setHeadPose, setControllerPose, processNewInput, disconnect, connectionTimeout)
{}

TELEPORT_EXPORT
void StartSession(avs::uid clientID, int32_t listenPort)
{
	ClientData newClientData(std::make_shared<PluginGeometryStreamingService>(), std::bind(&StopStreaming, clientID));

	if(newClientData.clientMessaging.startSession(clientID, listenPort))
	{
		clientServices.emplace(clientID, std::move(newClientData));

		ClientData& newClient = clientServices.at(clientID);
		newClient.casterContext.ColorQueue = std::make_unique<avs::Queue>();
		newClient.casterContext.GeometryQueue = std::make_unique<avs::Queue>();

		newClient.casterContext.ColorQueue->configure(16);
		newClient.casterContext.GeometryQueue->configure(16);

		///TODO: Initialise real delegates for capture component.
		SCServer::CaptureDelegates delegates;
		delegates.startStreaming = [](SCServer::CasterContext* context){};
		delegates.requestKeyframe = [](){};
		delegates.getClientCameraInfo = []()->SCServer::CameraInfo&
		{
			return SCServer::CameraInfo();
		};

		newClient.clientMessaging.initialise(&newClient.casterContext, delegates);

		unlinkedClientIDs.insert(clientID);
	}
	else
	{
		std::cout << "Failed to start session for client: " << clientID << std::endl;
	}
}

TELEPORT_EXPORT
void StopSession(avs::uid clientID)
{
	if(clientServices.at(clientID).isStreaming) StopStreaming(clientID);

	clientServices.at(clientID).clientMessaging.stopSession();
	clientServices.erase(clientID);
}

TELEPORT_EXPORT
void StartStreaming(avs::uid clientID)
{
	ClientData& client = clientServices.at(clientID);
	client.geometryStreamingService->startStreaming(&client.casterContext);

	///TODO: Need to retrieve encoder settings from unity.
	SCServer::CasterEncoderSettings encoderSettings
	{
		1536,
		1536,
		1920,
		960,
		false,
		true,
		true,
		10000
	};

	avs::SetupCommand setupCommand;
	setupCommand.video_width = encoderSettings.frameWidth;
	setupCommand.video_height = encoderSettings.frameHeight;
	setupCommand.depth_height = encoderSettings.depthHeight;
	setupCommand.depth_width = encoderSettings.depthWidth;
	setupCommand.colour_cubemap_size = encoderSettings.frameWidth / 3;
	setupCommand.compose_cube = encoderSettings.enableDecomposeCube;
	setupCommand.port = clientServices.at(clientID).clientMessaging.getServerPort() + 1;
	setupCommand.debug_stream = casterSettings.debugStream;
	setupCommand.debug_network_packets = casterSettings.enableDebugNetworkPackets;
	setupCommand.do_checksums = casterSettings.enableChecksums ? 1 : 0;
	setupCommand.server_id = serverID;
	setupCommand.use_10_bit_decoding = casterSettings.use10BitEncoding;
	setupCommand.use_yuv_444_decoding = casterSettings.useYUV444Decoding;
	setupCommand.requiredLatencyMs = casterSettings.requiredLatencyMs;

	///TODO: Initialise actors in range.

	client.clientMessaging.sendSetupCommand(std::move(setupCommand));

	client.isStreaming = true;
}

TELEPORT_EXPORT
void StopStreaming(avs::uid clientID)
{
	clientServices.at(clientID).geometryStreamingService->stopStreaming();
	clientServices.at(clientID).isStreaming = false;

	//Delay deletion of clients.
	lostClients.push_back(clientID);
}

TELEPORT_EXPORT
void Tick(float deltaTime)
{
	//Delete client data for clients who have been lost.
	if(lostClients.size() != 0)
	{
		for(avs::uid clientID : lostClients)
		{
			StopSession(clientID);
		}
		lostClients.clear();
	}

	for(auto& idClientPair : clientServices)
	{
		ClientData &clientData=idClientPair.second;
		clientData.clientMessaging.handleEvents();

		if(clientData.clientMessaging.hasPeer())
		{
			if(idClientPair.second.casterContext.NetworkPipeline)
			{
				idClientPair.second.casterContext.NetworkPipeline->process();
			}

			clientData.clientMessaging.tick(deltaTime);

			if(clientData.isStreaming == false)
			{
				StartStreaming(idClientPair.first);
			}
		}
	}

	discoveryService->tick();
}

TELEPORT_EXPORT
void Client_SetOrigin(avs::uid clientID, const avs::vec3 *pos)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return;
	ClientData &clientData=c->second;
	clientData.setOrigin(*pos);
}
TELEPORT_EXPORT
bool Client_IsConnected(avs::uid clientID)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	return clientData.isConnected();
}

TELEPORT_EXPORT
bool Client_HasOrigin(avs::uid clientID)
{
	auto &c=clientServices.find(clientID);
	if(c==clientServices.end())
		return false;
	ClientData &clientData=c->second;
	return(clientData.hasOrigin());
}

TELEPORT_EXPORT
void Reset()
{
	for(auto& clientIDInfoPair : clientServices)
	{
		clientIDInfoPair.second.geometryStreamingService->reset();
	}
}

TELEPORT_EXPORT
avs::uid GetUnlinkedClientID()
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
///PLUGIN-SPECIFC END

///libavstream START
TELEPORT_EXPORT
avs::uid GenerateID()
{
	return avs::GenerateUid();
}
///libavstream END

///GeometryStreamingService START
TELEPORT_EXPORT
void AddActor(avs::uid clientID, void* newActor, avs::uid actorID)
{
	clientServices.at(clientID).geometryStreamingService->addActor(newActor, actorID);
}

TELEPORT_EXPORT
avs::uid RemoveActor(avs::uid clientID, void* oldActor)
{
	return clientServices.at(clientID).geometryStreamingService->removeActor(oldActor);
}

TELEPORT_EXPORT
avs::uid GetActorID(avs::uid clientID, void* actor)
{
	return clientServices.at(clientID).geometryStreamingService->getActorID(actor);
}

TELEPORT_EXPORT
bool IsStreamingActor(avs::uid clientID, void* actor)
{
	return clientServices.at(clientID).geometryStreamingService->isStreamingActor(actor);
}

TELEPORT_EXPORT
void ShowActor(avs::uid clientID, avs::uid actorID)
{
	clientServices.at(clientID).geometryStreamingService->showActor(actorID);
}

TELEPORT_EXPORT
void HideActor(avs::uid clientID, avs::uid actorID)
{
	clientServices.at(clientID).geometryStreamingService->hideActor(actorID);
}

TELEPORT_EXPORT
void SetActorVisible(avs::uid clientID, avs::uid actorID, bool isVisible)
{
	if(isVisible) ShowActor(clientID, actorID);
	else HideActor(clientID, actorID);
}

bool HasResource(avs::uid clientID, avs::uid resourceID)
{
	return clientServices.at(clientID).geometryStreamingService->hasResource(resourceID);
}
///GeometryStreamingService END

///ClientMessaging START
TELEPORT_EXPORT
void ActorEnteredBounds(avs::uid clientID, avs::uid actorID)
{
	clientServices.at(clientID).clientMessaging.actorEnteredBounds(actorID);
}

TELEPORT_EXPORT
void ActorLeftBounds(avs::uid clientID, avs::uid actorID)
{
	clientServices.at(clientID).clientMessaging.actorLeftBounds(actorID);
}

TELEPORT_EXPORT
bool HasHost(avs::uid clientID)
{
	return clientServices.at(clientID).clientMessaging.hasHost();
}

TELEPORT_EXPORT
bool HasPeer(avs::uid clientID)
{
	return clientServices.at(clientID).clientMessaging.hasPeer();
}

TELEPORT_EXPORT
bool SendCommand(avs::uid clientID, const avs::Command& avsCommand)
{
	return clientServices.at(clientID).clientMessaging.sendCommand(avsCommand);
}

TELEPORT_EXPORT
bool SendCommandWithList(avs::uid clientID, const avs::Command& avsCommand, std::vector<avs::uid>& appendedList)
{
	return clientServices.at(clientID).clientMessaging.sendCommand(avsCommand, appendedList);
}

TELEPORT_EXPORT
char* GetClientIP(avs::uid clientID)
{
	return clientServices.at(clientID).clientMessaging.getClientIP().data();
}

TELEPORT_EXPORT
uint16_t GetClientPort(avs::uid clientID)
{
	return clientServices.at(clientID).clientMessaging.getClientPort();
}

TELEPORT_EXPORT
uint16_t GetServerPort(avs::uid clientID)
{
	return clientServices.at(clientID).clientMessaging.getServerPort();
}
///ClientMessaging END

///GeometryStore START
TELEPORT_EXPORT
void SetDelayTextureCompression(bool willDelay)
{
	geometryStore.willDelayTextureCompression = willDelay;
}

TELEPORT_EXPORT
void SetCompressionLevels(uint8_t compressionStrength, uint8_t compressionQuality)
{
	geometryStore.setCompressionLevels(compressionStrength, compressionQuality);
}

TELEPORT_EXPORT
const std::vector<std::pair<void*, avs::uid>>& GetHands()
{
	return geometryStore.getHands();
}

TELEPORT_EXPORT
void SetHands(std::pair<void*, avs::uid> firstHand, std::pair<void*, avs::uid> secondHand)
{
	geometryStore.setHands(firstHand, secondHand);
}

TELEPORT_EXPORT
void StoreNode(avs::uid id, InteropNode node)
{
	//Covert to avs::DataNode in-place.
	geometryStore.storeNode
	(
		id,
		{
			node.transform,
			node.dataID,
			node.dataType,
			{node.materialIDs, node.materialIDs + node.materialAmount},
			{node.childIDs, node.childIDs + node.childAmount}
		}
	);
}

TELEPORT_EXPORT
void StoreMesh(avs::uid id, InteropMesh* mesh)
{
	avs::Mesh newMesh;

	//Create vector in-place with pointer.
	newMesh.primitiveArrays = {mesh->primitiveArrays, mesh->primitiveArrays + mesh->primitiveArrayAmount};
	//Memcpy the attributes into a new memory location; the old location will be cleared/moved by C#'s garbage collector.
	for(int i = 0; i < mesh->primitiveArrayAmount; i++)
	{
		size_t dataSize = sizeof(avs::Attribute) * mesh->primitiveArrays[i].attributeCount;

		newMesh.primitiveArrays[i].attributes = new avs::Attribute[dataSize];
		memcpy_s(newMesh.primitiveArrays[i].attributes, dataSize, mesh->primitiveArrays[i].attributes, dataSize);
	}

	//Zip all of the maps back together.
	for(int i = 0; i < mesh->accessorAmount; i++)
	{
		newMesh.accessors[mesh->accessorIDs[i]] = mesh->accessors[i];
	}

	for(int i = 0; i < mesh->bufferViewAmount; i++)
	{
		newMesh.bufferViews[mesh->bufferViewIDs[i]] = mesh->bufferViews[i];
	}

	for(int i = 0; i < mesh->bufferAmount; i++)
	{
		newMesh.buffers[mesh->bufferIDs[i]] = mesh->buffers[i];

		//Memcpy the data into a new memory location; the old location will be cleared/moved by C#'s garbage collector.
		newMesh.buffers[mesh->bufferIDs[i]].data = new uint8_t[mesh->buffers[i].byteLength];
		memcpy_s(const_cast<uint8_t*>(newMesh.buffers[mesh->bufferIDs[i]].data), mesh->buffers[i].byteLength, mesh->buffers[i].data, mesh->buffers[i].byteLength);
	}

	geometryStore.storeMesh(id, std::move(newMesh));
}

TELEPORT_EXPORT
void StoreMaterial(avs::uid id, InteropMaterial material)
{
	std::unordered_map<avs::MaterialExtensionIdentifier, std::shared_ptr<avs::MaterialExtension>> extensions;

	//Stitch extension map together.
	for(int i = 0; i < material.extensionAmount; i++)
	{
		avs::MaterialExtensionIdentifier extensionID = material.extensionIDs[i];

		switch(extensionID)
		{
			case avs::MaterialExtensionIdentifier::SIMPLE_GRASS_WIND:
				extensions.emplace(extensionID, std::make_shared<avs::SimpleGrassWindExtension>(*static_cast<avs::SimpleGrassWindExtension*>(material.extensions[i])));
				break;
		}
	}

	geometryStore.storeMaterial
	(
		id,
		{
			{material.name, material.name + material.nameLength},
			material.pbrMetallicRoughness,
			material.normalTexture,
			material.occlusionTexture,
			material.emissiveTexture,
			material.emissiveFactor,
			extensions
		}
	);
}

TELEPORT_EXPORT
void StoreTexture(avs::uid id, InteropTexture texture, std::time_t lastModified, char* basisFileLocation)
{
	geometryStore.storeTexture(id, ToAvsTexture(texture, true), lastModified, basisFileLocation);
}

TELEPORT_EXPORT
void StoreShadowMap(avs::uid id, InteropTexture shadowMap)
{
	geometryStore.storeShadowMap(id, ToAvsTexture(shadowMap, true));
}

avs::DataNode* getNode(avs::uid nodeID)
{
	return geometryStore.getNode(nodeID);
}

TELEPORT_EXPORT
uint64_t GetAmountOfTexturesWaitingForCompression()
{
	return static_cast<int64_t>(geometryStore.getAmountOfTexturesWaitingForCompression());
}

TELEPORT_EXPORT
BSTR GetMessageForNextCompressedTexture(uint64_t textureIndex, uint64_t totalTextures)
{
	const avs::Texture* texture = geometryStore.getNextCompressedTexture();

	std::wstringstream messageStream;
	//Write compression message to wide string stream.
	messageStream << "Compressing texture " << textureIndex << "/" << totalTextures << " (" << texture->name.data() << " [" << texture->width << " x " << texture->height << "])";

	//Convert to binary string.
	return SysAllocString(messageStream.str().data());
}

TELEPORT_EXPORT
void CompressNextTexture()
{
	geometryStore.compressNextTexture();
}
///GeometryStore END