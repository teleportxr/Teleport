#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common_input.h"
#include "libavstream/common_networking.h"

#include "DiscoveryService.h"
#include "ErrorHandling.h"

namespace teleport
{
	std::atomic_bool ClientMessaging::asyncNetworkDataProcessingActive = false;
	std::unordered_map<avs::uid, NetworkPipeline*> ClientMessaging::networkPipelines;
	std::thread ClientMessaging::networkThread;
	std::mutex ClientMessaging::networkMutex;
	std::mutex ClientMessaging::dataMutex;
	avs::Timestamp ClientMessaging::lastTickTimestamp;

	ClientMessaging::ClientMessaging(const struct ServerSettings* settings,
									 std::shared_ptr<DiscoveryService> discoveryService,
									 std::shared_ptr<GeometryStreamingService> geometryStreamingService,
									 SetHeadPoseFn setHeadPose,
									 SetOriginFromClientFn setOriginFromClient,
									 SetControllerPoseFn setControllerPose,
									 ProcessNewInputFn processNewInput,
									 DisconnectFn onDisconnect,
									 const uint32_t& disconnectTimeout,
									 ReportHandshakeFn reportHandshakeFn)
		: settings(settings)
		, discoveryService(discoveryService)
		, geometryStreamingService(geometryStreamingService)
		, setHeadPose(setHeadPose)
		, setOriginFromClient(setOriginFromClient)
		, setControllerPose(setControllerPose)
		, processNewInput(processNewInput)
		, onDisconnect(onDisconnect)
		, reportHandshake(reportHandshakeFn)
		, disconnectTimeout(disconnectTimeout)
		, host(nullptr)
		, peer(nullptr)
		, casterContext(nullptr)
		, clientID(0)
		, startingSession(false)
		, timeStartingSession(0)
		, timeSinceLastClientComm(0)
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

	bool ClientMessaging::restartSession(avs::uid clientID, int32_t listenPort)
	{
		stopSession();
		
		if (host)
		{
			enet_host_destroy(host);
			host = nullptr;
		}
		return startSession(clientID, listenPort);
	}

