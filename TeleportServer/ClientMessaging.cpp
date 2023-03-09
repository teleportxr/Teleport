#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "libavstream/common_input.h"
#include "TeleportCore/CommonNetworking.h"

#include "DiscoveryService.h"
#include "TeleportCore/ErrorHandling.h"
#include "ClientManager.h"
#include "StringFunctions.h"

using namespace teleport;
using namespace server;

ClientMessaging::ClientMessaging(const struct ServerSettings* settings,
								 std::shared_ptr<DiscoveryService> discoveryService,
								 SetHeadPoseFn setHeadPose,
								 SetControllerPoseFn setControllerPose,
								 ProcessNewInputFn processNewInput,
								 DisconnectFn onDisconnect,
								 uint32_t disconnectTimeout,
								 ReportHandshakeFn reportHandshakeFn,
								 ClientManager* clientManager)
	: settings(settings)
	, discoveryService(discoveryService)
	, geometryStreamingService(settings)
	, clientManager(clientManager)
	, setHeadPose(setHeadPose)
	, setControllerPose(setControllerPose)
	, processNewInput(processNewInput)
	, onDisconnect(onDisconnect)
	, reportHandshake(reportHandshakeFn)
	, disconnectTimeout(disconnectTimeout)
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

void ClientMessaging::initialise(ClientNetworkContext* context, CaptureDelegates captureDelegates)
{
	clientNetworkContext = context;
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
		TELEPORT_COUT << "Stopping session." << std::endl;
		enet_peer_reset(peer);
		peer = nullptr;
	}

	receivedHandshake = false;
	geometryStreamingService.reset();

	eventQueue.clear();
}

