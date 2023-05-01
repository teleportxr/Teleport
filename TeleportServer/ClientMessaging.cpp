#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "libavstream/common_input.h"
#include "TeleportCore/CommonNetworking.h"

#include "SignalingService.h"
#include "TeleportCore/ErrorHandling.h"
#include "ClientManager.h"
#include "StringFunctions.h"

using namespace teleport;
using namespace server;

ClientMessaging::ClientMessaging(const struct ServerSettings* settings,
								 SignalingService &signalingService,
								 SetHeadPoseFn setHeadPose,
								 SetControllerPoseFn setControllerPose,
								 ProcessNewInputStateFn processNewInputState,
								ProcessNewInputEventsFn processNewInputEvents,
								 DisconnectFn onDisconnect,
								 uint32_t disconnectTimeout,
								 ReportHandshakeFn reportHandshakeFn,
								 ClientManager* clientManager)
	: settings(settings)
	, signalingService(signalingService)
	, geometryStreamingService(settings)
	, clientManager(clientManager)
	, setHeadPose(setHeadPose)
	, setControllerPose(setControllerPose)
	, processNewInputState(processNewInputState)
	, processNewInputEvents(processNewInputEvents)
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

void ClientMessaging::initialize( CaptureDelegates captureDelegates)
{
	captureComponentDelegates = captureDelegates;
	initialized = true;
}

bool ClientMessaging::startSession(avs::uid clientID, std::string clientIP)
{
	this->clientID = clientID;
	this->clientIP = clientIP;

	startingSession = true;

	ensureStreamingPipeline();
	return true;
}

void ClientMessaging::stopSession()
{
	receivedHandshake = false;
	geometryStreamingService.reset();

	stopped = true;
}
bool ClientMessaging::isStopped() const
{
	return stopped;
}

void ClientMessaging::sendStreamingControlMessage(const std::string& msg)
{
	signalingService.sendToClient(clientID, msg);
	// messages to be sent as text e.g. WebRTC config.
}

void ClientMessaging::tick(float deltaTime)
{
	std::string msg;
	if ( clientNetworkContext.NetworkPipeline.getNextStreamingControlMessage(msg))
	{
		sendStreamingControlMessage(msg);
	}
	//Don't stream to the client before we've received the handshake.
	if (!receivedHandshake)
		return;
	commandPipeline.process();
	messagePipeline.process();
	if (!clientNetworkContext.NetworkPipeline.isProcessingEnabled())
	{
		TELEPORT_COUT << "Network error occurred with client " << getClientIP() <<", disconnecting." << "\n";
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
			

				//Resize packet, and insert node lists.
				size_t totalSize = commandSize + enteredBoundsSize + leftBoundsSize;
				std::vector<uint8_t> packet(totalSize);
				memcpy(packet.data() , &boundsCommand, commandSize);
				memcpy(packet.data() + commandSize, nodesEnteredBounds.data(), enteredBoundsSize);
				memcpy(packet.data() + commandSize + enteredBoundsSize, nodesLeftBounds.data(), leftBoundsSize);

				SendCommand(packet.data(),totalSize);
				nodesEnteredBounds.clear();
				nodesLeftBounds.clear();
			}

		timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
	}

	if (settings->isReceivingAudio )
	{
		clientNetworkContext.sourceNetworkPipeline.process();
	}
}

// This handles the event queue that the ClientManager's processNetworkDataAsync thread has accumulated.
//But we need to lock to prevent these two threads conflicting.
void ClientMessaging::handleEvents(float deltaTime)
{
	timeSinceLastClientComm += deltaTime;

	if (startingSession)
	{
		timeStartingSession += deltaTime;
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
		uint16_t numBinaryEvents		=(uint16_t)latestInputStateAndEvents.binaryEvents.size();
		uint16_t numAnalogueEvents	=(uint16_t)latestInputStateAndEvents.analogueEvents.size();
		uint16_t numMotionEvents		=(uint16_t)latestInputStateAndEvents.motionEvents.size();
		processNewInputState(clientID, &inputState, &binaryStatesPtr, &analogueStatesPtr);
		processNewInputEvents(clientID, numBinaryEvents, numAnalogueEvents, numMotionEvents
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
	stopped = true;
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
	sendCommand2<>(command, updateList);
}

void ClientMessaging::updateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState>& updateList)
{
	teleport::core::UpdateNodeEnabledStateCommand command(updateList.size());
	sendCommand2<>(command, updateList);
}

void ClientMessaging::setNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
	teleport::core::SetNodeHighlightedCommand command(nodeID, isHighlighted);
	sendCommand2(command);
}

