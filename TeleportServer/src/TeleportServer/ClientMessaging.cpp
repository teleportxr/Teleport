#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "libavstream/common_input.h"
#include "libavstream/common_networking.h"

#include "DiscoveryService.h"
#include "TeleportCore/ErrorHandling.h"
#include "ClientManager.h"

namespace teleport
{
	ClientMessaging::ClientMessaging(const struct ServerSettings* settings,
									 std::shared_ptr<DiscoveryService> discoveryService,
									 std::shared_ptr<GeometryStreamingService> geometryStreamingService,
									 SetHeadPoseFn setHeadPose,
									 SetOriginFromClientFn setOriginFromClient,
									 SetControllerPoseFn setControllerPose,
									 ProcessNewInputFn processNewInput,
									 DisconnectFn onDisconnect,
									 uint32_t disconnectTimeout,
									 ReportHandshakeFn reportHandshakeFn,
									 ClientManager* clientManager)
		: settings(settings)
		, discoveryService(discoveryService)
		, geometryStreamingService(geometryStreamingService)
		, setHeadPose(setHeadPose)
		, setOriginFromClient(setOriginFromClient)
		, setControllerPose(setControllerPose)
		, processNewInput(processNewInput)
		, onDisconnect(onDisconnect)
		, disconnectTimeout(disconnectTimeout)
		, reportHandshake(reportHandshakeFn)
		, clientManager(clientManager)
		, peer(nullptr)
		, casterContext(nullptr)
		, clientID(0)
		, startingSession(false)
		, timeStartingSession(0)
		, timeSinceLastClientComm(0)
		, streamingPort(0)
	{}

	ClientMessaging::~ClientMessaging()
	{
		
	}

	bool ClientMessaging::isInitialised() const
	{
		return initialized;
	}

	void ClientMessaging::unInitialise()
	{
		initialized = false;
	}

	void ClientMessaging::initialise(CasterContext* context, CaptureDelegates captureDelegates)
	{
		casterContext = context;
		captureComponentDelegates = captureDelegates;
		initialized = true;
	}

	bool ClientMessaging::restartSession(avs::uid clientID, std::string clientIP)
	{
		stopSession();
		
		return startSession(clientID, clientIP);
	}

	bool ClientMessaging::startSession(avs::uid clientID, std::string clientIP)
	{
		this->clientID = clientID;
		this->clientIP = clientIP;

		startingSession = true;

		clientManager->addClient(this);

		return true;
	}

	void ClientMessaging::stopSession()
	{
		clientManager->removeClient(this);

		if (peer)
		{
			/*enet_peer_disconnect(peer, 0);

			bool bIsPeerConnected = true;
			while (bIsPeerConnected && !eventQueue.empty())
			{
				ENetEvent& event = eventQueue.front();
				switch (event.type)
				{
				case ENET_EVENT_TYPE_RECEIVE:
					enet_packet_destroy(event.packet);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					bIsPeerConnected = false;
					break;
				}
				eventQueue.pop();
			}
			if (bIsPeerConnected)
			{
				enet_peer_reset(peer);
			}*/
			enet_peer_reset(peer);
			peer = nullptr;
		}

		receivedHandshake = false;
		geometryStreamingService->reset();

		eventQueue.clear();
	}

