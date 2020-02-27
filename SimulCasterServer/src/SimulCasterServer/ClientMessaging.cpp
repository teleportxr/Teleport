#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common.hpp" //InputState

#include "DiscoveryService.h"

using namespace SCServer;

ClientMessaging::ClientMessaging(const CasterSettings* settings,
								 std::shared_ptr<DiscoveryService> discoveryService,
								 std::shared_ptr<GeometryStreamingService> geometryStreamingService,
								 std::function<void(avs::uid,const avs::HeadPose*)> inSetHeadPose,
								 std::function<void(avs::uid,int index,const avs::HeadPose*)> inSetControllerPose,
								 std::function<void(avs::uid,const avs::InputState*)> inProcessNewInput,
								 std::function<void(void)> onDisconnect,
								 const int32_t& disconnectTimeout)
	: settings(settings) 
	, discoveryService(discoveryService) 
	, geometryStreamingService(geometryStreamingService)
	, setHeadPose(inSetHeadPose)
	, setControllerPose(inSetControllerPose)
	, processNewInput(inProcessNewInput)
	, onDisconnect(onDisconnect)
	, disconnectTimeout(disconnectTimeout)
	, host(nullptr)
	, peer(nullptr)
	, casterContext(nullptr)
{}

void ClientMessaging::initialise(CasterContext* context, CaptureDelegates captureDelegates)
{
	casterContext = context;
	captureComponentDelegates = captureDelegates;
}

bool ClientMessaging::startSession(avs::uid clientID, int32_t listenPort)
{
	this->clientID = clientID;

	ENetAddress ListenAddress;
	ListenAddress.host = ENET_HOST_ANY;
	ListenAddress.port = listenPort;

	// ServerHost will live for the lifetime of the session.
	host = enet_host_create(&ListenAddress, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
	if(!host)
	{
		std::cout << "Session: Failed to create ENET server host!\n";
	}

	return host;
}

void ClientMessaging::stopSession()
{
	if(peer)
	{
		assert(host);

		enet_host_flush(host);
		enet_peer_disconnect(peer, 0);

		ENetEvent event;
		bool bIsPeerConnected = true;
		while(bIsPeerConnected && enet_host_service(host, &event, disconnectTimeout) > 0)
		{
			switch(event.type)
			{
				case ENET_EVENT_TYPE_RECEIVE:
					enet_packet_destroy(event.packet);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					bIsPeerConnected = false;
					break;
			}
		}
		if(bIsPeerConnected)
		{
			enet_peer_reset(peer);
		}
		peer = nullptr;
	}

	if(host)
	{
		enet_host_destroy(host);
		host = nullptr;
	}
}

void ClientMessaging::tick(float deltaTime)
{
	static float timeSinceLastGeometryStream = 0;
	timeSinceLastGeometryStream += deltaTime;

	const float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f / settings->geometryTicksPerSecond;

	//Only tick the geometry streaming service a set amount of times per second.
	if(timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
	{
		geometryStreamingService->tick(TIME_BETWEEN_GEOMETRY_TICKS);

		//Tell the client to change the visibility of actors that have changed whether they are within streamable bounds.
		if(!actorsEnteredBounds.empty() || !actorsLeftBounds.empty())
		{
			size_t commandSize = sizeof(avs::ActorBoundsCommand);
			size_t enteredBoundsSize = sizeof(avs::uid) * actorsEnteredBounds.size();
			size_t leftBoundsSize = sizeof(avs::uid) * actorsLeftBounds.size();

			avs::ActorBoundsCommand boundsCommand(actorsEnteredBounds.size(), actorsLeftBounds.size());
			ENetPacket* packet = enet_packet_create(&boundsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);

			//Resize packet, and insert actor lists.
			enet_packet_resize(packet, commandSize + enteredBoundsSize + leftBoundsSize);
			memcpy(packet->data + commandSize, actorsEnteredBounds.data(), enteredBoundsSize);
			memcpy(packet->data + commandSize + enteredBoundsSize, actorsLeftBounds.data(), leftBoundsSize);

			enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);

			actorsEnteredBounds.clear();
			actorsLeftBounds.clear();
		}

		timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
	}
}

void ClientMessaging::handleEvents()
{
	ENetEvent event;
	while(enet_host_service(host, &event, 0) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
				assert(!peer);

				char address[20];
				enet_address_get_host_ip(&event.peer->address, address, sizeof(address));

				peer = event.peer;
				///TODO: This work allow multi-connect with Unity, or otherwise; change it to allow multiple connections.

				// TODO: This is pretty ropey: Discovery service really shouldn't be inside a specific client.
				if(!clientID) this->clientID = discoveryService->getNewClientID();
				discoveryService->shutdown();

				std::cout << "Client connected: " << getClientIP() << ":" << getClientPort() << std::endl;
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				assert(peer == event.peer);

				std::cout << "Client disconnected " << getClientIP() << ":" << getClientPort() << std::endl;
				onDisconnect();
				peer = nullptr;
				
				// TRY to restart the discovery service...
				discoveryService->initialise();
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				dispatchEvent(event);
				break;
		}
	}
}

