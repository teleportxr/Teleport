#include <functional>
#include <iostream>
#include <vector>

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "SimulCasterServer/ClientMessaging.h"
#include "SimulCasterServer/DiscoveryService.h"
#include "SimulCasterServer/GeometryStore.h"
#include "SimulCasterServer/GeometryStreamingService.h"

#include "Export.h"
#include "InteropStructures.h"

using namespace SCServer;

class PluginDiscoveryService;
struct ClientData;

TELEPORT_EXPORT
void StartSession(avs::uid clientID, int32_t listenPort);

namespace
{
	static const uint16_t DISCOVERY_PORT = 10607;
	static const uint16_t SERVICE_PORT = 10500;

	static std::shared_ptr<PluginDiscoveryService> discoveryService = std::make_unique<PluginDiscoveryService>();
	static GeometryStore geometryStore;

	static std::map<avs::uid, ClientData> clientServices;

	static const SCServer::CasterSettings* casterSettings;

	static std::function<void(void* actorPtr)> onShowActor;
	static std::function<void(void* actorPtr)> onHideActor;

	static std::function<void(const avs::HeadPose*)> setHeadPose;
	static std::function<void(const avs::InputState*)> processNewInput;
	static std::function<void(void)> onDisconnect;
	static int32_t connectionTimeout = 5;
	static avs::uid serverID = 0;
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
		if(discoveryPort == 0 && this->discoveryPort == 0)
		{
			printf_s("Discovery port is not set.\n");
			return false;
		}

		if(servicePort == 0 && this->servicePort == 0)
		{
			printf_s("Service port is not set.\n");
			return false;
		}

		this->discoveryPort = discoveryPort;
		this->servicePort = servicePort;

		discoverySocket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);
		if(discoverySocket <= 0)
		{
			printf_s("Failed to create discovery socket.\n");
			return false;
		}

		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_BROADCAST, 1);
		enet_socket_set_option(discoverySocket, ENetSocketOption::ENET_SOCKOPT_REUSEADDR, 1);

		ENetAddress address{ENET_HOST_ANY, discoveryPort};
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
		ENetAddress address{ENET_HOST_ANY, discoveryPort};

		uint32_t clientID = 0;
		ENetBuffer buffer = {sizeof(clientID), &clientID};
		if(enet_socket_receive(discoverySocket, &address, &buffer, 1) != 0)
		{
			std::wstring desiredIP(casterSettings->clientIP);

			bool sendResponse = true;
			///TODO: Get desired IP from Unity. Strings don't seem to like being passed inside a class that is referenced by pointer.
			/*if(desiredIP.length() != 0)
			{
				std::string clientIP(20, ' ');
				enet_address_get_host_ip(&address, clientIP.data(), 20);

				sendResponse = desiredIP.compare({clientIP.begin(), clientIP.end()}) == 0;
			}*/

#pragma pack(push, 1) 
			struct ServiceDiscoveryResponse
			{
				uint32_t clientID;
				uint16_t remotePort;
			} response;
#pragma pack(pop)

			response.clientID = clientID;
			response.remotePort = servicePort;

			buffer = {sizeof(ServiceDiscoveryResponse), &response};
			enet_socket_send(discoverySocket, &address, &buffer, 1);

			StartSession(clientID, servicePort);
		}
	}
private:
	ENetSocket discoverySocket;

	uint16_t discoveryPort = 0;
	uint16_t servicePort = 0;
};

class PluginGeometryStreamingService : public SCServer::GeometryStreamingService
{
public:
	PluginGeometryStreamingService()
		:SCServer::GeometryStreamingService(casterSettings)
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
		if(onShowActor) onShowActor(actorPtr);
	}

	virtual void hideActor_Internal(void* actorPtr)
	{
		if(onHideActor) onHideActor(actorPtr);
	}
};

struct ClientData
{
	SCServer::CasterContext casterContext;

	std::shared_ptr<PluginGeometryStreamingService> geometryStreamingService;
	SCServer::ClientMessaging clientMessaging;

	bool isStreaming = false;
};