void ClientMessaging::tick(float deltaTime)
{
	//Don't stream geometry to the client before we've received the handshake.
	if (!receivedHandshake)
		return;

	
	if (peer && clientNetworkContext->NetworkPipeline && !clientNetworkContext->NetworkPipeline->isProcessingEnabled())
	{
		TELEPORT_COUT << "Network error occurred with client " << getClientIP() << ":" << getClientPort() << " so disconnecting." << "\n";
		Disconnect();
		return;
	}
	

	static float timeSinceLastGeometryStream = 0;
	timeSinceLastGeometryStream += deltaTime;

	float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f/settings->geometryTicksPerSecond;

	//Only tick the geometry streaming service a set amount of times per second.
	if (timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
	{
		geometryStreamingService.tick(TIME_BETWEEN_GEOMETRY_TICKS);

		//Tell the client to change the visibility of nodes that have changed whether they are within streamable bounds.
		if (!nodesEnteredBounds.empty() || !nodesLeftBounds.empty())
			{
				size_t commandSize = sizeof(teleport::core::NodeVisibilityCommand);
				size_t enteredBoundsSize = sizeof(avs::uid) * nodesEnteredBounds.size();
				size_t leftBoundsSize = sizeof(avs::uid) * nodesLeftBounds.size();

				teleport::core::NodeVisibilityCommand boundsCommand(nodesEnteredBounds.size(), nodesLeftBounds.size());
				ENetPacket* packet = enet_packet_create(&boundsCommand, commandSize, ENET_PACKET_FLAG_RELIABLE);

				//Resize packet, and insert node lists.
				enet_packet_resize(packet, commandSize + enteredBoundsSize + leftBoundsSize);
				memcpy(packet->data + commandSize, nodesEnteredBounds.data(), enteredBoundsSize);
				memcpy(packet->data + commandSize + enteredBoundsSize, nodesLeftBounds.data(), leftBoundsSize);

				enet_peer_send(peer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet);

				nodesEnteredBounds.clear();
				nodesLeftBounds.clear();
			}

		timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
	}

	if (settings->isReceivingAudio && clientNetworkContext->sourceNetworkPipeline)
	{
		clientNetworkContext->sourceNetworkPipeline->process();
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
			TELEPORT_COUT << clientID<<": Client Enet connected: " << getClientIP() << ":" << getClientPort() << "\n";
			break;
		case ENET_EVENT_TYPE_DISCONNECT:
			assert(peer == event.peer);
			timeSinceLastClientComm = 0;
			if (!startingSession)
			{
				TELEPORT_COUT << clientID << ": Client Enet disconnected: " << getClientIP() << ":" << getClientPort() << "\n";
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
				receive(event);
			}
			break;
		default:
			break;
		}
		if (event.packet)
			enet_packet_destroy(event.packet);
		eventQueue.pop();
	}

	// We may stop debugging on client and not receive an ENET_EVENT_TYPE_DISCONNECT so this should handle it. 
	if (peer && timeSinceLastClientComm > (disconnectTimeout / 1000.0f) + 2)
	{
		TELEPORT_COUT << "No message received in " << timeSinceLastClientComm << " seconds from " << getClientIP() << ":" << getClientPort() << " so disconnecting." << "\n";
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
		const uint8_t* binaryStatesPtr = latestInputStateAndEvents.binaryStates.data();
		const float* analogueStatesPtr = latestInputStateAndEvents.analogueStates.data();
		const avs::InputEventBinary* binaryEventsPtr		= latestInputStateAndEvents.binaryEvents.data();
		const avs::InputEventAnalogue* analogueEventsPtr	= latestInputStateAndEvents.analogueEvents.data();
		const avs::InputEventMotion* motionEventsPtr		= latestInputStateAndEvents.motionEvents.data();
		for (auto c : latestInputStateAndEvents.analogueEvents)
		{
			TELEPORT_COUT << "processNewInput: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< "\n";
		}
		for (int i=0;i<latestInputStateAndEvents.analogueStates.size();i++)
		{
			float &f=latestInputStateAndEvents.analogueStates[i];
			if(f<-1.f||f>1.f||isnan(f))
			{
				TELEPORT_CERR<<"Bad analogue state value "<<f<<std::endl;
				f=0;
			}
		}
		teleport::core::InputState inputState;
		inputState.numBinaryStates		=(uint16_t)latestInputStateAndEvents.binaryStates.size();
		inputState.numAnalogueStates	=(uint16_t)latestInputStateAndEvents.analogueStates.size();
		inputState.numBinaryEvents		=(uint16_t)latestInputStateAndEvents.binaryEvents.size();
		inputState.numAnalogueEvents	=(uint16_t)latestInputStateAndEvents.analogueEvents.size();
		inputState.numMotionEvents		=(uint16_t)latestInputStateAndEvents.motionEvents.size();
		processNewInput(clientID, &inputState,&binaryStatesPtr,&analogueStatesPtr
								,&binaryEventsPtr, &analogueEventsPtr, &motionEventsPtr);
	}
	//Input has been passed, so clear the events.
	latestInputStateAndEvents.clearEvents();
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

void ClientMessaging::updateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList)
{
	teleport::core::UpdateNodeMovementCommand command(updateList.size());
	sendCommand<>(command, updateList);
}

void ClientMessaging::updateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList)
{
	teleport::core::UpdateNodeEnabledStateCommand command(updateList.size());
	sendCommand<>(command, updateList);
}

void ClientMessaging::setNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	teleport::core::SetNodeHighlightedCommand command(nodeID, isHighlighted);
	sendCommand(command);
}

void ClientMessaging::reparentNode(avs::uid nodeID, avs::uid newParentID,avs::Pose relPose)
{
	avs::ConvertRotation(settings->serverAxesStandard, clientNetworkContext->axesStandard, relPose.orientation);
	avs::ConvertPosition(settings->serverAxesStandard, clientNetworkContext->axesStandard, relPose.position);
	teleport::core::UpdateNodeStructureCommand command(nodeID, newParentID, relPose);
	sendCommand(command);
}