void ClientMessaging::actorEnteredBounds(avs::uid actorID)
{
	actorsEnteredBounds.push_back(actorID);
	actorsLeftBounds.erase(std::remove(actorsLeftBounds.begin(), actorsLeftBounds.end(), actorID), actorsLeftBounds.end());
}

void ClientMessaging::actorLeftBounds(avs::uid actorID)
{
	actorsLeftBounds.push_back(actorID);
	actorsEnteredBounds.erase(std::remove(actorsEnteredBounds.begin(), actorsEnteredBounds.end(), actorID), actorsEnteredBounds.end());
}

bool ClientMessaging::hasHost() const
{
	return host;
}

bool ClientMessaging::hasPeer() const
{
	return peer;
}

bool ClientMessaging::sendCommand(const avs::Command& avsCommand) const
{
	assert(peer);

	size_t commandSize = avs::GetCommandSize(avsCommand.commandPayloadType);
	ENetPacket* packet = enet_packet_create(&avsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);
	assert(packet);

	return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
}

bool ClientMessaging::sendSetupCommand(avs::SetupCommand&& setupCommand)
{
	std::vector<avs::uid> resourcesClientNeeds;

	//Get resources the client will need to check it has.
	std::vector<avs::MeshNodeResources> outMeshResources;
	std::vector<avs::LightNodeResources> outLightResources;
	geometryStreamingService->getResourcesToStream(outMeshResources, outLightResources);

	for(const avs::MeshNodeResources& meshResource : outMeshResources)
	{
		resourcesClientNeeds.push_back(meshResource.node_uid);
		resourcesClientNeeds.push_back(meshResource.mesh_uid);

		for(const avs::MaterialResources& material : meshResource.materials)
		{
			resourcesClientNeeds.push_back(material.material_uid);

			for(avs::uid texture_uid : material.texture_uids)
			{
				resourcesClientNeeds.push_back(texture_uid);
			}
		}
	}
	for(const avs::LightNodeResources& lightResource : outLightResources)
	{
		resourcesClientNeeds.push_back(lightResource.node_uid);
		resourcesClientNeeds.push_back(lightResource.shadowmap_uid);
	}

	//Remove duplicates, and UIDs of 0.
	std::sort(resourcesClientNeeds.begin(), resourcesClientNeeds.end());
	resourcesClientNeeds.erase(std::unique(resourcesClientNeeds.begin(), resourcesClientNeeds.end()), resourcesClientNeeds.end());
	resourcesClientNeeds.erase(std::remove(resourcesClientNeeds.begin(), resourcesClientNeeds.end(), 0), resourcesClientNeeds.end());

	//If the client needs a resource it will tell us; we don't want to stream the data if the client already has it.
	for(avs::uid resourceID : resourcesClientNeeds)
	{
		geometryStreamingService->confirmResource(resourceID);
	}
	
	setupCommand.resourceCount = resourcesClientNeeds.size();
	return sendCommand<avs::uid>(setupCommand, resourcesClientNeeds);
}

