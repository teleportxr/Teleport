#include "ClientMessaging.h"

#include <algorithm>
#include <iostream>

#include "enet/enet.h"
#include "libavstream/common.hpp" //InputState

#include "DiscoveryService.h"
#include "ErrorHandling.h"

namespace SCServer
{
	std::atomic_bool ClientMessaging::asyncNetworkDataProcessingActive = false;
	std::unordered_map<avs::uid, NetworkPipeline*> ClientMessaging::networkPipelines;
	std::thread ClientMessaging::networkThread;
	std::mutex ClientMessaging::networkMutex;
	std::mutex ClientMessaging::dataMutex;
	avs::Timestamp ClientMessaging::lastTickTimestamp;

	ClientMessaging::ClientMessaging(const CasterSettings* settings,
		std::shared_ptr<DiscoveryService> discoveryService,
		std::shared_ptr<GeometryStreamingService> geometryStreamingService,
		std::function<void(avs::uid, const avs::Pose*)> inSetHeadPose,
		std::function<void(avs::uid, int index, const avs::Pose*)> inSetControllerPose,
		std::function<void(avs::uid, const avs::InputState *,const avs::InputEvent** )> inProcessNewInput,
		std::function<void(void)> onDisconnect,
		const uint32_t& disconnectTimeout
		,ReportHandshakeFn reportHandshakeFn)
		: settings(settings)
		, discoveryService(discoveryService)
		, geometryStreamingService(geometryStreamingService)
		, setHeadPose(inSetHeadPose)
		, setControllerPose(inSetControllerPose)
		, processNewInput(inProcessNewInput)
		, onDisconnect(onDisconnect)
		, reportHandshake(reportHandshakeFn)
		, disconnectTimeout(disconnectTimeout)
		, host(nullptr)
		, peer(nullptr)
		, casterContext(nullptr)
		, clientID(0)
	{}

	ClientMessaging::~ClientMessaging()
	{
		removeNetworkPipelineFromAsyncProcessing();
	}

	bool ClientMessaging::isInitialised() const
	{
		return initialized;
	}

	void ClientMessaging::unInitialise() 
	{
		 initialized=false;
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
		if(host)
		{
			TELEPORT_COUT<<"startSession - already have host ptr.\n";
		}
		else
		{
			host = enet_host_create(&ListenAddress, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
		}
		if(!host)
		{
			std::cerr << "Session: Failed to create ENET server host!\n";
			DEBUG_BREAK_ONCE;
			return false;
		}

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
		if (!receivedHandshake) return;

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

		if (settings->isReceivingAudio && casterContext->sourceNetworkPipeline.get())
		{
			casterContext->sourceNetworkPipeline->process();
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
				enet_peer_timeout(peer, 0, disconnectTimeout, disconnectTimeout * 6);
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

		// We may stop debugging on client and not receive an ENET_EVENT_TYPE_DISCONNECT so this should handle it. 
		if (host && peer && timeSinceLastMessage > disconnectTimeout / 1000)
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
		if(peer)
			enet_address_get_host_ip(&peer->address, address, sizeof(address));
		else
			sprintf(address,"");

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

	float ClientMessaging::getBandWidthKPS() const
	{
		if (casterContext && casterContext->NetworkPipeline)
		{
			std::lock_guard<std::mutex> lock(networkMutex);
			return casterContext->NetworkPipeline->getBandWidthKPS();
		}
		return 0;
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
				settings->requiredLatencyMs,
				disconnectTimeout
			};

			casterContext->NetworkPipeline.reset(new NetworkPipeline(settings));
			casterContext->NetworkPipeline->initialise(networkSettings, casterContext->ColorQueue.get(), casterContext->DepthQueue.get(), casterContext->GeometryQueue.get(), casterContext->AudioQueue.get());

			addNetworkPipelineToAsyncProcessing();
		}

		if (settings->isReceivingAudio && !casterContext->sourceNetworkPipeline)
		{
			avs::NetworkSourceParams sourceParams;
			sourceParams.connectionTimeout = disconnectTimeout;
			sourceParams.localPort = streamingPort;
			sourceParams.remoteIP = getClientIP().c_str();
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
			std::vector<avs::uid> streamedNodeIDs = geometryStreamingService->getStreameNodeIDs();

			avs::AcknowledgeHandshakeCommand ack(streamedNodeIDs.size());
			sendCommand<avs::uid>(ack, streamedNodeIDs);
		}
		reportHandshake(this->clientID,&handshake);
		TELEPORT_COUT << "RemotePlay: Started streaming to " << getClientIP() << ":" << streamingPort << std::endl;
	}

	bool ClientMessaging::setPosition(const avs::vec3& pos,bool set_rel,const avs::vec3 &rel_to_head)
	{
		avs::SetPositionCommand setp;
		if (casterContext->axesStandard != avs::AxesStandard::NotInitialized)
		{
			avs::vec3 p = pos;
			avs::ConvertPosition(settings->axesStandard, casterContext->axesStandard, p);
			setp.origin_pos = p;
			setp.set_relative_pos = (uint8_t)set_rel;
			avs::vec3 o=rel_to_head;
			avs::ConvertPosition(settings->axesStandard, casterContext->axesStandard, o);
			setp.relative_pos=o;
			return sendCommand(setp);
		}
		return false;
	}

	void ClientMessaging::receiveInput(const ENetPacket* packet)
	{
		if (packet->dataLength < sizeof(avs::InputState))
		{
			TELEPORT_CERR << "Session: Received malformed input state change packet of length: " << packet->dataLength << std::endl;
			return;
		}
		avs::InputState inputState;
		memcpy(&inputState, packet->data, sizeof(avs::InputState));
		if(packet->dataLength!=sizeof(avs::InputState)+inputState.numEvents*sizeof(avs::InputEvent))
		{
			TELEPORT_CERR << "Session: Received malformed input state change packet of length: " << packet->dataLength <<" but with "<<inputState.numEvents<<" events."<< std::endl;
			return;
		}
	
		std::vector<avs::InputEvent> inputEvents;
		//inputState.numEvents++;
		inputEvents.resize(inputState.numEvents);
		memcpy(const_cast<avs::InputEvent*>(inputEvents.data()), packet->data+ sizeof(avs::InputState),packet->dataLength-sizeof(avs::InputState));
		//inputEvents[inputEvents.size()-1]=inputEvents[0];
		const avs::InputEvent *v=inputEvents.data();
		processNewInput(clientID, &inputState, (const avs::InputEvent **)&v);
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
			TELEPORT_COUT << "Received keyframe request, but capture component isn't set.\n";
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
				networkThread = std::thread(&ClientMessaging::processNetworkDataAsync);
			}		
		}
	}
		 bool ClientMessaging::asyncNetworkDataProcessingFailed=false;

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
		else if(networkThread.joinable())
		{
			networkThread.join();
		}
	}

	void ClientMessaging::processNetworkDataAsync()
	{
		asyncNetworkDataProcessingFailed=false;
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
}