void ClientMessaging::setNodePosePath(avs::uid nodeID,const std::string &regexPosePath)
{
	teleport::core::AssignNodePosePathCommand command(nodeID, (uint16_t)regexPosePath.size());
	std::vector<char> chars;
	chars.resize(regexPosePath.size());
	memcpy(chars.data(),regexPosePath.data(),chars.size());
	sendCommand(command,chars);
}

void ClientMessaging::updateNodeAnimation(teleport::core::ApplyAnimation update)
{
	teleport::core::UpdateNodeAnimationCommand command(update);
	sendCommand(command);
}

void ClientMessaging::updateNodeAnimationControl(teleport::core::NodeUpdateAnimationControl update)
{
	teleport::core::SetAnimationControlCommand command(update);
	sendCommand(command);
}


void ClientMessaging::updateNodeRenderState(avs::uid nodeID,avs::NodeRenderState update)
{
	TELEPORT_ASSERT(false);// not implemented
}

void ClientMessaging::setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	teleport::core::SetNodeAnimationSpeedCommand command(nodeID, animationID, speed);
	sendCommand(command);
}

bool ClientMessaging::hasPeer() const
{
	return peer;
}

bool ClientMessaging::hasReceivedHandshake() const
{
	return clientNetworkContext->axesStandard != avs::AxesStandard::NotInitialized;
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
		address[0]=0;
	}

	return std::string(address);
}

uint16_t ClientMessaging::getClientPort() const
{
	assert(peer);

	return peer->address.port;
}