void ClientMessaging::reparentNode(avs::uid nodeID, avs::uid newParentID,avs::Pose relPose)
{
	avs::ConvertRotation(settings->serverAxesStandard, clientNetworkContext.axesStandard, relPose.orientation);
	avs::ConvertPosition(settings->serverAxesStandard, clientNetworkContext.axesStandard, relPose.position);
	teleport::core::UpdateNodeStructureCommand command(nodeID, newParentID, relPose);
	sendCommand2(command);
}

void ClientMessaging::setNodePosePath(avs::uid nodeID,const std::string &regexPosePath)
{
	teleport::core::AssignNodePosePathCommand command(nodeID, (uint16_t)regexPosePath.size());
	std::vector<char> chars;
	chars.resize(regexPosePath.size());
	memcpy(chars.data(),regexPosePath.data(),chars.size());
	TELEPORT_INTERNAL_COUT("Sent pose for node {0}: {1}", nodeID, regexPosePath);
	sendCommand2(command,chars);
}

void ClientMessaging::updateNodeAnimation(teleport::core::ApplyAnimation update)
{
	teleport::core::UpdateNodeAnimationCommand command(update);
	sendCommand2(command);
}

void ClientMessaging::updateNodeAnimationControl(teleport::core::NodeUpdateAnimationControl update)
{
	teleport::core::SetAnimationControlCommand command(update);
	sendCommand2(command);
}


void ClientMessaging::updateNodeRenderState(avs::uid nodeID,avs::NodeRenderState update)
{
	TELEPORT_ASSERT(false);// not implemented
}

void ClientMessaging::setNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed)
{
	teleport::core::SetNodeAnimationSpeedCommand command(nodeID, animationID, speed);
	sendCommand2(command);
}

bool ClientMessaging::hasPeer() const
{
	return true;
}

bool ClientMessaging::SendCommand(const void* c, size_t sz) const
{
	if (sz > 16384)
		return false;
	auto b=std::make_shared<std::vector<uint8_t>>(sz);
	memcpy(b->data(), c, sz);
	commandStack.PushBuffer(b);
	return true;
}

bool ClientMessaging::SendSignalingCommand(std::vector<uint8_t>&& bin)
{
	return signalingService.sendBinaryToClient(clientID,bin);
}

bool ClientMessaging::hasReceivedHandshake() const
{
	return clientNetworkContext.axesStandard != avs::AxesStandard::NotInitialized;
}

std::string ClientMessaging::getPeerIP() const
{
	return clientIP;
}

avs::Result ClientMessaging::decode(const void* buffer, size_t bufferSizeInBytes)
{
	std::vector<uint8_t> packet(bufferSizeInBytes);
	memcpy(packet.data(), buffer, bufferSizeInBytes);
	receiveClientMessage(packet);
	return avs::Result::OK;
}

void ClientMessaging::receiveSignaling(const std::vector<uint8_t>& bin)
{
	receiveHandshake(bin);
}

void ClientMessaging::receiveStreamingControl(const std::vector<uint8_t> &packet)
{
	uint16_t strlen = 0;
	memcpy(&strlen, packet.data(), sizeof(strlen));
	if (packet.size() != strlen + sizeof(strlen))
	{
		TELEPORT_CERR << "Bad packet.\n";
		return;
	}
	std::string str;
	str.resize(strlen);
	memcpy(str.data(), packet.data() + sizeof(strlen), strlen);
	clientNetworkContext.NetworkPipeline.receiveStreamingControlMessage(str);
}