	void ClientMessaging::tick(float deltaTime)
	{
		//Don't stream geometry to the client before we've received the handshake.
		if (!receivedHandshake)
			return;

		
		if (peer && casterContext->NetworkPipeline && !casterContext->NetworkPipeline->isProcessingEnabled())
		{
			TELEPORT_COUT << "Network error occurred with client " << getClientIP() << ":" << getClientPort() << " so disconnecting." << std::endl;
			Disconnect();
			return;
		}
		

		static float timeSinceLastGeometryStream = 0;
		timeSinceLastGeometryStream += deltaTime;

		const float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f / settings->geometryTicksPerSecond;

		//Only tick the geometry streaming service a set amount of times per second.
		if (timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
		{
			geometryStreamingService->tick(TIME_BETWEEN_GEOMETRY_TICKS);

			//Tell the client to change the visibility of nodes that have changed whether they are within streamable bounds.
			if (!nodesEnteredBounds.empty() || !nodesLeftBounds.empty())
			{
				size_t commandSize = sizeof(avs::NodeBoundsCommand);
				size_t enteredBoundsSize = sizeof(avs::uid) * nodesEnteredBounds.size();
				size_t leftBoundsSize = sizeof(avs::uid) * nodesLeftBounds.size();

				avs::NodeBoundsCommand boundsCommand(nodesEnteredBounds.size(), nodesLeftBounds.size());
				ENetPacket* packet = enet_packet_create(&boundsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);

				//Resize packet, and insert node lists.
				enet_packet_resize(packet, commandSize + enteredBoundsSize + leftBoundsSize);
				memcpy(packet->data + commandSize, nodesEnteredBounds.data(), enteredBoundsSize);
				memcpy(packet->data + commandSize + enteredBoundsSize, nodesLeftBounds.data(), leftBoundsSize);

				enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);

				nodesEnteredBounds.clear();
				nodesLeftBounds.clear();
			}

			timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
		}

		if (settings->isReceivingAudio && casterContext->sourceNetworkPipeline)
		{
			casterContext->sourceNetworkPipeline->process();
		}
	}

	void ClientMessaging::handleEvents(float deltaTime)
	{
		timeSinceLastClientComm += deltaTime;

		if (startingSession)
		{
			timeStartingSession += deltaTime;
		}
		size_t eventCount = eventQueue.size();
		for(int i = 0; i < eventCount; ++i)
		{
			ENetEvent event = eventQueue.front();
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
				assert(!peer);
				timeSinceLastClientComm = 0;
				char address[20];
				enet_address_get_host_ip(&event.peer->address, address, sizeof(address));
				peer = event.peer;
				enet_peer_timeout(peer, 0, disconnectTimeout, disconnectTimeout * 6);
				discoveryService->discoveryCompleteForClient(clientID);
				TELEPORT_COUT << "Client connected: " << getClientIP() << ":" << getClientPort() << std::endl;
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				assert(peer == event.peer);
				timeSinceLastClientComm = 0;
				if (!startingSession)
				{
					TELEPORT_COUT << "Client disconnected: " << getClientIP() << ":" << getClientPort() << std::endl;
					enet_packet_destroy(event.packet);
					eventQueue.pop();
					Disconnect();
					return;
				}
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				timeSinceLastClientComm = 0;
				if (!startingSession)
				{
					dispatchEvent(event);
				}
				break;
			}
			if (event.packet)
				enet_packet_destroy(event.packet);
			eventQueue.pop();
		}

		// We may stop debugging on client and not receive an ENET_EVENT_TYPE_DISCONNECT so this should handle it. 
		if (peer && timeSinceLastClientComm > (disconnectTimeout / 1000.0f) + 2)
		{
			TELEPORT_COUT << "No message received in " << timeSinceLastClientComm << " seconds from " << getClientIP() << ":" << getClientPort() << " so disconnecting." << std::endl;
			Disconnect();
			return;
		}

