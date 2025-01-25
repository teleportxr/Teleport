#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "TeleportCore/InputTypes.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/Profiling.h" 

#include "SignalingService.h"
#include "TeleportCore/ErrorHandling.h"
#include "ClientManager.h"
#include "TeleportCore/StringFunctions.h"
#include "TeleportCore/Logging.h"
#include "GeometryStore.h"

using namespace teleport;
using namespace server;
#pragma optimize("",off)

ClientMessaging::ClientMessaging(SignalingService &signalingService,
								 SetHeadPoseFn setHeadPose,
								 SetControllerPoseFn setControllerPose,
								 ProcessNewInputStateFn processNewInputState,
								ProcessNewInputEventsFn processNewInputEvents,
								 DisconnectFn onDisconnect,
								 uint32_t disconnectTimeout,
								 ReportHandshakeFn reportHandshakeFn, avs::uid clid)
	:  signalingService(signalingService)
	, geometryStreamingService(*this,clid)
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
	// TODO: send shutdown command
	//teleport::core::ShutdownCommand shutdownCommand;
	//clientData->clientMessaging->sendCommand(shutdownCommand);
	receivedHandshake = false;
	geometryStreamingService.reset();
	clientNetworkContext.NetworkPipeline.release();
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


void ClientMessaging::forceUpdateNodeMovement(const std::vector<avs::uid>& updateList)
{
	if(!updateList.size())
		return;
	for(auto u:updateList)
		nodes_to_force_update_movement.insert(u);
}
	
void ClientMessaging::sendNodeMovementUpdates()
{
	auto axesStandard = getClientNetworkContext()->axesStandard;
	if(axesStandard==avs::AxesStandard::NotInitialized)
		return;
	std::set<avs::uid> nodes_to_update_movement;
	geometryStreamingService.getNodesToUpdateMovement(nodes_to_update_movement,GetServerTimeUs());

	if(nodes_to_force_update_movement.size())
	{
		for(auto u:nodes_to_force_update_movement)
		{
			nodes_to_update_movement.insert(u);
		}
		nodes_to_force_update_movement.clear();
	}
	GeometryStore &geometryStore=GeometryStore::GetInstance();

	size_t numUpdates=nodes_to_update_movement.size();
	std::vector<teleport::core::MovementUpdate> updateList(numUpdates);
	
	int64_t server_time_us = GetServerTimeUs();
	size_t i=0;
	for (avs::uid nodeID : nodes_to_update_movement)
	{
		avs::Node *avsNode=geometryStore.getNode(nodeID);
		auto &u=updateList[i];
		u.isGlobal=false;
		u.nodeID=nodeID;
		u.server_time_us=server_time_us;
		u.position=avsNode->localTransform.position;
		u.rotation=avsNode->localTransform.rotation;
		u.scale=avsNode->localTransform.scale;
		u.velocity={0,0,0};
		u.angularVelocityAxis={0,0,0};
		u.angularVelocityAngle=0.0f;
		avs::ConvertPosition(serverSettings.serverAxesStandard, axesStandard, u.position);
		avs::ConvertRotation(serverSettings.serverAxesStandard, axesStandard, u.rotation);
		avs::ConvertScale	(serverSettings.serverAxesStandard, axesStandard, u.scale);
		avs::ConvertPosition(serverSettings.serverAxesStandard, axesStandard, u.velocity);
		avs::ConvertPosition(serverSettings.serverAxesStandard, axesStandard, u.angularVelocityAxis);
		i++;

	}
	if(updateList.size()==0)
		return;
	//updateList.resize(1);
	updateNodeMovement(updateList);
}