void ClientMessaging::ensureStreamingPipeline()
{
	if (!clientNetworkContext.NetworkPipeline.isInitialized())
	{
		{
			ServerNetworkSettings networkSettings =
			{
				static_cast<int32_t>(handshake.maxBandwidthKpS),
				static_cast<int32_t>(handshake.udpBufferSize),
				settings->requiredLatencyMs,
				static_cast<int32_t>(disconnectTimeout)
				,avs::StreamingTransportLayer::WEBRTC
			};

			clientNetworkContext.NetworkPipeline.initialise(networkSettings);

			MessageDecoder.configure(this);
			messagePipeline.link({ &clientNetworkContext.NetworkPipeline.MessageQueue,&MessageDecoder });
		}
		TELEPORT_COUT << "Received handshake from clientID" << clientID << " at IP " << clientIP.c_str() << " .\n";

		if (settings->isReceivingAudio)
		{
			avs::NetworkSourceParams sourceParams;
			sourceParams.connectionTimeout = disconnectTimeout;
			sourceParams.remoteIP = clientIP.c_str();
			sourceParams.remotePort = handshake.clientStreamingPort;

			clientNetworkContext.sourceNetworkPipeline.initialize(settings, sourceParams
				, &clientNetworkContext.sourceAudioQueue
				, &clientNetworkContext.audioDecoder
				, &clientNetworkContext.audioTarget);
		}
	}
}

void ClientMessaging::receiveHandshake(const std::vector<uint8_t> &packet)
{
	if (receivedHandshake)
		return;
	size_t handShakeSize = sizeof(teleport::core::Handshake);

	memcpy(&handshake, packet.data(), handShakeSize);

	clientNetworkContext.axesStandard = handshake.axesStandard;

	CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
	cameraInfo.width = static_cast<float>(handshake.startDisplayInfo.width);
	cameraInfo.height = static_cast<float>(handshake.startDisplayInfo.height);
	cameraInfo.fov = handshake.FOV;
	cameraInfo.isVR = handshake.isVR;

	//Extract list of resources the client has.
	std::vector<avs::uid> clientResources(handshake.resourceCount);
	memcpy(clientResources.data(), packet.data() + handShakeSize, sizeof(avs::uid)* handshake.resourceCount);

	//Confirm resources the client has told us they have.
	for (int i = 0; i < handshake.resourceCount; i++)
	{
		geometryStreamingService.confirmResource(clientResources[i]);
	}
	captureComponentDelegates.startStreaming(&clientNetworkContext);
	geometryStreamingService.startStreaming(&clientNetworkContext, handshake);
	{
		commandEncoder.configure(&commandStack);

		commandPipeline.link({ &commandEncoder, &clientNetworkContext.NetworkPipeline.CommandQueue });

	}

	//Client has nothing, thus can't show nodes.
	if (handshake.resourceCount == 0)
	{
		teleport::core::AcknowledgeHandshakeCommand ack;
		TELEPORT_LOG("Sending handshake acknowledgement to clientID {0} at IP {1}  .\n", clientID, clientIP);

		sendSignalingCommand(ack);
	}
	// Client may have required resources, as they are reconnecting; tell them to show streamed nodes.
	else
	{
		TELEPORT_LOG("Sending handshake acknowledgement to clientID {0} at IP {1} with {2} nodes .\n", clientID, clientIP, handshake.resourceCount);
		const std::set<avs::uid>& streamedNodeIDs = geometryStreamingService.getStreamedNodeIDs();
		teleport::core::AcknowledgeHandshakeCommand ack(streamedNodeIDs.size());
		sendSignalingCommand<>(ack, std::vector<avs::uid>{streamedNodeIDs.begin(), streamedNodeIDs.end()});
	}
	receivedHandshake = true;
	reportHandshake(this->clientID, &handshake);
	TELEPORT_LOG("Started streaming to clientID {0} at IP {1}.\n", clientID, clientIP);
}

bool ClientMessaging::setOrigin(uint64_t valid_counter, avs::uid originNode)
{
	geometryStreamingService.setOriginNode(originNode);
	teleport::core::SetStageSpaceOriginNodeCommand setp;
	
	setp.origin_node=originNode;
	setp.valid_counter = valid_counter;
	TELEPORT_LOG("Send origin node {0} with counter {1} to clientID {2}.\n", originNode, valid_counter, clientID);
	bool result=sendCommand2(setp);
	return result;
}