void ClientMessaging::receive(const ENetEvent& event)
{
	switch(static_cast<teleport::core::RemotePlaySessionChannel>(event.channelID))
	{
	case teleport::core::RemotePlaySessionChannel::RPCH_Handshake:
		//Delay the actual start of streaming until we receive a confirmation from the client that they are ready.
		receiveHandshake(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_Control:
		receiveInput(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_DisplayInfo:
		receiveDisplayInfo(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_HeadPose:
		receiveHeadPose(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_ResourceRequest:
		receiveResourceRequest(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_KeyframeRequest:
		receiveKeyframeRequest(event.packet);
		break;
	case teleport::core::RemotePlaySessionChannel::RPCH_ClientMessage:
		receiveClientMessage(event.packet);
		break;
	default:
		TELEPORT_CERR << "Unhandled channel " << event.channelID << "\n";
		break;
	}
}

void ClientMessaging::receiveHandshake(const ENetPacket* packet)
{
	size_t handShakeSize = sizeof(teleport::core::Handshake);

	memcpy(&handshake, packet->data, handShakeSize);

	clientNetworkContext->axesStandard = handshake.axesStandard;

	if (!clientNetworkContext->NetworkPipeline)
	{
		std::string multibyteClientIP = getClientIP();
		//size_t ipLength = strlen(multibyteClientIP.data());

		std::wstring clientIP=StringToWString(multibyteClientIP);
		CasterNetworkSettings networkSettings =
		{
			static_cast<int32_t>(streamingPort),
			clientIP.c_str(),
			static_cast<int32_t>(handshake.clientStreamingPort),
			static_cast<int32_t>(handshake.maxBandwidthKpS),
			static_cast<int32_t>(handshake.udpBufferSize),
			settings->requiredLatencyMs,
			static_cast<int32_t>(disconnectTimeout)
		};

		clientNetworkContext->NetworkPipeline.reset(new NetworkPipeline(settings));
		clientNetworkContext->NetworkPipeline->initialise(networkSettings, clientNetworkContext->ColorQueue.get(), clientNetworkContext->TagDataQueue.get(), clientNetworkContext->GeometryQueue.get(), clientNetworkContext->AudioQueue.get());
	}
	TELEPORT_COUT << "Received handshake from clientID" << clientID << " at IP " << clientIP.c_str() << " .\n";

	if (settings->isReceivingAudio && !clientNetworkContext->sourceNetworkPipeline)
	{
		avs::NetworkSourceParams sourceParams;
		sourceParams.connectionTimeout = disconnectTimeout;
		sourceParams.remoteIP = clientIP.c_str();
		sourceParams.remotePort = handshake.clientStreamingPort;

		clientNetworkContext->sourceNetworkPipeline.reset(new SourceNetworkPipeline(settings));
		clientNetworkContext->sourceNetworkPipeline->initialize(sourceParams, clientNetworkContext->sourceAudioQueue.get(), clientNetworkContext->audioDecoder.get(), clientNetworkContext->audioTarget.get());
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
		geometryStreamingService.confirmResource(clientResources[i]);
	}

	captureComponentDelegates.startStreaming(clientNetworkContext);
	geometryStreamingService.startStreaming(clientNetworkContext, handshake);
	receivedHandshake = true;

	//Client has nothing, thus can't show nodes.
	if (handshake.resourceCount == 0)
	{
		teleport::core::AcknowledgeHandshakeCommand ack;
		TELEPORT_COUT << "Sending handshake acknowledgement to clientID" << clientID << " at IP " << clientIP.c_str() << " .\n";

		sendCommand(ack);
	}
	// Client may have required resources, as they are reconnecting; tell them to show streamed nodes.
	else
	{
		const std::set<avs::uid>& streamedNodeIDs = geometryStreamingService.getStreamedNodeIDs();
		teleport::core::AcknowledgeHandshakeCommand ack(streamedNodeIDs.size());
		sendCommand<>(ack, std::vector<avs::uid>{streamedNodeIDs.begin(), streamedNodeIDs.end()});
	}
	reportHandshake(this->clientID, &handshake);
	TELEPORT_COUT << "RemotePlay: Started streaming to " << getClientIP() << ":" << streamingPort << "\n";
}

bool ClientMessaging::setOrigin(uint64_t valid_counter, avs::uid originNode)
{
	teleport::core::SetStageSpaceOriginNodeCommand setp;
	if (clientNetworkContext->axesStandard != avs::AxesStandard::NotInitialized)
	{
		setp.origin_node=originNode;
		setp.valid_counter = valid_counter;
		return sendCommand(setp);
	}
	return false;
}

void ClientMessaging::receiveInput(const ENetPacket* packet)
{
	size_t inputStateSize = sizeof(teleport::core::InputState);

	if (packet->dataLength < inputStateSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet->dataLength << "; less than minimum size of " << inputStateSize << "!\n";
		return;
	}

	teleport::core::InputState receivedInputState;
	//Copy newest input state into member variable.
	memcpy(&receivedInputState, packet->data, inputStateSize);
	
	size_t binaryStateSize		= receivedInputState.numBinaryStates;
	size_t analogueStateSize	= sizeof(float)*receivedInputState.numAnalogueStates;
	size_t binaryEventSize		= sizeof(avs::InputEventBinary) * receivedInputState.numBinaryEvents;
	size_t analogueEventSize	= sizeof(avs::InputEventAnalogue) * receivedInputState.numAnalogueEvents;
	size_t motionEventSize		= sizeof(avs::InputEventMotion) * receivedInputState.numMotionEvents;

	if (packet->dataLength != inputStateSize +binaryStateSize+analogueStateSize+ binaryEventSize + analogueEventSize + motionEventSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet->dataLength << "; expected size of " << inputStateSize + binaryEventSize + analogueEventSize + motionEventSize << "!\n" <<
			"     InputState Size: " << inputStateSize << "\n" <<
			"  Binary States Size:" << binaryStateSize << "(" << receivedInputState.numBinaryStates << ")\n" <<
			"Analogue States Size:" << analogueStateSize << "(" << receivedInputState.numAnalogueStates << ")\n" <<
			"  Binary Events Size:" << binaryEventSize << "(" << receivedInputState.numBinaryEvents << ")\n" <<
			"Analogue Events Size:" << analogueEventSize << "(" << receivedInputState.numAnalogueEvents << ")\n" <<
			"  Motion Events Size:" << motionEventSize << "(" << receivedInputState.numMotionEvents << ")\n";

		return;
	}
	latestInputStateAndEvents.analogueStates.resize(receivedInputState.numAnalogueStates);
	latestInputStateAndEvents.binaryStates.resize(binaryStateSize);
	uint8_t *src=packet->data+inputStateSize;
	if(receivedInputState.numBinaryStates != 0)
	{
		memcpy(latestInputStateAndEvents.binaryStates.data(), src, binaryStateSize);
		src+=binaryStateSize;
	}
	if(receivedInputState.numAnalogueStates != 0)
	{
		memcpy(latestInputStateAndEvents.analogueStates.data(), src, analogueStateSize);
		src+=analogueStateSize;
		for (int i=0;i<latestInputStateAndEvents.analogueStates.size();i++)
		{
			float &f=latestInputStateAndEvents.analogueStates[i];
			if(f<-1.f||f>1.f||isnan(f))
			{
				TELEPORT_CERR<<"Bad analogue state value "<<f<<std::endl;
				f=0;
			}
		}
	}
	if(receivedInputState.numBinaryEvents != 0)
	{
		avs::InputEventBinary* binaryData = reinterpret_cast<avs::InputEventBinary*>(src);
		latestInputStateAndEvents.binaryEvents.insert(latestInputStateAndEvents.binaryEvents.end(), binaryData, binaryData + receivedInputState.numBinaryEvents);
		src+=binaryEventSize;
	}
	if(receivedInputState.numAnalogueEvents != 0)
	{
		avs::InputEventAnalogue* analogueData = reinterpret_cast<avs::InputEventAnalogue*>(src);
		latestInputStateAndEvents.analogueEvents.insert(latestInputStateAndEvents.analogueEvents.end(), analogueData, analogueData + receivedInputState.numAnalogueEvents);
		for (auto c : latestInputStateAndEvents.analogueEvents)
		{
			TELEPORT_COUT << "Analogue: "<<c.eventID <<" "<<(int)c.inputID<<" "<<c.strength<< "\n";
		}
		src+=analogueEventSize;
	}

	if(receivedInputState.numMotionEvents != 0)
	{
		avs::InputEventMotion* motionData = reinterpret_cast<avs::InputEventMotion*>(src);
		latestInputStateAndEvents.motionEvents.insert(latestInputStateAndEvents.motionEvents.end(), motionData, motionData + receivedInputState.numMotionEvents);
	}
}

void ClientMessaging::receiveDisplayInfo(const ENetPacket* packet)
{
	if (packet->dataLength != sizeof(avs::DisplayInfo))
	{
		TELEPORT_COUT << "Session: Received malformed display info packet of length: " << packet->dataLength << "\n";
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
		TELEPORT_COUT << "Session: Received malformed head pose packet of length: " << packet->dataLength << "\n";
		return;
	}

	avs::Pose headPose;
	memcpy(&headPose, packet->data, packet->dataLength);

	avs::ConvertRotation(clientNetworkContext->axesStandard, settings->serverAxesStandard, headPose.orientation);
	avs::ConvertPosition(clientNetworkContext->axesStandard, settings->serverAxesStandard, headPose.position);
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
		geometryStreamingService.requestResource(id);
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
	teleport::core::ClientMessagePayloadType clientMessagePayloadType = *(reinterpret_cast<teleport::core::ClientMessagePayloadType*>(packet->data));
	switch (clientMessagePayloadType)
	{
	case teleport::core::ClientMessagePayloadType::ControllerPoses:
	{
		teleport::core::ControllerPosesMessage message;
		if(packet->dataLength<sizeof(message))
		{
			TELEPORT_CERR << "Bad packet size.\n";
			return;
		}
		memcpy(&message, packet->data, sizeof(message));
		if(packet->dataLength!=sizeof(message)+sizeof(teleport::core::NodePose)*message.numPoses)
		{
			TELEPORT_CERR << "Bad packet size.\n";
			return;
		}
		avs::ConvertRotation(clientNetworkContext->axesStandard, settings->serverAxesStandard, message.headPose.orientation);
		avs::ConvertPosition(clientNetworkContext->axesStandard, settings->serverAxesStandard, message.headPose.position);
		setHeadPose(clientID, &message.headPose);
		uint8_t *src=packet->data+sizeof(message);
		for (int i = 0; i < message.numPoses; i++)
		{
			teleport::core::NodePose nodePose;
			memcpy(&nodePose,src,sizeof(nodePose));
			src+=sizeof(nodePose);
			avs::PoseDynamic &nodePoseDynamic=nodePose.poseDynamic;
			avs::ConvertRotation(clientNetworkContext->axesStandard, settings->serverAxesStandard, nodePoseDynamic.pose.orientation);
			avs::ConvertPosition(clientNetworkContext->axesStandard, settings->serverAxesStandard, nodePoseDynamic.pose.position);
			avs::ConvertPosition(clientNetworkContext->axesStandard, settings->serverAxesStandard, nodePoseDynamic.velocity);
			avs::ConvertPosition(clientNetworkContext->axesStandard, settings->serverAxesStandard, nodePoseDynamic.angularVelocity);
			setControllerPose(clientID, int(nodePose.uid), &nodePoseDynamic);
		}
	}
		break;
	case teleport::core::ClientMessagePayloadType::NodeStatus:
	{
		size_t messageSize = sizeof(teleport::core::NodeStatusMessage);
		teleport::core::NodeStatusMessage message;
		memcpy(&message, packet->data, messageSize);
		size_t drawnSize = sizeof(avs::uid) * message.nodesDrawnCount;
		if(messageSize+drawnSize>packet->dataLength)
		{
			TELEPORT_CERR<<"Bad packet.\n";
			return;
		}
		std::vector<avs::uid> drawn(message.nodesDrawnCount);
		memcpy(drawn.data(), packet->data + messageSize, drawnSize);

		size_t toReleaseSize = sizeof(avs::uid) * message.nodesWantToReleaseCount;
		if(messageSize+drawnSize+toReleaseSize>packet->dataLength)
		{
			TELEPORT_CERR<<"Bad packet.\n";
			return;
		}
		std::vector<avs::uid> toRelease(message.nodesWantToReleaseCount);
		memcpy(toRelease.data(), packet->data + messageSize + drawnSize, toReleaseSize);

		for (avs::uid nodeID : drawn)
		{
			geometryStreamingService.clientStartedRenderingNode(clientID, nodeID);
		}

		for (avs::uid nodeID : toRelease)
		{
			geometryStreamingService.clientStoppedRenderingNode(clientID, nodeID);
		}

	}
		break;
	case teleport::core::ClientMessagePayloadType::ReceivedResources:
	{
		size_t messageSize = sizeof(teleport::core::ReceivedResourcesMessage);
		teleport::core::ReceivedResourcesMessage message;
		memcpy(&message, packet->data, messageSize);

		size_t confirmedResourcesSize = sizeof(avs::uid) * message.receivedResourcesCount;
		std::vector<avs::uid> confirmedResources(message.receivedResourcesCount);
		if(messageSize+confirmedResourcesSize>packet->dataLength)
		{
			TELEPORT_CERR<<"Bad packet.\n";
			return;
		}
		memcpy(confirmedResources.data(), packet->data + messageSize, confirmedResourcesSize);

		for (avs::uid id : confirmedResources)
		{
			geometryStreamingService.confirmResource(id);
		}
	}
	break;
	default:
		TELEPORT_CERR<<"Unknown client message: "<<(int)clientMessagePayloadType<<"\n";
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