///PLUGIN-SPECIFIC START
TELEPORT_EXPORT
void SetCasterSettings(const SCServer::CasterSettings* settings)
{
	casterSettings = settings;
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
void SetHeadPoseSetterDelegate(void(*headPoseSetter)(const avs::HeadPose*))
{
	setHeadPose = headPoseSetter;
}

TELEPORT_EXPORT
void SetNewInputProcessingDelegate(void(*newInputProcessing)(const avs::InputState*))
{
	processNewInput = newInputProcessing;
}

TELEPORT_EXPORT
void SetDisconnectDelegate(void(*disconnect)(void))
{
	onDisconnect = disconnect;
}

TELEPORT_EXPORT
void SetConnectionTimeout(int32_t timeout)
{
	connectionTimeout = timeout;
}

TELEPORT_EXPORT
void Initialise(const SCServer::CasterSettings* settings, void(*showActor)(void*), void(*hideActor)(void*),
				void(*headPoseSetter)(const avs::HeadPose*), void(*newInputProcessing)(const avs::InputState*), void(*disconnect)(void))
{
	serverID = avs::GenerateUid();

	SetCasterSettings(settings);
	SetShowActorDelegate(showActor);
	SetHideActorDelegate(hideActor);
	SetHeadPoseSetterDelegate(headPoseSetter);
	SetNewInputProcessingDelegate(newInputProcessing);
	SetDisconnectDelegate(disconnect);

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

	casterSettings = nullptr;

	onShowActor = nullptr;
	onHideActor = nullptr;

	setHeadPose = nullptr;
	processNewInput = nullptr;
	onDisconnect = nullptr;
}

TELEPORT_EXPORT
void StartSession(avs::uid clientID, int32_t listenPort)
{
	std::shared_ptr<PluginGeometryStreamingService> newStreamingService(std::make_shared<PluginGeometryStreamingService>());
	ClientData newClientData
	{
		SCServer::CasterContext(),
		newStreamingService,
		SCServer::ClientMessaging(casterSettings, discoveryService, newStreamingService, setHeadPose, processNewInput, onDisconnect, connectionTimeout)
	};

	if(newClientData.clientMessaging.startSession(listenPort))
	{
		clientServices.emplace(clientID, std::move(newClientData));

		ClientData& newClient = clientServices.at(clientID);
		newClient.casterContext.NetworkPipeline = std::make_unique<SCServer::NetworkPipeline>(casterSettings);
		newClient.casterContext.ColorQueue = std::make_unique<avs::Queue>();
		newClient.casterContext.GeometryQueue = std::make_unique<avs::Queue>();

		newClient.casterContext.ColorQueue->configure(16);
		newClient.casterContext.GeometryQueue->configure(16);

		///TODO: Initialise clientMessaging.
		//clientServices.at(clientID).clientMessaging.initialise(casterContext, captureComponent);
	}
	else
	{
		std::cout << "Failed to start session for client: " << clientID << std::endl;
	}
}

TELEPORT_EXPORT
void StopSession(avs::uid clientID)
{
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
	setupCommand.debug_stream = casterSettings->debugStream;
	setupCommand.debug_network_packets = casterSettings->enableDebugNetworkPackets;
	setupCommand.do_checksums = casterSettings->enableChecksums ? 1 : 0;
	setupCommand.server_id = serverID;
	setupCommand.use_10_bit_decoding = casterSettings->use10BitEncoding;
	setupCommand.use_yuv_444_decoding = casterSettings->useYUV444Decoding;
	setupCommand.requiredLatencyMs = casterSettings->requiredLatencyMs;

	///TODO: Initialise actors in range.

	///TODO: Need to initalise clientMessaging before the handshake can be received.
	//client.clientMessaging.sendSetupCommand(std::move(setupCommand));

	client.isStreaming = true;
}

TELEPORT_EXPORT
void StopStreaming(avs::uid clientID)
{
	clientServices.at(clientID).geometryStreamingService->stopStreaming();
}

TELEPORT_EXPORT
void Tick(float deltaTime)
{
	for(auto& idClientPair : clientServices)
	{
		if(idClientPair.second.clientMessaging.hasPeer())
		{
			idClientPair.second.clientMessaging.tick(deltaTime);

			if(idClientPair.second.isStreaming == false && idClientPair.second.clientMessaging.hasPeer())
			{
				StartStreaming(idClientPair.first);
			}
		}

		idClientPair.second.clientMessaging.handleEvents();
	}
	
	discoveryService->tick();
}

TELEPORT_EXPORT
void Reset()
{
	for(auto& clientIDInfoPair : clientServices)
	{
		clientIDInfoPair.second.geometryStreamingService->reset();
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
void addActor(avs::uid clientID, void* newActor, avs::uid actorID)
{
	clientServices.at(clientID).geometryStreamingService->addActor(newActor, actorID);
}

TELEPORT_EXPORT
avs::uid removeActor(avs::uid clientID, void* oldActor)
{
	return clientServices.at(clientID).geometryStreamingService->removeActor(oldActor);
}

TELEPORT_EXPORT
avs::uid getActorID(avs::uid clientID, void* actor)
{
	return clientServices.at(clientID).geometryStreamingService->getActorID(actor);
}

TELEPORT_EXPORT
bool isStreamingActor(avs::uid clientID, void* actor)
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
bool SendSetupCommand(avs::uid clientID, avs::SetupCommand&& setupCommand)
{
	return clientServices.at(clientID).clientMessaging.sendSetupCommand(std::move(setupCommand));
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
void StoreMesh(avs::uid id, InteropMesh mesh)
{
	avs::Mesh newMesh;
	//Create vector in-place with pointer.
	newMesh.primitiveArrays = {mesh.primitiveArrays, mesh.primitiveArrays + mesh.primitiveArrayAmount};

	//Zip all of the maps back together.
	for(int i = 0; i < mesh.accessorAmount; i++)
	{
		newMesh.accessors[mesh.accessorIDs[i]] = mesh.accessors[i];
	}

	for(int i = 0; i < mesh.bufferViewAmount; i++)
	{
		newMesh.bufferViews[mesh.bufferViewIDs[i]] = mesh.bufferViews[i];
	}

	for(int i = 0; i < mesh.accessorAmount; i++)
	{
		newMesh.buffers[mesh.bufferIDs[i]] = mesh.buffers[i];
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
	///If I'm passing the pointer to the texture memory, then I need to perform an operation to retrieve the actual data.

	//geometryStore.storeTexture(id, ToAvsTexture(texture), lastModified, basisFileLocation);
}

TELEPORT_EXPORT
void StoreShadowMap(avs::uid id, InteropTexture shadowMap)
{
	geometryStore.storeShadowMap(id, ToAvsTexture(shadowMap));
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
const avs::Texture* GetNextCompressedTexture()
{
	return geometryStore.getNextCompressedTexture();
}

TELEPORT_EXPORT
void CompressNextTexture()
{
	geometryStore.compressNextTexture();
}
///GeometryStore END