		if (startingSession)
		{
			// Return because we don't want to process input until the C# side objects have been created.
			return;
		}
		{
			//Send latest input to managed code for this networking tick; we need the variables as we can't take the memory address of an rvalue.
			const avs::InputEventBinary* binaryEventsPtr = latestInputStateAndEvents.binaryEvents.data();
			const avs::InputEventAnalogue* analogueEventsPtr = latestInputStateAndEvents.analogueEvents.data();
			const avs::InputEventMotion* motionEventsPtr = latestInputStateAndEvents.motionEvents.data();
			for (auto c : latestInputStateAndEvents.analogueEvents)
			{
				TELEPORT_COUT << "processNewInput: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< std::endl;
			}
			processNewInput(clientID, &latestInputStateAndEvents.inputState, &binaryEventsPtr, &analogueEventsPtr, &motionEventsPtr);
			if(latestInputStateAndEvents.analogueEvents.size())
			{
				TELEPORT_COUT << "processNewInput sends "<<latestInputStateAndEvents.analogueEvents.size()<<" analogue events."<<latestInputStateAndEvents.inputState.numAnalogueEvents<< std::endl;
			}
		}
		//Input has been passed, so clear the events.
		latestInputStateAndEvents.clear();
	}

	void ClientMessaging::ConfirmSessionStarted()
	{
		startingSession = false;
		timeStartingSession = 0;
	}

	bool ClientMessaging::timedOutStartingSession() const
	{
		return timeStartingSession > startSessionTimeout;
	}

	void ClientMessaging::Disconnect()
	{
		onDisconnect(clientID);
		peer = nullptr;
	}

	void ClientMessaging::nodeEnteredBounds(avs::uid nodeID)
	{
		nodesEnteredBounds.push_back(nodeID);
		nodesLeftBounds.erase(std::remove(nodesLeftBounds.begin(), nodesLeftBounds.end(), nodeID), nodesLeftBounds.end());
	}

	void ClientMessaging::nodeLeftBounds(avs::uid nodeID)
	{
		nodesLeftBounds.push_back(nodeID);
		nodesEnteredBounds.erase(std::remove(nodesEnteredBounds.begin(), nodesEnteredBounds.end(), nodeID), nodesEnteredBounds.end());
	}

	void ClientMessaging::updateNodeMovement(const std::vector<avs::MovementUpdate>& updateList)
	{
		avs::UpdateNodeMovementCommand command(updateList.size());
		sendCommand<avs::MovementUpdate>(command, updateList);
	}

	void ClientMessaging::updateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList)
	{
		avs::UpdateNodeEnabledStateCommand command(updateList.size());
		sendCommand<avs::NodeUpdateEnabledState>(command, updateList);
	}

	void ClientMessaging::setNodeHighlighted(avs::uid nodeID, bool isHighlighted)
	{
		avs::SetNodeHighlightedCommand command(nodeID, isHighlighted);
		sendCommand(command);
	}

	void ClientMessaging::reparentNode(avs::uid nodeID, avs::uid newParentID,avs::Pose relPose)
	{
		avs::ConvertRotation(settings->serverAxesStandard, casterContext->axesStandard, relPose.orientation);
		avs::ConvertPosition(settings->serverAxesStandard, casterContext->axesStandard, relPose.position);
		avs::UpdateNodeStructureCommand command(nodeID, newParentID, relPose);
		sendCommand(command);
	}
	
	void ClientMessaging::setNodeSubtype( avs::uid nodeID, avs::NodeSubtype subType)
	{
		avs::UpdateNodeSubtypeCommand command(nodeID,  subType);
		sendCommand(command);
	}

	void ClientMessaging::updateNodeAnimation(avs::ApplyAnimation update)
	{
		avs::UpdateNodeAnimationCommand command(update);
		sendCommand(command);
	}

	void ClientMessaging::updateNodeAnimationControl(avs::NodeUpdateAnimationControl update)
	{
		avs::SetAnimationControlCommand command(update);
		sendCommand(command);
	}
	

	void ClientMessaging::updateNodeRenderState(avs::uid nodeID,avs::NodeRenderState update)
	{
		TELEPORT_ASSERT(false);// not implemented
	}

	void ClientMessaging::setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
	{
		avs::SetNodeAnimationSpeedCommand command(nodeID, animationID, speed);
		sendCommand(command);
	}

	bool ClientMessaging::hasPeer() const
	{
		return peer;
	}

	bool ClientMessaging::hasReceivedHandshake() const
	{
		return casterContext->axesStandard != avs::AxesStandard::NotInitialized;
	}

	bool ClientMessaging::sendCommand(const avs::Command& command) const
	{
		if(!peer)
		{
			TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! ClientMessaging has no peer!\n";
			return false;
		}

		size_t commandSize = command.getCommandSize();
		ENetPacket* packet = enet_packet_create(&command, commandSize, ENET_PACKET_FLAG_RELIABLE);
		
		if(!packet)
		{
			TELEPORT_CERR << "Failed to send command with type: " << static_cast<int>(command.commandPayloadType) << "! Failed to create packet!\n";
			return false;
		}

		return enet_peer_send(peer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet) == 0;
	}

	// Same as clientIP
	std::string ClientMessaging::getPeerIP() const
	{
		assert(peer);

		char address[20];
		if(peer)
		{
			enet_address_get_host_ip(&peer->address, address, sizeof(address));
		}
		else
		{
			sprintf(address, "");
		}

		return std::string(address);
	}

	uint16_t ClientMessaging::getClientPort() const
	{
		assert(peer);

		return peer->address.port;
	}

	void ClientMessaging::dispatchEvent(const ENetEvent& event)
	{
		switch(static_cast<avs::RemotePlaySessionChannel>(event.channelID))
		{
		case avs::RemotePlaySessionChannel::RPCH_Handshake:
			//Delay the actual start of streaming until we receive a confirmation from the client that they are ready.
			receiveHandshake(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_Control:
			receiveInput(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_DisplayInfo:
			receiveDisplayInfo(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_HeadPose:
			receiveHeadPose(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_ResourceRequest:
			receiveResourceRequest(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_KeyframeRequest:
			receiveKeyframeRequest(event.packet);
			break;
		case avs::RemotePlaySessionChannel::RPCH_ClientMessage:
			receiveClientMessage(event.packet);
			break;
		default:
			TELEPORT_CERR << "Unhandled channel " << event.channelID << std::endl;
			break;
		}
	}

	void ClientMessaging::receiveHandshake(const ENetPacket* packet)
	{
		size_t handShakeSize = sizeof(avs::Handshake);

		memcpy(&handshake, packet->data, handShakeSize);

		casterContext->axesStandard = handshake.axesStandard;

		if (!casterContext->NetworkPipeline)
		{
			std::string multibyteClientIP = getClientIP();
			size_t ipLength = strlen(multibyteClientIP.data());

			wchar_t clientIP[20];
			mbstowcs_s(&ipLength, clientIP, multibyteClientIP.data(), 20);

			CasterNetworkSettings networkSettings =
			{
				static_cast<int32_t>(streamingPort),
				clientIP,
				static_cast<int32_t>(handshake.clientStreamingPort),
				static_cast<int32_t>(handshake.maxBandwidthKpS),
				static_cast<int32_t>(handshake.udpBufferSize),
				settings->requiredLatencyMs,
				static_cast<int32_t>(disconnectTimeout)
			};

			casterContext->NetworkPipeline.reset(new NetworkPipeline(settings));
			casterContext->NetworkPipeline->initialise(networkSettings, casterContext->ColorQueue.get(), casterContext->TagDataQueue.get(), casterContext->GeometryQueue.get(), casterContext->AudioQueue.get());
		}

		if (settings->isReceivingAudio && !casterContext->sourceNetworkPipeline)
		{
			avs::NetworkSourceParams sourceParams;
			sourceParams.connectionTimeout = disconnectTimeout;
			sourceParams.remoteIP = clientIP.c_str();
			sourceParams.remotePort = handshake.clientStreamingPort;

			casterContext->sourceNetworkPipeline.reset(new SourceNetworkPipeline(settings));
			casterContext->sourceNetworkPipeline->initialize(sourceParams, casterContext->sourceAudioQueue.get(), casterContext->audioDecoder.get(), casterContext->audioTarget.get());
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
		geometryStreamingService->startStreaming(casterContext, handshake);
		receivedHandshake = true;

		//Client has nothing, thus can't show nodes.
		if (handshake.resourceCount == 0)
		{
			avs::AcknowledgeHandshakeCommand ack;
			sendCommand(ack);
		}
		// Client may have required resources, as they are reconnecting; tell them to show streamed nodes.
		else
		{
			const std::set<avs::uid>& streamedNodeIDs = geometryStreamingService->getStreamedNodeIDs();
			avs::AcknowledgeHandshakeCommand ack(streamedNodeIDs.size());
			sendCommand<avs::uid>(ack, std::vector<avs::uid>{streamedNodeIDs.begin(), streamedNodeIDs.end()});
		}
		reportHandshake(this->clientID, &handshake);
		TELEPORT_COUT << "RemotePlay: Started streaming to " << getClientIP() << ":" << streamingPort << std::endl;
	}

	bool ClientMessaging::setPosition(uint64_t valid_counter, const avs::vec3& pos, bool set_rel, const avs::vec3& rel_to_head, const avs::vec4& orientation)
	{
		avs::SetPositionCommand setp;
		if (casterContext->axesStandard != avs::AxesStandard::NotInitialized)
		{
			avs::vec3 p = pos;
			avs::ConvertPosition(settings->serverAxesStandard, casterContext->axesStandard, p);
			setp.origin_pos = p;
			setp.set_relative_pos = (uint8_t)set_rel;
			setp.orientation = orientation;
			avs::ConvertRotation(settings->serverAxesStandard, casterContext->axesStandard, setp.orientation);
			avs::vec3 o = rel_to_head;
			avs::ConvertPosition(settings->serverAxesStandard, casterContext->axesStandard, o);
			setp.relative_pos = o;
			setp.valid_counter = valid_counter;
			return sendCommand(setp);
		}
		return false;
	}

	void ClientMessaging::receiveInput(const ENetPacket* packet)
	{
		size_t inputStateSize = sizeof(avs::InputState);

		if (packet->dataLength < inputStateSize)
		{
			TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet->dataLength << "; less than minimum size of " << inputStateSize << "!\n";
			return;
		}

		avs::InputState receivedInputState;
		//Copy newest input state into member variable.
		memcpy(&receivedInputState, packet->data, inputStateSize);

		size_t binaryEventSize = sizeof(avs::InputEventBinary) * receivedInputState.numBinaryEvents;
		size_t analogueEventSize = sizeof(avs::InputEventAnalogue) * receivedInputState.numAnalogueEvents;
		size_t motionEventSize = sizeof(avs::InputEventMotion) * receivedInputState.numMotionEvents;

		if (packet->dataLength != inputStateSize + binaryEventSize + analogueEventSize + motionEventSize)
		{
			TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet->dataLength << "; expected size of " << inputStateSize + binaryEventSize + analogueEventSize + motionEventSize << "!\n" <<
				"InputState Size: " << inputStateSize << "\n" <<
				"Binary Events Size:" << binaryEventSize << "(" << receivedInputState.numBinaryEvents << ")\n" <<
				"Analogue Events Size:" << analogueEventSize << "(" << receivedInputState.numAnalogueEvents << ")\n" <<
				"Motion Events Size:" << motionEventSize << "(" << receivedInputState.numMotionEvents << ")\n";

			return;
		}
		
		latestInputStateAndEvents.inputState.add(receivedInputState);

		if(receivedInputState.numBinaryEvents != 0)
		{
			avs::InputEventBinary* binaryData = reinterpret_cast<avs::InputEventBinary*>(packet->data + inputStateSize);
			latestInputStateAndEvents.binaryEvents.insert(latestInputStateAndEvents.binaryEvents.end(), binaryData, binaryData + receivedInputState.numBinaryEvents);
		}

		if(receivedInputState.numAnalogueEvents != 0)
		{
			avs::InputEventAnalogue* analogueData = reinterpret_cast<avs::InputEventAnalogue*>(packet->data + inputStateSize + binaryEventSize);
			latestInputStateAndEvents.analogueEvents.insert(latestInputStateAndEvents.analogueEvents.end(), analogueData, analogueData + receivedInputState.numAnalogueEvents);
			for (auto c : latestInputStateAndEvents.analogueEvents)
			{
				TELEPORT_COUT << "Analogue: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< std::endl;
			}
		}

		if(receivedInputState.numMotionEvents != 0)
		{
			avs::InputEventMotion* motionData = reinterpret_cast<avs::InputEventMotion*>(packet->data + inputStateSize + binaryEventSize + analogueEventSize);
			latestInputStateAndEvents.motionEvents.insert(latestInputStateAndEvents.motionEvents.end(), motionData, motionData + receivedInputState.numMotionEvents);
		}
	}

	void ClientMessaging::receiveDisplayInfo(const ENetPacket* packet)
	{
		if (packet->dataLength != sizeof(avs::DisplayInfo))
		{
			TELEPORT_COUT << "Session: Received malformed display info packet of length: " << packet->dataLength << std::endl;
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
			TELEPORT_COUT << "Session: Received malformed head pose packet of length: " << packet->dataLength << std::endl;
			return;
		}

		avs::Pose headPose;
		memcpy(&headPose, packet->data, packet->dataLength);

		avs::ConvertRotation(casterContext->axesStandard, settings->serverAxesStandard, headPose.orientation);
		avs::ConvertPosition(casterContext->axesStandard, settings->serverAxesStandard, headPose.position);
		setHeadPose(clientID, &headPose);
	}

	void ClientMessaging::receiveResourceRequest(const ENetPacket* packet)
	{
		size_t resourceCount;
		memcpy(&resourceCount, packet->data, sizeof(size_t));

		std::vector<avs::uid> resourceRequests(resourceCount);
		memcpy(resourceRequests.data(), packet->data + sizeof(size_t), sizeof(avs::uid) * resourceCount);

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
			TELEPORT_COUT << "Received keyframe request, but capture component isn't set.\n";
		}
	}

	void ClientMessaging::receiveClientMessage(const ENetPacket* packet)
	{
		avs::ClientMessagePayloadType clientMessagePayloadType = *(reinterpret_cast<avs::ClientMessagePayloadType*>(packet->data) + sizeof(void*));
		switch (clientMessagePayloadType)
		{
		case avs::ClientMessagePayloadType::OriginPose:
		{
			avs::OriginPoseMessage message;
			memcpy(&message, packet->data, packet->dataLength);
			avs::ConvertRotation(casterContext->axesStandard, settings->serverAxesStandard, message.originPose.orientation);
			avs::ConvertPosition(casterContext->axesStandard, settings->serverAxesStandard, message.originPose.position);
			setOriginFromClient(clientID, message.counter, &message.originPose);
		}
		break;
		case avs::ClientMessagePayloadType::ControllerPoses:
		{
			avs::ControllerPosesMessage message;
			memcpy(&message, packet->data, packet->dataLength);

			avs::ConvertRotation(casterContext->axesStandard, settings->serverAxesStandard, message.headPose.orientation);
			avs::ConvertPosition(casterContext->axesStandard, settings->serverAxesStandard, message.headPose.position);
			setHeadPose(clientID, &message.headPose);
			for (int i = 0; i < 2; i++)
			{
				avs::Pose& pose = message.controllerPoses[i];
				avs::ConvertRotation(casterContext->axesStandard, settings->serverAxesStandard, pose.orientation);
				avs::ConvertPosition(casterContext->axesStandard, settings->serverAxesStandard, pose.position);
				setControllerPose(clientID, i, &pose);
			}
			break;
		}
		case avs::ClientMessagePayloadType::NodeStatus:
		{
			size_t messageSize = sizeof(avs::NodeStatusMessage);
			avs::NodeStatusMessage message;
			memcpy(&message, packet->data, messageSize);

			size_t drawnSize = sizeof(avs::uid) * message.nodesDrawnCount;
			std::vector<avs::uid> drawn(message.nodesDrawnCount);
			memcpy(drawn.data(), packet->data + messageSize, drawnSize);

			size_t toReleaseSize = sizeof(avs::uid) * message.nodesWantToReleaseCount;
			std::vector<avs::uid> toRelease(message.nodesWantToReleaseCount);
			memcpy(toRelease.data(), packet->data + messageSize + drawnSize, toReleaseSize);

			for (avs::uid nodeID : drawn)
			{
				geometryStreamingService->hideNode(clientID, nodeID);
			}

			for (avs::uid nodeID : toRelease)
			{
				geometryStreamingService->showNode(clientID, nodeID);
			}

			break;
		}
		case avs::ClientMessagePayloadType::ReceivedResources:
		{
			size_t messageSize = sizeof(avs::ReceivedResourcesMessage);
			avs::ReceivedResourcesMessage message;
			memcpy(&message, packet->data, messageSize);

			size_t confirmedResourcesSize = sizeof(avs::uid) * message.receivedResourcesCount;
			std::vector<avs::uid> confirmedResources(message.receivedResourcesCount);
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

	uint16_t ClientMessaging::getServerPort() const
	{
		assert(clientManager);

		return clientManager->getServerPort();
	}

	uint16_t ClientMessaging::getStreamingPort() const
	{
		return streamingPort;
	}
}