	bool ClientMessaging::startSession(avs::uid clientID, int32_t listenPort)
	{
		this->clientID = clientID;

		ENetAddress ListenAddress = {};
		ListenAddress.host = ENET_HOST_ANY;
		ListenAddress.port = listenPort;

		// ServerHost will live for the lifetime of the session.
		if (host)
		{
			TELEPORT_COUT << "startSession - already have host ptr.\n";
		}
		else
		{
			host = enet_host_create(&ListenAddress, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
		}
		if (!host)
		{
			host = enet_host_create(&ListenAddress, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
			if (!host)
			{
				std::cerr << "Session: Failed to create ENET server host!\n";
				DEBUG_BREAK_ONCE;
				return false;
			}
		}

		startingSession = true;

		return true;
	}

	void ClientMessaging::stopSession()
	{
		removeNetworkPipelineFromAsyncProcessing();

		if (peer)
		{
			assert(host);

			enet_host_flush(host);
			enet_peer_disconnect(peer, 0);

			ENetEvent event;
			bool bIsPeerConnected = true;
			while (bIsPeerConnected && enet_host_service(host, &event, 5) > 0)
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
		if (!receivedHandshake)
			return;

		
		if (host && peer && casterContext->NetworkPipeline && !casterContext->NetworkPipeline->isProcessingEnabled())
		{
			TELEPORT_COUT << "Network error occurred with client " << getClientIP() << ":" << getClientPort() << " so disconnecting." << std::endl;
			Disconnect();
			return;
		}
		

		static float timeSinceLastGeometryStream = 0;
		timeSinceLastGeometryStream += deltaTime;

		{
			std::lock_guard<std::mutex> guard(dataMutex);
			lastTickTimestamp = avs::PlatformWindows::getTimestamp();
		}


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
		
		ENetEvent event;
		while (enet_host_service(host, &event, 0) > 0)
		{
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
		}

		// We may stop debugging on client and not receive an ENET_EVENT_TYPE_DISCONNECT so this should handle it. 
		if (host && peer && timeSinceLastClientComm > (disconnectTimeout / 1000.0f) + 2)
		{
			TELEPORT_COUT << "No message received in " << timeSinceLastClientComm << " seconds from " << getClientIP() << ":" << getClientPort() << " so disconnecting." << std::endl;
			Disconnect();
		}

		if (startingSession)
		{
			// Return because we don't want to process input until the C# side objects have been created.
			return;
		}
		{
			//Send latest input to managed code for this networking tick; we need the variables as we can't take the memory address of an rvalue.
			const avs::InputEventBinary* binaryEventsPtr = latestInputStateAndEvents[0].binaryEvents.data();
			const avs::InputEventAnalogue* analogueEventsPtr = latestInputStateAndEvents[0].analogueEvents.data();
			const avs::InputEventMotion* motionEventsPtr = latestInputStateAndEvents[0].motionEvents.data();
			for (auto c : latestInputStateAndEvents[0].analogueEvents)
			{
				TELEPORT_COUT << "processNewInput: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< std::endl;
			}
			processNewInput(clientID, &latestInputStateAndEvents[0].inputState, &binaryEventsPtr, &analogueEventsPtr, &motionEventsPtr);
			if(latestInputStateAndEvents[0].analogueEvents.size())
			{
				TELEPORT_COUT << "processNewInput sends "<<latestInputStateAndEvents[0].analogueEvents.size()<<" analogue events."<<latestInputStateAndEvents[0].inputState.numAnalogueEvents<< std::endl;
			}
		}
		//Input has been passed, so clear the events.
		latestInputStateAndEvents[0].clear();

		{
			const avs::InputEventBinary* binaryEventsPtr = latestInputStateAndEvents[1].binaryEvents.data();
			const avs::InputEventAnalogue* analogueEventsPtr = latestInputStateAndEvents[1].analogueEvents.data();
			const avs::InputEventMotion* motionEventsPtr = latestInputStateAndEvents[1].motionEvents.data();
			processNewInput(clientID, &latestInputStateAndEvents[1].inputState, &binaryEventsPtr, &analogueEventsPtr, &motionEventsPtr);
		}
		latestInputStateAndEvents[1].clear();
	}

	void ClientMessaging::ConfirmSessionStarted()
	{
		startingSession = false;
		timeStartingSession = 0;
	}

	bool ClientMessaging::TimedOutStartingSession() const
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

	std::string ClientMessaging::getClientIP() const
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

	uint16_t ClientMessaging::getServerPort() const
	{
		assert(host);

		return host->address.port;
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
		enet_packet_destroy(event.packet);
	}

	void ClientMessaging::receiveHandshake(const ENetPacket* packet)
	{
		size_t handShakeSize = sizeof(avs::Handshake);

		memcpy(&handshake, packet->data, handShakeSize);

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
				static_cast<int32_t>(handshake.clientStreamingPort),
				static_cast<int32_t>(handshake.maxBandwidthKpS),
				static_cast<int32_t>(handshake.udpBufferSize),
				settings->requiredLatencyMs,
				static_cast<int32_t>(disconnectTimeout)
			};

			casterContext->NetworkPipeline.reset(new NetworkPipeline(settings));
			casterContext->NetworkPipeline->initialise(networkSettings, casterContext->ColorQueue.get(), casterContext->TagDataQueue.get(), casterContext->GeometryQueue.get(), casterContext->AudioQueue.get());
		}

		addNetworkPipelineToAsyncProcessing();

		if (settings->isReceivingAudio && !casterContext->sourceNetworkPipeline)
		{
			std::string clientIP = getClientIP();

			avs::NetworkSourceParams sourceParams;
			sourceParams.connectionTimeout = disconnectTimeout;
			sourceParams.localPort = streamingPort;
			sourceParams.remoteIP = clientIP.c_str();
			sourceParams.remotePort = streamingPort + 1;

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

		InputStateAndEvents &aggregateInputState = latestInputStateAndEvents[receivedInputState.controllerId];
		
		aggregateInputState.inputState.add(receivedInputState);

		if(receivedInputState.numBinaryEvents != 0)
		{
			avs::InputEventBinary* binaryData = reinterpret_cast<avs::InputEventBinary*>(packet->data + inputStateSize);
			aggregateInputState.binaryEvents.insert(aggregateInputState.binaryEvents.end(), binaryData, binaryData + receivedInputState.numBinaryEvents);
		}

		if(receivedInputState.numAnalogueEvents != 0)
		{
			avs::InputEventAnalogue* analogueData = reinterpret_cast<avs::InputEventAnalogue*>(packet->data + inputStateSize + binaryEventSize);
			aggregateInputState.analogueEvents.insert(aggregateInputState.analogueEvents.end(), analogueData, analogueData + receivedInputState.numAnalogueEvents);
			for (auto c : aggregateInputState.analogueEvents)
			{
				TELEPORT_COUT << "Analogue: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< std::endl;
			}
		}

		if(receivedInputState.numMotionEvents != 0)
		{
			avs::InputEventMotion* motionData = reinterpret_cast<avs::InputEventMotion*>(packet->data + inputStateSize + binaryEventSize + analogueEventSize);
			aggregateInputState.motionEvents.insert(aggregateInputState.motionEvents.end(), motionData, motionData + receivedInputState.numMotionEvents);
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

	void ClientMessaging::addNetworkPipelineToAsyncProcessing()
	{
		std::lock_guard<std::mutex> lock(networkMutex);
		networkPipelines[clientID] = casterContext->NetworkPipeline.get();
	}

	void ClientMessaging::removeNetworkPipelineFromAsyncProcessing()
	{
		std::lock_guard<std::mutex> lock(networkMutex);
		if (networkPipelines.find(clientID) != networkPipelines.end())
		{
			networkPipelines.erase(clientID);
		}
	}

	void ClientMessaging::startAsyncNetworkDataProcessing()
	{
		if (!asyncNetworkDataProcessingActive)
		{
			asyncNetworkDataProcessingActive = true;
			if (!networkThread.joinable())
			{
				{
					std::lock_guard<std::mutex> guard(dataMutex);
					lastTickTimestamp = avs::PlatformWindows::getTimestamp();
				}
				networkThread = std::thread(&ClientMessaging::processNetworkDataAsync);
			}
		}
	}
	bool ClientMessaging::asyncNetworkDataProcessingFailed = false;

	void ClientMessaging::stopAsyncNetworkDataProcessing(bool killThread)
	{
		if (asyncNetworkDataProcessingActive)
		{
			asyncNetworkDataProcessingActive = false;
			if (killThread && networkThread.joinable())
			{
				networkThread.join();
			}
		}
		else if (networkThread.joinable())
		{
			networkThread.join();
		}
	}

	void ClientMessaging::processNetworkDataAsync()
	{
		asyncNetworkDataProcessingFailed = false;
		// Elapsed time since the main thread last ticked (seconds).
		avs::Timestamp timestamp;
		double elapsedTime;
		while (asyncNetworkDataProcessingActive)
		{
			// Only continue processing if the main thread hasn't hung.
			timestamp = avs::PlatformWindows::getTimestamp();
			{
				std::lock_guard<std::mutex> lock(dataMutex);
				elapsedTime = avs::PlatformWindows::getTimeElapsedInSeconds(lastTickTimestamp, timestamp);
			}

			// Proceed only if the main thread hasn't hung.
			if (elapsedTime < 0.20)
			{
				std::lock_guard<std::mutex> lock(networkMutex);
				for (auto& keyVal : networkPipelines)
				{
					if (keyVal.second)
					{
						if (!keyVal.second->process())
						{
							asyncNetworkDataProcessingFailed = true;
						}
					}
				}
			}
		}
	}

	avs::Timestamp ClientMessaging::getLastTickTimestamp()
	{
		std::lock_guard<std::mutex> guard(dataMutex);
		return lastTickTimestamp;
	}

}