void ClientMessaging::tick(float deltaTime)
{
	TELEPORT_PROFILE_AUTOZONE;
	std::string msg;
	if ( clientNetworkContext.NetworkPipeline.getNextStreamingControlMessage(msg))
	{
		sendStreamingControlMessage(msg);
	} 
	//Don't stream to the client before we've received the handshake.
	if (!receivedHandshake)
		return;
	int64_t server_time_us = GetServerTimeUs();
	if(!currentOriginState.acknowledged)
	{
		if(server_time_us-currentOriginState.serverTimeSentUs>5000000)
			setOrigin(currentOriginState.originClientHas);
	}
	avs::Result commandResult=commandPipeline.process();
	if(commandResult==avs::Result::IO_Full)
	{
		if (!commandPipeline.IsPipelineBlocked())
		{
			commandPipeline.SetPipelineBlocked(true);
			TELEPORT_CERR << "Client "<<clientID<<": Command pipeline is full. No further commands accepted until it clears.\n";
		}
	}
	else
	{
		commandPipeline.SetPipelineBlocked(false);
		sendNodeMovementUpdates();
	}
	avs::Result messageResult = messagePipeline.process();
	
	if (commandResult == avs::Result::IO_Full)
	{
		if(!messagePipeline.IsPipelineBlocked())
		{
			messagePipeline.SetPipelineBlocked(true);
			TELEPORT_CERR << "Client " << clientID << ": Message pipeline is full. No further messages accepted until it clears.\n";
		}
	}
	else
	{
		messagePipeline.SetPipelineBlocked(false);
	}
	if (!clientNetworkContext.NetworkPipeline.isProcessingEnabled())
	{
		TELEPORT_COUT << "Network error occurred with client " << getClientIP() <<", disconnecting." << "\n";
		Disconnect();
		return;
	}
	timeSinceLastGeometryStream += deltaTime;

	float TIME_BETWEEN_GEOMETRY_TICKS = 1.0f / serverSettings.geometryTicksPerSecond;

	//Only tick the geometry streaming service a set amount of times per second.
	if (timeSinceLastGeometryStream >= TIME_BETWEEN_GEOMETRY_TICKS)
	{
	//	 only do geom streaming if the streaming connection is open.
		if(clientNetworkContext.NetworkPipeline.IsStreamingActive())
			geometryStreamingService.tick(TIME_BETWEEN_GEOMETRY_TICKS);

		//Tell the client to change the visibility of nodes that have changed whether they are within streamable bounds.
	
		timeSinceLastGeometryStream -= TIME_BETWEEN_GEOMETRY_TICKS;
	}

	if (serverSettings.isReceivingAudio)
	{
		clientNetworkContext.sourceNetworkPipeline.process();
	}
	framesSinceLastPing++;
	if (framesSinceLastPing > 100)
	{
		framesSinceLastPing = 0;
		pingForLatency();
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
	//Send latest input to managed code for this networking tick; we need the variables as we can't take the memory address of an rvalue.
	const uint8_t* binaryStatesPtr = latestInputStateAndEvents.binaryStates.data();
	const float* analogueStatesPtr = latestInputStateAndEvents.analogueStates.data();
	const teleport::core::InputEventBinary* binaryEventsPtr		= latestInputStateAndEvents.binaryEvents.data();
	const teleport::core::InputEventAnalogue* analogueEventsPtr	= latestInputStateAndEvents.analogueEvents.data();
	const teleport::core::InputEventMotion* motionEventsPtr		= latestInputStateAndEvents.motionEvents.data();
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
	uint16_t numAnalogueEvents		=(uint16_t)latestInputStateAndEvents.analogueEvents.size();
	uint16_t numMotionEvents		=(uint16_t)latestInputStateAndEvents.motionEvents.size();
	processNewInputState(clientID, &inputState, &binaryStatesPtr, &analogueStatesPtr);
	processNewInputEvents(clientID, numBinaryEvents, numAnalogueEvents, numMotionEvents
							,&binaryEventsPtr, &analogueEventsPtr, &motionEventsPtr);
	


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
	ClientManager::instance().stopClient(clientID);
	stopped = true;
}

void ClientMessaging::sendSetupCommand(const teleport::core::SetupCommand &setupCommand
	, const teleport::core::SetupLightingCommand setupLightingCommand,const std::vector<avs::uid> &global_illumination_texture_uids
, const teleport::core::SetupInputsCommand &setupInputsCommand,const std::vector<teleport::core::InputDefinition>& inputDefinitions)
{
	ConfirmSessionStarted();
	sendSignalingCommand(setupCommand);
	sendSignalingCommand(setupLightingCommand, global_illumination_texture_uids);
	sendSignalingCommand(setupInputsCommand, inputDefinitions);
	lastSetupCommand = setupCommand;
}

void ClientMessaging::sendReconfigureVideoCommand(const core::ReconfigureVideoCommand& cmd)
{
	sendCommand(cmd);
}

void ClientMessaging::sendSetupLightingCommand(const teleport::core::SetupLightingCommand setupLightingCommand, const std::vector<avs::uid>& global_illumination_texture_uids)
{
	sendCommand(setupLightingCommand, global_illumination_texture_uids);
}

void ClientMessaging::updateNodeMovement(const std::vector<teleport::core::MovementUpdate>& updateList)
{
	if(!updateList.size())
		return;
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

void ClientMessaging::reparentNode(const teleport::core::UpdateNodeStructureCommand &cmd)
{
	sendCommand(cmd);
}

void ClientMessaging::setNodePosePath(avs::uid nodeID,const std::string &regexPosePath)
{
	teleport::core::AssignNodePosePathCommand command(nodeID, (uint16_t)regexPosePath.size());
	std::vector<char> chars;
	chars.resize(regexPosePath.size());
	memcpy(chars.data(),regexPosePath.data(),chars.size());
	TELEPORT_INTERNAL_COUT("Sent pose for node {0}: {1}", nodeID, regexPosePath);
	sendCommand(command,chars);
}

void ClientMessaging::updateNodeAnimation(teleport::core::ApplyAnimation update)
{
	teleport::core::ApplyAnimationCommand command(update);
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

void ClientMessaging::pingForLatency()
{
	teleport::core::PingForLatencyCommand pingForLatencyCommand;
	pingForLatencyCommand.unix_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	if (sendCommand(pingForLatencyCommand) > 1)
	{
		TELEPORT_WARN_NOSPAM("Pinging on queue.\n");
	}
}

size_t ClientMessaging::SendCommand(const void* c, size_t sz) const
{
	if (sz > 16384)
	{
		TELEPORT_CERR << "Command too large, size is "<<sz<<".\n";
		return 0;
	}
	if(commandPipeline.IsPipelineBlocked())
		return 0;
	auto b=std::make_shared<std::vector<uint8_t>>(sz);
	memcpy(b->data(), c, sz);
	commandStack.PushBuffer(b);
	return commandStack.buffers.size();
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
					serverSettings.requiredLatencyMs,
				static_cast<int32_t>(disconnectTimeout)
			};

			clientNetworkContext.NetworkPipeline.initialise(networkSettings);

			MessageDecoder.configure(this,"MessageDecoder");
			messagePipeline.link({&clientNetworkContext.NetworkPipeline.unreliableReceiveQueue, &MessageDecoder});
		}
		TELEPORT_COUT << "Received handshake from clientID" << clientID << " at IP " << clientIP.c_str() << " .\n";

		if (serverSettings.isReceivingAudio)
		{
			avs::NetworkSourceParams sourceParams;
			sourceParams.connectionTimeout = disconnectTimeout;
			sourceParams.remoteIP = clientIP.c_str();

			clientNetworkContext.sourceNetworkPipeline.initialize( sourceParams
				, &clientNetworkContext.sourceAudioQueue
				, &clientNetworkContext.audioDecoder
				, &clientNetworkContext.audioTarget);
		}
	}
}