void ClientMessaging::receiveInputStates(const std::vector<uint8_t> &packet)
{
	size_t InputsMessageSize = sizeof(teleport::core::InputStatesMessage);

	if (packet.size() < InputsMessageSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet.size() << "; less than minimum size of " << InputsMessageSize << "!\n";
		return;
	}

	teleport::core::InputStatesMessage msg;
	//Copy newest input state into member variable.
	memcpy(&msg, packet.data(), InputsMessageSize);
	
	size_t binaryStateSize		= msg.inputState.numBinaryStates;
	size_t analogueStateSize	= sizeof(float)* msg.inputState.numAnalogueStates;
	size_t totalSize = InputsMessageSize + binaryStateSize + analogueStateSize;

	if (packet.size() != totalSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet.size() << "; expected size of " << totalSize << "!\n" <<
			"     InputsMessage Size: " << InputsMessageSize << "\n" <<
			"  Binary States Size:" << binaryStateSize << "(" << msg.inputState.numBinaryStates << ")\n" <<
			"Analogue States Size:" << analogueStateSize << "(" << msg.inputState.numAnalogueStates << ")\n" ;

		return;
	}
	latestInputStateAndEvents.analogueStates.resize(msg.inputState.numAnalogueStates);
	latestInputStateAndEvents.binaryStates.resize(binaryStateSize);
	const uint8_t *src=packet.data()+ InputsMessageSize;
	if(msg.inputState.numBinaryStates != 0)
	{
		memcpy(latestInputStateAndEvents.binaryStates.data(), src, binaryStateSize);
		src+=binaryStateSize;
	}
	if(msg.inputState.numAnalogueStates != 0)
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
	if (src - packet.data() != totalSize)
	{
		TELEPORT_CERR << "Bad input size\n";
	}
}

void ClientMessaging::receiveInputEvents(const std::vector<uint8_t> &packet)
{
	size_t InputsMessageSize = sizeof(teleport::core::InputEventsMessage);

	if (packet.size() < InputsMessageSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet.size() << "; less than minimum size of " << InputsMessageSize << "!\n";
		return;
	}

	teleport::core::InputEventsMessage msg;
	//Copy newest input state into member variable.
	memcpy(&msg, packet.data(), InputsMessageSize);

	size_t binaryEventSize = sizeof(avs::InputEventBinary) * msg.numBinaryEvents;
	size_t analogueEventSize = sizeof(avs::InputEventAnalogue) * msg.numAnalogueEvents;
	size_t motionEventSize = sizeof(avs::InputEventMotion) * msg.numMotionEvents;
	size_t totalSize = InputsMessageSize +  binaryEventSize + analogueEventSize + motionEventSize;

	if (packet.size() != totalSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_" << clientID << "! Received malformed InputState packet of length " << packet.size() << "; expected size of " << totalSize << "!\n" <<
			"     InputsMessage Size: " << InputsMessageSize << "\n" <<
			"  Binary Events Size:" << binaryEventSize << "(" << msg.numBinaryEvents << ")\n" <<
			"Analogue Events Size:" << analogueEventSize << "(" << msg.numAnalogueEvents << ")\n" <<
			"  Motion Events Size:" << motionEventSize << "(" << msg.numMotionEvents << ")\n";

		return;
	}
	const uint8_t* src = packet.data() + InputsMessageSize;
	if (msg.numBinaryEvents != 0)
	{
		const avs::InputEventBinary* binaryData = reinterpret_cast<const avs::InputEventBinary*>(src);
		latestInputStateAndEvents.binaryEvents.insert(latestInputStateAndEvents.binaryEvents.end(), binaryData, binaryData + msg.numBinaryEvents);
		src += binaryEventSize;
	}
	else if (latestInputStateAndEvents.binaryEvents.size())
	{
		//	TELEPORT_CERR << "... " <<  std::endl;
	}
	if (msg.numAnalogueEvents != 0)
	{
		const avs::InputEventAnalogue* analogueData = reinterpret_cast<const avs::InputEventAnalogue*>(src);
		latestInputStateAndEvents.analogueEvents.insert(latestInputStateAndEvents.analogueEvents.end(), analogueData, analogueData + msg.numAnalogueEvents);
		for (auto c : latestInputStateAndEvents.analogueEvents)
		{
			TELEPORT_COUT << "Analogue: " << c.eventID << " " << (int)c.inputID << " " << c.strength << "\n";
		}
		src += analogueEventSize;
	}

	if (msg.numMotionEvents != 0)
	{
		const avs::InputEventMotion* motionData = reinterpret_cast<const avs::InputEventMotion*>(src);
		latestInputStateAndEvents.motionEvents.insert(latestInputStateAndEvents.motionEvents.end(), motionData, motionData + msg.numMotionEvents);
	}
	if (src - packet.data() != totalSize)
	{
		TELEPORT_CERR << "Bad input size\n";
	}
}

