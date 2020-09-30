#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common.hpp" //InputState

#include "DiscoveryService.h"
#include "ErrorHandling.h"

namespace SCServer
{
	ClientMessaging::ClientMessaging(const CasterSettings* settings,
		std::shared_ptr<DiscoveryService> discoveryService,
		std::shared_ptr<GeometryStreamingService> geometryStreamingService,
		std::function<void(avs::uid, const avs::Pose*)> inSetHeadPose,
		std::function<void(avs::uid, int index, const avs::Pose*)> inSetControllerPose,
		std::function<void(avs::uid, const avs::InputState*)> inProcessNewInput,
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
		, clientID(0)
	{}

	bool ClientMessaging::isInitialised() const
	{
		return initialized;
	}

	void ClientMessaging::initialise(CasterContext* context, CaptureDelegates captureDelegates)
	{
		casterContext = context;
		captureComponentDelegates = captureDelegates;
		initialized=true;
	}

	bool ClientMessaging::startSession(avs::uid clientID, int32_t listenPort)
	{
		this->clientID = clientID;

		ENetAddress ListenAddress;
		ListenAddress.host = ENET_HOST_ANY;
		ListenAddress.port = listenPort;

		// ServerHost will live for the lifetime of the session.
		host = enet_host_create(&ListenAddress, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
		if (!host)
		{
			std::cerr << "Session: Failed to create ENET server host!\n";
			DEBUG_BREAK_ONCE
				return false;
		}

		return true;
	}

	void ClientMessaging::stopSession()
	{
		if (peer)
		{
			assert(host);

			enet_host_flush(host);
			enet_peer_disconnect(peer, 0);

			ENetEvent event;
			bool bIsPeerConnected = true;
			while (bIsPeerConnected && enet_host_service(host, &event, disconnectTimeout) > 0)
			{
				switch (event.type)
				{
				case ENET_EVENT_TYPE_RECEIVE:
					enet_packet_destroy(event.packet);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					bIsPeerConnected = false;
					break;
				}
			}
			if (bIsPeerConnected)
			{
				enet_peer_reset(peer);
			}
			peer = nullptr;
		}

		if (host)
		{
			enet_host_destroy(host);
			host = nullptr;
		}

		receivedHandshake = false;
		geometryStreamingService->reset();
	}

	void ClientMessaging::tick(float deltaTime)
	{
		//Don't stream geometry to the client before we've received the handshake.
		if (!receivedHandshake) return;

		static float timeSinceLastGeometryStream = 0;
		timeSinceLastGeometryStream += deltaTime;

		const float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f / settings->geometryTicksPerSecond;

		//Only tick the geometry streaming service a set amount of times per second.
		if (timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
		{
			geometryStreamingService->tick(TIME_BETWEEN_GEOMETRY_TICKS);

			//Tell the client to change the visibility of actors that have changed whether they are within streamable bounds.
			if (!actorsEnteredBounds.empty() || !actorsLeftBounds.empty())
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

	void ClientMessaging::handleEvents(float deltaTime)
	{
		static float timeSinceLastMessage = 0;
		timeSinceLastMessage += deltaTime;

		ENetEvent event;
		while (enet_host_service(host, &event, 0) > 0)
		{
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
				assert(!peer);
				timeSinceLastMessage = 0;
				char address[20];
				enet_address_get_host_ip(&event.peer->address, address, sizeof(address));
				peer = event.peer;
				discoveryService->discoveryCompleteForClient(clientID);
				TELEPORT_COUT << "Client connected: " << getClientIP() << ":" << getClientPort() << std::endl;
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				assert(peer == event.peer);
				timeSinceLastMessage = 0;
				TELEPORT_COUT << "Client disconnected: " << getClientIP() << ":" << getClientPort() << std::endl;
				onDisconnect();
				peer = nullptr;
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				timeSinceLastMessage = 0;
				dispatchEvent(event);
				break;
			}
		}

		// Aidan: Just trying this out for the moment for picking up when we don't receive ENET_EVENT_TYPE_DISCONNECT when we should. 
		// This should be a lot more sophisticated and not hard coded.
		if (host && peer && timeSinceLastMessage > 5)
		{
			TELEPORT_COUT << "No message received in " << timeSinceLastMessage << " seconds from " << getClientIP() << ":" << getClientPort() << " so disconnecting" << std::endl;
			onDisconnect();
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

	void ClientMessaging::updateActorMovement(std::vector<avs::MovementUpdate>& updateList)
	{
		avs::UpdateActorMovementCommand command(updateList.size());
		sendCommand<avs::MovementUpdate>(command, updateList);
	}

	bool ClientMessaging::hasHost() const
	{
		return host;
	}

	bool ClientMessaging::hasPeer() const
	{
		return peer;
	}

	bool ClientMessaging::hasReceivedHandshake() const
	{
		return casterContext->axesStandard != avs::AxesStandard::NotInitialized;
	}

	bool ClientMessaging::sendCommand(const avs::Command& avsCommand) const
	{
		assert(peer);

		size_t commandSize = avs::GetCommandSize(avsCommand.commandPayloadType);
		ENetPacket* packet = enet_packet_create(&avsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);
		assert(packet);

		return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
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
		switch (event.channelID)
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
		size_t handShakeSize = sizeof(avs::Handshake);

		avs::Handshake handshake;
		memcpy(&handshake, packet->data, handShakeSize);

		if (handshake.usingHands)
		{
			geometryStreamingService->addHandsToStream();
		}

		casterContext->axesStandard = handshake.axesStandard;

		int32_t streamingPort = getServerPort() + 1;

		if (!casterContext->NetworkPipeline)
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
			casterContext->NetworkPipeline->initialise(networkSettings, casterContext->ColorQueue.get(), casterContext->DepthQueue.get(), casterContext->GeometryQueue.get(), casterContext->AudioQueue.get());
		}

		CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
		cameraInfo.width = static_cast<float>(handshake.startDisplayInfo.width);
		cameraInfo.height = static_cast<float>(handshake.startDisplayInfo.height);
		cameraInfo.fov = handshake.FOV;
		cameraInfo.isVR = handshake.isVR;

		//Extract list of resources the client has.
		std::vector<avs::uid> clientResources(handshake.resourceCount);
		memcpy(clientResources.data(), packet->data + handShakeSize, sizeof(avs::uid)* handshake.resourceCount);

		//Confirm resources the client has told us they have.
		for (int i = 0; i < handshake.resourceCount; i++)
		{
			geometryStreamingService->confirmResource(clientResources[i]);
		}

		captureComponentDelegates.startStreaming(casterContext);
		geometryStreamingService->startStreaming(casterContext);
		receivedHandshake = true;

		//Client has nothing, thus can't show actors.
		if (handshake.resourceCount == 0)
		{
			avs::AcknowledgeHandshakeCommand ack;
			sendCommand(ack);
		}
		//Client may have required resources, as they are reconnecting; tell them to show streamed actors.
		else
		{
			std::vector<avs::uid> streamedActorIDs = geometryStreamingService->getStreamedActorIDs();

			avs::AcknowledgeHandshakeCommand ack(streamedActorIDs.size());
			sendCommand<avs::uid>(ack, streamedActorIDs);
		}

		std::cout << "RemotePlay: Started streaming to " << getClientIP() << ":" << streamingPort << std::endl;
	}

	bool ClientMessaging::setPosition(const avs::vec3& pos)
	{
		avs::SetPositionCommand setp;
		avs::vec3 p = pos;
		if (casterContext->axesStandard != avs::AxesStandard::NotInitialized)
		{
			avs::ConvertPosition(settings->axesStandard, casterContext->axesStandard, p);
			setp.position = p;
			sendCommand(setp);
		}
		else
			return false;
	}

	void ClientMessaging::receiveInput(const ENetPacket* packet)
	{
		if (packet->dataLength != sizeof(avs::InputState))
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
		if (packet->dataLength != sizeof(avs::DisplayInfo))
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
		if (packet->dataLength != sizeof(avs::Pose))
		{
			std::cout << "Session: Received malformed head pose packet of length: " << packet->dataLength << std::endl;
			return;
		}

		avs::Pose headPose;
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

		for (avs::uid id : resourceRequests)
		{
			geometryStreamingService->requestResource(id);
		}
	}

	void ClientMessaging::receiveKeyframeRequest(const ENetPacket* packet)
	{
		if (captureComponentDelegates.requestKeyframe)
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
		switch (clientMessagePayloadType)
		{
		case avs::ClientMessagePayloadType::ControllerPoses:
		{
			avs::ControllerPosesMessage message;
			memcpy(&message, packet->data, packet->dataLength);

			for (int i = 0; i < 2; i++)
			{
				avs::Pose& pose = message.controllerPoses[i];
				avs::ConvertRotation(casterContext->axesStandard, settings->axesStandard, pose.orientation);
				avs::ConvertPosition(casterContext->axesStandard, settings->axesStandard, pose.position);
				setControllerPose(clientID, i, &pose);
			}
			break;
		}
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

			for (avs::uid actorID : drawn)
			{
				geometryStreamingService->hideNode(clientID, actorID);
			}

			for (avs::uid actorID : toRelease)
			{
				geometryStreamingService->showNode(clientID, actorID);
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

			for (avs::uid id : confirmedResources)
			{
				geometryStreamingService->confirmResource(id);
			}

			break;
		}
		default:
			break;
		};
	}
}