std::string ClientMessaging::getClientIP() const
{
	assert(peer);

	char address[20];
	enet_address_get_host_ip(&peer->address, address, sizeof(address));

	return std::string(address);
}

uint16_t ClientMessaging::getClientPort() const
{
	assert(peer);

	return peer->address.port;
}

uint16_t ClientMessaging::getServerPort() const
{
	assert(host);

	return host->address.port;
}

void ClientMessaging::dispatchEvent(const ENetEvent& event)
{
	switch(event.channelID)
	{
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Handshake):
			//Delay the actual start of streaming until we receive a confirmation from the client that they are ready.
			receiveHandshake(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control):
			receiveInput(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_DisplayInfo):
			receiveDisplayInfo(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_HeadPose):
			receiveHeadPose(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ResourceRequest):
			receiveResourceRequest(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_KeyframeRequest):
			receiveKeyframeRequest(event.packet);
			break;
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage):
			receiveClientMessage(event.packet);
			break;
		default:
			break;
	}
	enet_packet_destroy(event.packet);
}

void ClientMessaging::receiveHandshake(const ENetPacket* packet)
{
	if(packet->dataLength != sizeof(avs::Handshake))
	{
		std::cout << "Session: Received malformed handshake packet of length: " << packet->dataLength << std::endl;
		return;
	}
	avs::Handshake handshake;
	memcpy(&handshake, packet->data, packet->dataLength);
	
	if(handshake.isReadyToReceivePayloads != true)
	{
		std::cout << "Session: Handshake not ready to receive.\n";
		return;
	}

	if(handshake.usingHands)
	{
		geometryStreamingService->addHandsToStream();
	}

	casterContext->axesStandard = handshake.axesStandard;

	int32_t streamingPort = getServerPort() + 1;

	if(!casterContext->NetworkPipeline)
	{
		std::string multibyteClientIP = getClientIP();
		size_t ipLength = strlen(multibyteClientIP.data());

		wchar_t clientIP[20];
		mbstowcs_s(&ipLength, clientIP, multibyteClientIP.data(), 20);

		CasterNetworkSettings networkSettings =
		{
			streamingPort,
			clientIP,
			networkSettings.localPort + 1,
			static_cast<int32_t>(handshake.maxBandwidthKpS),
			static_cast<int32_t>(handshake.udpBufferSize),
			settings->requiredLatencyMs
		};

		casterContext->NetworkPipeline.reset(new NetworkPipeline(settings));
		casterContext->NetworkPipeline->initialise(networkSettings, casterContext->ColorQueue.get(), casterContext->DepthQueue.get(), casterContext->GeometryQueue.get());
	}

	CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
	cameraInfo.width = static_cast<float>(handshake.startDisplayInfo.width);
	cameraInfo.height = static_cast<float>(handshake.startDisplayInfo.height);
	cameraInfo.fov = handshake.FOV;
	cameraInfo.isVR = handshake.isVR;

	captureComponentDelegates.startStreaming(casterContext);
	geometryStreamingService->startStreaming(casterContext);

	avs::AcknowledgeHandshakeCommand ack;
	sendCommand(ack);

	std::cout << "RemotePlay: Started streaming to " << getClientIP() << ":" << streamingPort << std::endl;
}

void ClientMessaging::setPosition(const avs::vec3 &pos)
{
	avs::SetPositionCommand setp;
	setp.position=pos;
	sendCommand(setp);
}

void ClientMessaging::receiveInput(const ENetPacket* packet)
{
	if(packet->dataLength != sizeof(avs::InputState))
	{
		std::cout << "Session: Received malformed input state change packet of length: " << packet->dataLength << std::endl;
		return;
	}

	avs::InputState inputState;
	memcpy(&inputState, packet->data, packet->dataLength);

	processNewInput(clientID, &inputState);
}

void ClientMessaging::receiveDisplayInfo(const ENetPacket* packet)
{
	if(packet->dataLength != sizeof(avs::DisplayInfo))
	{
		std::cout << "Session: Received malformed display info packet of length: " << packet->dataLength << std::endl;
		return;
	}

	avs::DisplayInfo displayInfo;
	memcpy(&displayInfo, packet->data, packet->dataLength);

	CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
	cameraInfo.width = static_cast<float>(displayInfo.width);
	cameraInfo.height = static_cast<float>(displayInfo.height);
}