void ClientMessaging::receiveHandshake(const std::vector<uint8_t> &packet)
{
	size_t handShakeSize = sizeof(teleport::core::Handshake);
	if (packet.size() < handShakeSize)
	{
		TELEPORT_INTERNAL_CERR("Bad handshake size, IP {}.", clientIP);
		return;
	}
	memcpy(&handshake, packet.data(), handShakeSize);

	clientNetworkContext.axesStandard = handshake.axesStandard;

	CameraInfo& cameraInfo = captureComponentDelegates.getClientCameraInfo();
	cameraInfo.width = static_cast<float>(handshake.startDisplayInfo.width);
	cameraInfo.height = static_cast<float>(handshake.startDisplayInfo.height);
	cameraInfo.fov = handshake.FOV;
	cameraInfo.isVR = handshake.isVR;

	if (packet.size() !=handShakeSize+sizeof(avs::uid)* handshake.resourceCount)
	{
		TELEPORT_INTERNAL_CERR("Bad handshake size, IP {}.", clientIP);
		return;
	}
	//Extract list of resources the client has.
	std::vector<avs::uid> clientResources(handshake.resourceCount);
	memcpy(clientResources.data(), packet.data() + handShakeSize, sizeof(avs::uid)* handshake.resourceCount);

	//Confirm resources the client has told us they have.
	for (int i = 0; i < handshake.resourceCount; i++)
	{
		geometryStreamingService.confirmResource(clientResources[i]);
	}
	if (!receivedHandshake)
	{
		captureComponentDelegates.startStreaming(&clientNetworkContext);
		geometryStreamingService.startStreaming(&clientNetworkContext, handshake);
		{
			commandEncoder.configure(&commandStack,"Command Encoder");
			commandPipeline.link({&commandEncoder, &clientNetworkContext.NetworkPipeline.reliableSendQueue});
		}
		receivedHandshake = true;
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
	reportHandshake(this->clientID, &handshake);
	TELEPORT_LOG("Started streaming to clientID {0} at IP {1}.\n", clientID, clientIP);
	setOrigin(currentOriginState.originClientHas);
}
avs::uid ClientMessaging::getOrigin() const
{
	return currentOriginState.originClientHas;
}

int64_t ClientMessaging::GetServerTimeUs() const 
{
	int64_t unix_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	int64_t serverTimeUs = unix_time_us - lastSetupCommand.startTimestamp_utc_unix_us;
	return serverTimeUs;
}

bool ClientMessaging::setOrigin( avs::uid originNode)
{
	if(originNode==0)
		return false;
	if(lastSetupCommand.startTimestamp_utc_unix_us==0)
		return false;
	if(currentOriginState.originClientHas==originNode&&currentOriginState.acknowledged)
		return true;
	// If we sent it but it wasn't acknowledged in a reasonable time?
	static int64_t originAckWaitTimeUs=3000000;// three seconds
	if(currentOriginState.originClientHas==originNode&&(GetServerTimeUs()-currentOriginState.serverTimeSentUs)<originAckWaitTimeUs)
	{
		return true;
	}
	currentOriginState.valid_counter++;
	geometryStreamingService.setOriginNode(originNode);
	teleport::core::SetStageSpaceOriginNodeCommand setp;
	setp.ack_id=next_ack_id++;
	setp.origin_node=originNode;
	setp.valid_counter = currentOriginState.valid_counter;
	
	// This is now the valid origin.
	currentOriginState.originClientHas=originNode;
	currentOriginState.sent=true;
	currentOriginState.ack_id=setp.ack_id;
	currentOriginState.acknowledged=false;
	currentOriginState.serverTimeSentUs=GetServerTimeUs();
	if (!hasReceivedHandshake())
	{
		static char t=1;
		t--;
		if(!t)
		{
			TELEPORT_INTERNAL_CERR("Client {0} - Can't set origin - no handshake yet.\n",clientID);
		}
		return false;
	}
	bool result=sendCommand(setp);
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

	size_t binaryEventSize = sizeof(teleport::core::InputEventBinary) * msg.numBinaryEvents;
	size_t analogueEventSize = sizeof(teleport::core::InputEventAnalogue) * msg.numAnalogueEvents;
	size_t motionEventSize = sizeof(teleport::core::InputEventMotion) * msg.numMotionEvents;
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
		const teleport::core::InputEventBinary* binaryData = reinterpret_cast<const teleport::core::InputEventBinary*>(src);
		latestInputStateAndEvents.binaryEvents.insert(latestInputStateAndEvents.binaryEvents.end(), binaryData, binaryData + msg.numBinaryEvents);
		src += binaryEventSize;
	}
	else if (latestInputStateAndEvents.binaryEvents.size())
	{
		//	TELEPORT_CERR << "... " <<  std::endl;
	}
	if (msg.numAnalogueEvents != 0)
	{
		const teleport::core::InputEventAnalogue* analogueData = reinterpret_cast<const teleport::core::InputEventAnalogue*>(src);
		latestInputStateAndEvents.analogueEvents.insert(latestInputStateAndEvents.analogueEvents.end(), analogueData, analogueData + msg.numAnalogueEvents);
		for (auto c : latestInputStateAndEvents.analogueEvents)
		{
			TELEPORT_COUT << "Analogue: " << c.eventID << " " << (int)c.inputID << " " << c.strength << "\n";
		}
		src += analogueEventSize;
	}

	if (msg.numMotionEvents != 0)
	{
		const teleport::core::InputEventMotion* motionData = reinterpret_cast<const teleport::core::InputEventMotion*>(src);
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
	displayInfo =displayInfoMessage.displayInfo;
}

const avs::DisplayInfo& ClientMessaging::getDisplayInfo() const
{
	return displayInfo;
}

void ClientMessaging::receiveAcknowledgement(const std::vector<uint8_t> &packet)
{
	core::AcknowledgementMessage msg;
	if (packet.size()!=sizeof(core::AcknowledgementMessage))
	{
		TELEPORT_COUT << "Session: Received malformed OriginRequest packet of length: " << packet.size() << "\n";
		return;
	}
	memcpy(&msg, packet.data(), sizeof(msg));
	if(msg.ack_id==currentOriginState.ack_id)
	{
		currentOriginState.acknowledged=true;
	}
}

void ClientMessaging::receiveKeyframeRequest(const std::vector<uint8_t>& packet)
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

void ClientMessaging::receivePongForLatency(const std::vector<uint8_t>& packet)
{
	if (packet.size() != sizeof(core::PongForLatencyMessage))
	{
		TELEPORT_COUT << "Session: Received malformed KeyframeRequestMessage packet of length: " << packet.size() << "\n";
		return;
	}
	int64_t unix_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	core::PongForLatencyMessage msg;
	memcpy(&msg, packet.data() , sizeof(core::PongForLatencyMessage));
	int64_t diff_ns = unix_time_ns - msg.unix_time_ns;

	clientNetworkState.client_to_server_latency_ms = float(double(diff_ns) * 0.000001);
	clientNetworkState.server_to_client_latency_ms= float(double(msg.server_to_client_latency_ns) * 0.000001);
}

void ClientMessaging::receiveClientMessage(const std::vector<uint8_t> &packet)
{
	teleport::core::ClientMessagePayloadType clientMessagePayloadType = *(reinterpret_cast<const teleport::core::ClientMessagePayloadType*>(packet.data()));
	switch (clientMessagePayloadType)
	{
		case teleport::core::ClientMessagePayloadType::OrthogonalAcknowledgement:
		{
			const auto *message=reinterpret_cast<const core::OrthogonalAcknowledgementMessage * >(packet.data());
			confirmationsReceived.insert(message->confirmationNumber);
		}
		case teleport::core::ClientMessagePayloadType::ControllerPoses:
		{
			teleport::core::ControllerPosesMessage message;
			if(packet.size()<sizeof(message))
			{
				TELEPORT_CERR << "Bad packet size.\n";
				return;
			}
			memcpy(&message, packet.data(), sizeof(message));
			
			if(packet.size()!=sizeof(message)+sizeof(teleport::core::NodePose)*message.numPoses)
			{
				TELEPORT_CERR << "Bad packet size.\n";
				return;
			}
			avs::ConvertRotation(clientNetworkContext.axesStandard, serverSettings.serverAxesStandard, message.headPose.orientation);
			avs::ConvertPosition(clientNetworkContext.axesStandard, serverSettings.serverAxesStandard, message.headPose.position);
			setHeadPose(clientID, (teleport::core::Pose*)&message.headPose);
			const uint8_t *src=packet.data()+sizeof(message);
			for (int i = 0; i < message.numPoses; i++)
			{
				teleport::core::NodePose nodePose;
				memcpy(&nodePose,src,sizeof(nodePose));
				src+=sizeof(nodePose);
				teleport::core::PoseDynamic nodePoseDynamic=nodePose.poseDynamic;
				avs::ConvertRotation(clientNetworkContext.axesStandard,serverSettings.serverAxesStandard, nodePoseDynamic.pose.orientation);
				avs::ConvertPosition(clientNetworkContext.axesStandard,serverSettings.serverAxesStandard, nodePoseDynamic.pose.position);
				avs::ConvertPosition(clientNetworkContext.axesStandard,serverSettings.serverAxesStandard, nodePoseDynamic.velocity);
				avs::ConvertPosition(clientNetworkContext.axesStandard,serverSettings.serverAxesStandard, nodePoseDynamic.angularVelocity);
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
				geometryStreamingService.startedRenderingNode( nodeID);
			}

			for (avs::uid nodeID : toRelease)
			{
				geometryStreamingService.stoppedRenderingNode( nodeID);
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
		case teleport::core::ClientMessagePayloadType::ResourceLost:
			//receiveResourceLost(packet);
			TELEPORT_WARN("Received ResourceLost, but this is not yet supported.");
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
		case teleport::core::ClientMessagePayloadType::PongForLatency:
			receivePongForLatency(packet);
			break;
		case teleport::core::ClientMessagePayloadType::Acknowledgement:
			receiveAcknowledgement(packet);
			break;
		default:
			TELEPORT_CERR<<"Unknown client message: "<<(int)clientMessagePayloadType<<"\n";
		break;
	};
}

avs::StreamingConnectionState ClientMessaging::getStreamingState() const
{
	if (clientNetworkContext.NetworkPipeline.mNetworkSink)
		return clientNetworkContext.NetworkPipeline.mNetworkSink->getConnectionState();
	else
		return avs::StreamingConnectionState::UNINITIALIZED;
}

void ClientMessaging::Warn(const char *w) const
{
	TELEPORT_WARN(w);
}