void ClientMessaging::receiveDisplayInfo(const std::vector<uint8_t> &packet)
{
	if (packet.size() != sizeof(core::DisplayInfoMessage))
	{
		TELEPORT_COUT << "Session: Received malformed display info packet of length: " << packet.size() << "\n";
		return;
	}

	core::DisplayInfoMessage displayInfoMessage;
	memcpy(&displayInfoMessage, packet.data(), sizeof(core::DisplayInfoMessage));

	CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
	cameraInfo.width = static_cast<float>(displayInfoMessage.displayInfo.width);
	cameraInfo.height = static_cast<float>(displayInfoMessage.displayInfo.height);
}

void ClientMessaging::receiveResourceRequest(const std::vector<uint8_t> &packet)
{
	core::ResourceRequestMessage msg;
	if (packet.size() <sizeof(core::ResourceRequestMessage))
	{
		TELEPORT_COUT << "Session: Received malformed ResourceRequest packet of length: " << packet.size() << "\n";
		return;
	}
	memcpy(&msg, packet.data(), sizeof(msg));
	if (packet.size() < sizeof(core::ResourceRequestMessage)+msg.resourceCount*sizeof(avs::uid))
	{
		TELEPORT_COUT << "Session: Received malformed ResourceRequest packet of length: " << packet.size() << "\n";
		return;
	}
	std::vector<avs::uid> resourceRequests(msg.resourceCount);
	memcpy(resourceRequests.data(), packet.data() + sizeof(msg), sizeof(avs::uid) * msg.resourceCount);

	for (avs::uid id : resourceRequests)
	{
		geometryStreamingService.requestResource(id);
	}
}

void ClientMessaging::receiveKeyframeRequest(const std::vector<uint8_t> &packet)
{
	if (packet.size() < sizeof(core::KeyframeRequestMessage))
	{
		TELEPORT_COUT << "Session: Received malformed KeyframeRequestMessage packet of length: " << packet.size() << "\n";
		return;
	}
	if (captureComponentDelegates.requestKeyframe)
	{
		captureComponentDelegates.requestKeyframe();
	}
	else
	{
		TELEPORT_COUT << "Received keyframe request, but capture component isn't set.\n";
	}
}