void ClientMessaging::receiveHeadPose(const ENetPacket* packet)
{
	if(packet->dataLength != sizeof(avs::HeadPose))
	{
		std::cout << "Session: Received malformed head pose packet of length: " << packet->dataLength << std::endl;
		return;
	}
	
	avs::HeadPose headPose;
	memcpy(&headPose, packet->data, packet->dataLength);
	
	avs::ConvertRotation(casterContext->axesStandard, settings->axesStandard, headPose.orientation);
	avs::ConvertPosition(casterContext->axesStandard, settings->axesStandard, headPose.position);
	setHeadPose(clientID, &headPose);
}

void ClientMessaging::receiveResourceRequest(const ENetPacket* packet)
{
	size_t resourceAmount;
	memcpy(&resourceAmount, packet->data, sizeof(size_t));

	std::vector<avs::uid> resourceRequests(resourceAmount);
	memcpy(resourceRequests.data(), packet->data + sizeof(size_t), sizeof(avs::uid) * resourceAmount);

	for(avs::uid id : resourceRequests)
	{
		geometryStreamingService->requestResource(id);
	}
}

void ClientMessaging::receiveKeyframeRequest(const ENetPacket* packet)
{
	if(captureComponentDelegates.requestKeyframe)
	{
		captureComponentDelegates.requestKeyframe();
	}
	else
	{
		std::cout << "Received keyframe request, but capture component isn't set.\n";
	}
}

void ClientMessaging::receiveClientMessage(const ENetPacket* packet)
{
	avs::ClientMessagePayloadType clientMessagePayloadType = *((avs::ClientMessagePayloadType*)packet->data);
	switch(clientMessagePayloadType)
	{
		case avs::ClientMessagePayloadType::ControllerPoses:
		{
			avs::ControllerPosesMessage message;
			memcpy(&message, packet->data, packet->dataLength);
	
			for(int i=0;i<2;i++)
			{
				avs::HeadPose &pose=message.controllerPoses[i];
				avs::ConvertRotation(casterContext->axesStandard, settings->axesStandard, pose.orientation);
				avs::ConvertPosition(casterContext->axesStandard, settings->axesStandard, pose.position);
				setControllerPose(clientID, i, &pose);
			}
		}
		break;
		case avs::ClientMessagePayloadType::ActorStatus:
		{
			size_t messageSize = sizeof(avs::ActorStatusMessage);
			avs::ActorStatusMessage message;
			memcpy(&message, packet->data, messageSize);

			size_t drawnSize = sizeof(avs::uid) * message.actorsDrawnAmount;
			std::vector<avs::uid> drawn(message.actorsDrawnAmount);
			memcpy(drawn.data(), packet->data + messageSize, drawnSize);

			size_t toReleaseSize = sizeof(avs::uid) * message.actorsWantToReleaseAmount;
			std::vector<avs::uid> toRelease(message.actorsWantToReleaseAmount);
			memcpy(toRelease.data(), packet->data + messageSize + drawnSize, toReleaseSize);

			for(avs::uid actorID : drawn)
			{
				geometryStreamingService->hideActor(actorID);
			}

			for(avs::uid actorID : toRelease)
			{
				geometryStreamingService->showActor(actorID);
			}

			break;
		}
		case avs::ClientMessagePayloadType::ReceivedResources:
		{
			size_t messageSize = sizeof(avs::ReceivedResourcesMessage);
			avs::ReceivedResourcesMessage message;
			memcpy(&message, packet->data, messageSize);

			size_t confirmedResourcesSize = sizeof(avs::uid) * message.receivedResourcesAmount;
			std::vector<avs::uid> confirmedResources(message.receivedResourcesAmount);
			memcpy(confirmedResources.data(), packet->data + messageSize, confirmedResourcesSize);

			for(avs::uid id : confirmedResources)
			{
				geometryStreamingService->confirmResource(id);
			}

			break;
		}
		default:
			break;
	};
}