void ClientMessaging::receiveClientMessage(const std::vector<uint8_t> &packet)
{
	teleport::core::ClientMessagePayloadType clientMessagePayloadType = *(reinterpret_cast<const teleport::core::ClientMessagePayloadType*>(packet.data()));
	switch (clientMessagePayloadType)
	{
		case teleport::core::ClientMessagePayloadType::ControllerPoses:
		{
			teleport::core::ControllerPosesMessage message;
			if(packet.size()<sizeof(message))
			{
				TELEPORT_CERR << "Bad packet size.\n";
				return;
			}
			memcpy(&message, packet.data(), sizeof(message));
			//std::cout << "timestamp_unix_ms: "<<(message.timestamp_unix_ms/1000.0) << std::endl;
			if(packet.size()!=sizeof(message)+sizeof(teleport::core::NodePose)*message.numPoses)
			{
				TELEPORT_CERR << "Bad packet size.\n";
				return;
			}
			avs::ConvertRotation(clientNetworkContext.axesStandard, settings->serverAxesStandard, message.headPose.orientation);
			avs::ConvertPosition(clientNetworkContext.axesStandard, settings->serverAxesStandard, message.headPose.position);
			setHeadPose(clientID, (avs::Pose*)&message.headPose);
			const uint8_t *src=packet.data()+sizeof(message);
			for (int i = 0; i < message.numPoses; i++)
			{
				teleport::core::NodePose nodePose;
				memcpy(&nodePose,src,sizeof(nodePose));
				src+=sizeof(nodePose);
				avs::PoseDynamic nodePoseDynamic=nodePose.poseDynamic;
				avs::ConvertRotation(clientNetworkContext.axesStandard, settings->serverAxesStandard, nodePoseDynamic.pose.orientation);
				avs::ConvertPosition(clientNetworkContext.axesStandard, settings->serverAxesStandard, nodePoseDynamic.pose.position);
				avs::ConvertPosition(clientNetworkContext.axesStandard, settings->serverAxesStandard, nodePoseDynamic.velocity);
				avs::ConvertPosition(clientNetworkContext.axesStandard, settings->serverAxesStandard, nodePoseDynamic.angularVelocity);
				setControllerPose(clientID, int(nodePose.uid), &nodePoseDynamic);
			}
		}
			break;
		case teleport::core::ClientMessagePayloadType::NodeStatus:
		{
			size_t messageSize = sizeof(teleport::core::NodeStatusMessage);
			teleport::core::NodeStatusMessage message;
			memcpy(&message, packet.data(), messageSize);
			size_t drawnSize = sizeof(avs::uid) * message.nodesDrawnCount;
			if(messageSize+drawnSize>packet.size())
			{
				TELEPORT_CERR<<"Bad packet.\n";
				return;
			}
			std::vector<avs::uid> drawn(message.nodesDrawnCount);
			memcpy(drawn.data(), packet.data() + messageSize, drawnSize);

			size_t toReleaseSize = sizeof(avs::uid) * message.nodesWantToReleaseCount;
			if(messageSize+drawnSize+toReleaseSize>packet.size())
			{
				TELEPORT_CERR<<"Bad packet.\n";
				return;
			}
			std::vector<avs::uid> toRelease(message.nodesWantToReleaseCount);
			memcpy(toRelease.data(), packet.data() + messageSize + drawnSize, toReleaseSize);

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
			memcpy(&message, packet.data(), messageSize);

			size_t confirmedResourcesSize = sizeof(avs::uid) * message.receivedResourcesCount;

			if(messageSize+confirmedResourcesSize>packet.size())
			{
				TELEPORT_CERR<<"Bad packet.\n";
				return;
			}
			std::vector<avs::uid> confirmedResources(message.receivedResourcesCount);
			memcpy(confirmedResources.data(), packet.data() + messageSize, confirmedResourcesSize);

			for (avs::uid id : confirmedResources)
			{
				geometryStreamingService.confirmResource(id);
			}
		}
		break;
		case teleport::core::ClientMessagePayloadType::ResourceRequest:
			receiveResourceRequest(packet);
			break;
		case teleport::core::ClientMessagePayloadType::InputStates:
			receiveInputStates(packet);
			break;
		case teleport::core::ClientMessagePayloadType::InputEvents:
			receiveInputEvents(packet);
			break;
		case teleport::core::ClientMessagePayloadType::DisplayInfo:
			receiveDisplayInfo(packet);
			break;
		case teleport::core::ClientMessagePayloadType::KeyframeRequest:
			receiveKeyframeRequest(packet);
			break;
		default:
			TELEPORT_CERR<<"Unknown client message: "<<(int)clientMessagePayloadType<<"\n";
		break;
	};
}
avs::ConnectionState ClientMessaging::getConnectionState() const
{
	if (clientNetworkContext.NetworkPipeline.mNetworkSink)
		return clientNetworkContext.NetworkPipeline.mNetworkSink->getConnectionState();
	return avs::ConnectionState::UNINITIALIZED;
};