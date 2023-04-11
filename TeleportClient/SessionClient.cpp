// (C) Copyright 2018-2022 Simul Software Ltd
#include "SessionClient.h"

#include <limits>
#include <fmt/core.h>

#include "libavstream/common.hpp"
#include "TeleportCore/CommonNetworking.h"
#include "libavstream/common_input.h"
#include <libavstream/geometry/mesh_interface.hpp>

#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"
#include "DiscoveryService.h"
#include "Config.h"

using namespace teleport;
using namespace client;
using namespace clientrender;
avs::Timestamp tBegin;

static std::map<avs::uid,std::shared_ptr<teleport::client::SessionClient>> sessionClients;

struct IpPort
{
	std::string ip;
	int port=0;
};
IpPort GetIpPort(const char *ip_port)
{
	std::string ip= ip_port;
	size_t pos=ip.find(":");
	IpPort ipPort;
	if(pos>=ip.length())
	{
		ipPort.port=0;
		ipPort.ip=ip;
	}
	else
	{
		ipPort.port=(atoi(ip.substr(pos+1,ip.length()-pos-1).c_str()));
		ipPort.ip=ip.substr(0,pos);
	}
	return ipPort;
}

std::shared_ptr<teleport::client::SessionClient> SessionClient::GetSessionClient(avs::uid server_uid)
{
	auto i=sessionClients.find(server_uid);
	if(i==sessionClients.end())
	{
		auto r=std::make_shared<client::SessionClient>(server_uid);
		sessionClients[server_uid]=r;
		return r;
	}
	return i->second;
}

void SessionClient::DestroySessionClients()
{
	for (auto c : sessionClients)
	{
		c.second=nullptr;
	}
	sessionClients.clear();
}


void SessionClient::ConnectButtonHandler(avs::uid server_uid,const std::string& url)
{
	IpPort ipP=GetIpPort(url.c_str());
	auto sc=GetSessionClient(server_uid);
	sc->RequestConnection(ipP.ip,ipP.port?ipP.port:TELEPORT_SERVER_DISCOVERY_PORT);
}

void SessionClient::CancelConnectButtonHandler(avs::uid server_uid)
{
	auto sc=GetSessionClient(server_uid);
	sc->connectionStatus= client::ConnectionStatus::UNCONNECTED;
}

SessionClient::SessionClient(avs::uid s)
	:server_uid(s)
{
}

SessionClient::~SessionClient()
{
	//Disconnect(0); causes crash. trying to access deleted objects.
}

void SessionClient::RequestConnection(const std::string &ip,int port)
{
	if (connectionStatus != client::ConnectionStatus::UNCONNECTED)
		return;
	if (server_ip != ip || server_discovery_port != port || !mServerPeer)
	{
		SetServerIP(ip);
		SetServerDiscoveryPort(port);
	}
	connectionStatus = client::ConnectionStatus::OFFERING;
}

void SessionClient::SetSessionCommandInterface(SessionCommandInterface *s)
{
	mCommandInterface=s;
}

void SessionClient::SetGeometryCache(avs::GeometryCacheBackendInterface* r)
{
	geometryCache = r;
}

bool SessionClient::HandleConnections()
{
	if (!IsConnected())
	{
		auto &config=Config::GetInstance();
		ENetAddress remoteEndpoint; 
		if (connectionStatus == client::ConnectionStatus::OFFERING)
		{
			uint64_t cl_id=teleport::client::DiscoveryService::GetInstance().Discover("", TELEPORT_CLIENT_DISCOVERY_PORT, server_ip.c_str(), server_discovery_port, remoteEndpoint);
			if(cl_id!=0&&Connect(remoteEndpoint, config.options.connectionTimeout,cl_id))
			{
				return true;
			}
		}
	}
	return false;
}

bool SessionClient::Connect(const char* remote_ip, uint16_t remotePort, uint timeout,avs::uid cl_id)
{
	tBegin = avs::Platform::getTimestamp();
	ENetAddress remote;
	enet_address_set_host_ip(&remote, remote_ip);
	remote.port = remotePort;

	return Connect(remote, timeout,cl_id);
}

bool SessionClient::Connect(const ENetAddress& remote, uint timeout,avs::uid cl_id)
{
	tBegin = avs::Platform::getTimestamp();
	// TODO: don't reset this if reconnecting.
	ResetSessionState();

	mClientHost = enet_host_create(nullptr, 1, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
	if(!mClientHost)
	{
		TELEPORT_CLIENT_FAIL("Failed to create ENET client host\n");
		connectionStatus=ConnectionStatus::UNCONNECTED;
		remoteIP="";
		return false;
	}
	clientID=cl_id;
	mServerPeer = enet_host_connect(mClientHost, &remote, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0);
	if(!mServerPeer)
	{
		TELEPORT_CLIENT_WARN("Failed to initiate connection to the server\n");
		connectionStatus=ConnectionStatus::UNCONNECTED;
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
		remoteIP="";
		return false;
	}

	ENetEvent event;
	if((enet_host_service(mClientHost, &event, timeout) > 0)&& event.type == ENET_EVENT_TYPE_CONNECT)
	{
		mServerEndpoint = remote;

		char remote_ip[20];
		enet_address_get_host_ip(&mServerEndpoint, remote_ip, sizeof(remote_ip));
		TELEPORT_CLIENT_LOG("Connected to session server: %s:%d\n", remote_ip, remote.port);
		remoteIP=remote_ip;
		connectionStatus = ConnectionStatus::HANDSHAKING;
		return true;
	}

	TELEPORT_CLIENT_WARN("Failed to connect to remote session server\n");
	connectionStatus=ConnectionStatus::UNCONNECTED;

	enet_host_destroy(mClientHost);
	mClientHost = nullptr;
	mServerPeer = nullptr;
	remoteIP="";
	mTimeSinceLastServerComm = 0;
	return false;
}

void SessionClient::Disconnect(uint timeout, bool resetClientID)
{
	remoteIP="";
	mCommandInterface->OnVideoStreamClosed();

	if(mClientHost && mServerPeer)
	{
		if(timeout == 0)
		{
			enet_peer_disconnect_now(mServerPeer, 0);
		}
		else
		{
			enet_peer_disconnect(mServerPeer, 0);

			bool isPeerConnected = true;
			ENetEvent event;
			while(isPeerConnected && enet_host_service(mClientHost, &event, timeout) > 0)
			{
				switch(event.type)
				{
					case ENET_EVENT_TYPE_RECEIVE:
						enet_packet_destroy(event.packet);
						break;
					case ENET_EVENT_TYPE_DISCONNECT:
						isPeerConnected = false;
						break;
					default:
						break;
				}
			}

			if(isPeerConnected)
			{
				enet_peer_reset(mServerPeer);
			}
		}
		mServerPeer = nullptr;
		mServerEndpoint = {};
	}

	if(mClientHost)
	{
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
	}

	connectionStatus = ConnectionStatus::UNCONNECTED;
	receivedInitialPos = 0;
	if (resetClientID)
	{
		clientID = 0;
	}
	clientPipeline.pipeline.deconfigure();
}

void SessionClient::SetPeerTimeout(uint timeout)
{
	if (IsConnected())
	{
		enet_peer_timeout(mServerPeer, 0, timeout, timeout * 6);
	}
}


void SessionClient::Frame(const avs::DisplayInfo &displayInfo
	,const avs::Pose &headPose
	,const std::map<avs::uid,avs::PoseDynamic> &nodePoses
	,uint64_t poseValidCounter
	,const avs::Pose &originPose
	,const core::Input &input
	,double t
	,double deltaTime)
{
	time = t;
	bool requestKeyframe = clientPipeline.decoder.idrRequired();
	if(mClientHost && mServerPeer)
	{
		SendDisplayInfo(displayInfo);
		if(poseValidCounter)
		{
			SendNodePoses(headPose,nodePoses);
		}
		SendInput(input);
		SendResourceRequests();
		SendReceivedResources();
		SendNodeUpdates();
		if(requestKeyframe)
			SendKeyframeRequest();
		
		ENetEvent event;
		while(enet_host_service(mClientHost, &event, 0) > 0)
		{
			switch(event.type)
			{
				case ENET_EVENT_TYPE_NONE:
					return;
				case ENET_EVENT_TYPE_CONNECT:
					mTimeSinceLastServerComm = 0;
					return;
				case ENET_EVENT_TYPE_RECEIVE:
					mTimeSinceLastServerComm = 0;
					DispatchEvent(event);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					TELEPORT_INTERNAL_COUT("ENet disconnected due to internal timeout. Should reconnect here.");
					mTimeSinceLastServerComm = 0;
					Disconnect(0);
					return;
			}
		}
	}

	mTimeSinceLastServerComm += deltaTime;

#if 0
	// Handle cases where we set a breakpoint and enet doesn't receive disconnect message
	// This only works with geometry streaming on. Otherwise there are not regular http messages received. 
	if (mTimeSinceLastServerComm > (setupCommand.idle_connection_timeout * 0.001) + 2)
	{
		Disconnect(0);
		return;
	}
#endif

	//Append resource requests to the request list again, if it has been too long since we sent the request.
	for(auto sentResource : mSentResourceRequests)
	{
		double timeSinceSent = time - sentResource.second;
		if(timeSinceSent > RESOURCE_REQUEST_RESEND_TIME)
		{
//			TELEPORT_COUT << "Requesting resource " << sentResource.first << " again, as it has been " << timeSinceSent << " seconds since we sent the last request." << std::endl;
			mQueuedResourceRequests.push_back(sentResource.first);
		}
	}
	// TODO: These pipelines could be on different threads,
	messageToServerPipeline.process();

	avs::Result result = clientPipeline.pipeline.process();
	if (result == avs::Result::Network_Disconnection)
	{
		TELEPORT_INTERNAL_CERR("Got avs::Result::Network_Disconnection. We should try to reconnect here.\n");
		Disconnect(0);
		return;
	}
	// Source might not yet be configured...
	if (clientPipeline.source)
	{
		std::string str;
		if (clientPipeline.source->getNextStreamingControlMessage(str))
		{
			SendStreamingControlMessage(str);
		}
	}
}

int SessionClient::GetServerDiscoveryPort() const
{
	return server_discovery_port;
}

void SessionClient::SetServerIP(std::string s) 
{
	IpPort ipP=GetIpPort(s.c_str());
	server_ip=ipP.ip;
	if(ipP.port)
		server_discovery_port=ipP.port;
}

void SessionClient::SetServerDiscoveryPort(int p) 
{
	server_discovery_port=p;
}


int SessionClient::GetPort() const
{
	if(!mServerPeer)
		return 0;
	return mServerPeer->address.port;
}

std::string SessionClient::GetServerIP() const
{
	return remoteIP;
}

ConnectionStatus SessionClient::GetConnectionStatus() const
{
	return connectionStatus;
}

bool SessionClient::IsConnecting() const
{
	if(connectionStatus==ConnectionStatus::OFFERING|| connectionStatus== ConnectionStatus::HANDSHAKING)
		return true;
	return false;
}

bool SessionClient::IsConnected() const
{
	return (connectionStatus == ConnectionStatus::CONNECTED|| connectionStatus == ConnectionStatus::HANDSHAKING);
}

void SessionClient::DispatchEvent(const ENetEvent& event)
{
	switch(event.channelID)
	{
	case static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control):
		ReceiveCommandPacket(event.packet);
		break;
	case static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_StreamingControl):
		ReceiveTextCommand(event.packet);
		break;
		default:
			TELEPORT_CLIENT_WARN("Received packet on output-only channel: %d\n", event.channelID);
			break;
	}

	enet_packet_destroy(event.packet);
}

avs::Result SessionClient::decode(const void* buffer, size_t bufferSizeInBytes)
{
	if (!buffer || bufferSizeInBytes < 1)
		return avs::Result::Failed;
	ENetPacket packet;
	packet.data = ((enet_uint8*)buffer);
	packet.dataLength = bufferSizeInBytes;
	//packet.
	//teleport::core::CommandPayloadType commandPayloadType = *(reinterpret_cast<const teleport::core::CommandPayloadType*>(buffer));
	ReceiveCommandPacket(&packet);
	return avs::Result::OK;
}

void SessionClient::ReceiveCommandPacket(ENetPacket* packet)
{
	teleport::core::CommandPayloadType commandPayloadType = *(reinterpret_cast<teleport::core::CommandPayloadType*>(packet->data));
	switch(commandPayloadType)
	{
		case teleport::core::CommandPayloadType::Shutdown:
			mCommandInterface->OnVideoStreamClosed();
			break;
		case teleport::core::CommandPayloadType::Setup:
			ReceiveSetupCommand(packet);
			break;
		case teleport::core::CommandPayloadType::AcknowledgeHandshake:
			ReceiveHandshakeAcknowledgement(packet);
			break;
		case teleport::core::CommandPayloadType::ReconfigureVideo:
			ReceiveVideoReconfigureCommand(packet);
			break;
		case teleport::core::CommandPayloadType::SetStageSpaceOriginNode:
			ReceiveStageSpaceOriginNodeId(packet);
			break;
		case teleport::core::CommandPayloadType::NodeVisibility:
			ReceiveNodeVisibilityUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::UpdateNodeMovement:
			ReceiveNodeMovementUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::UpdateNodeEnabledState:
			ReceiveNodeEnabledStateUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::SetNodeHighlighted:
			ReceiveNodeHighlightUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::UpdateNodeAnimation:
			ReceiveNodeAnimationUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::UpdateNodeAnimationControl:
			ReceiveNodeAnimationControlUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::SetNodeAnimationSpeed:
			ReceiveNodeAnimationSpeedUpdate(packet);
			break;
		case teleport::core::CommandPayloadType::SetupLighting:
			ReceiveSetupLightingCommand(packet);
			break;
		case teleport::core::CommandPayloadType::SetupInputs:
			ReceiveSetupInputsCommand(packet);
			break;
		case teleport::core::CommandPayloadType::UpdateNodeStructure:
			ReceiveUpdateNodeStructureCommand(packet);
			break;
		case teleport::core::CommandPayloadType::AssignNodePosePath:
			ReceiveAssignNodePosePathCommand(packet);
			break;
		default:
			TELEPORT_CERR << "Invalid CommandPayloadType.\n";
			TELEPORT_INTERNAL_BREAK_ONCE("Invalid payload");
			break;
	};
}

void SessionClient::SendDisplayInfo(const avs::DisplayInfo &displayInfo)
{
	core::DisplayInfoMessage displayInfoMessage;
	displayInfoMessage.displayInfo = displayInfo;

	SendMessageToServer(&displayInfoMessage, sizeof(displayInfoMessage));
}

void SessionClient::SendNodePoses(const avs::Pose& headPose,const std::map<avs::uid,avs::PoseDynamic> poses)
{
	teleport::core::ControllerPosesMessage message;

	auto ts = avs::Platform::getTimestamp();
	message.timestamp_unix_ms=(uint64_t)(avs::Platform::getTimeElapsedInMilliseconds(tBegin, ts));
#if 0
	static uint8_t c = 0;
	c--;
	if (!c)
	{
		std::cout << "SendNodePoses: " << double(message.timestamp_unix_ms)/ 1000.0 << std::endl;
		std::cout << "messageToServerStack: " << messageToServerStack.buffers.size() << "\n";
	}
#endif
	message.headPose=headPose;
	message.numPoses=(uint16_t)poses.size();
	if(isnan(headPose.position.x))
	{
		TELEPORT_CLIENT_WARN("Trying to send NaN\n");
		return;
	}
	size_t messageSize = sizeof(teleport::core::ControllerPosesMessage)+message.numPoses*sizeof(teleport::core::NodePose);
	ENetPacket* packet = enet_packet_create(&message, sizeof(teleport::core::ControllerPosesMessage), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
	enet_packet_resize(packet, messageSize);
	int i=0;
	teleport::core::NodePose nodePose;
	uint8_t *target=packet->data+sizeof(teleport::core::ControllerPosesMessage);
	for(const auto &p:poses)
	{
		nodePose.uid=p.first;
		nodePose.poseDynamic=p.second;
		memcpy(target,&nodePose,sizeof(nodePose));
		target+=sizeof(nodePose);
	}
	// This is a special type of message, with its own queue.
	clientPipeline.nodePosesQueue.push(packet->data, messageSize);
}


static void copy_and_increment(uint8_t *&target,const void *source,size_t size)
{
	if(!size)
		return;
	memcpy(target, source, size);
	target+=size;
}

void SessionClient::SendInput(const core::Input& input)
{
	teleport::core::InputStatesMessage inputStatesMessage;
	teleport::core::InputEventsMessage inputEventsMessage;
	auto ts = avs::Platform::getTimestamp();
	inputStatesMessage.timestamp_unix_ms = (uint64_t)(avs::Platform::getTimeElapsedInMilliseconds(tBegin, ts));
	inputEventsMessage.timestamp_unix_ms = inputStatesMessage.timestamp_unix_ms;
	enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;
	//Set event amount.
	if(input.analogueEvents.size()>50)
	{
		TELEPORT_BREAK_ONCE("That's a lot of events.");
	}
	inputStatesMessage.inputState.numBinaryStates	= static_cast<uint32_t>(input.binaryStates.size());
	inputStatesMessage.inputState.numAnalogueStates	= static_cast<uint32_t>(input.analogueStates.size());
	inputEventsMessage.numBinaryEvents	= static_cast<uint32_t>(input.binaryEvents.size());
	inputEventsMessage.numAnalogueEvents	= static_cast<uint32_t>(input.analogueEvents.size());
	inputEventsMessage.numMotionEvents	= static_cast<uint32_t>(input.motionEvents.size());
	//Calculate sizes for memory copy operations.
	size_t inputStateSize		= sizeof(inputStatesMessage);
	size_t binaryStateSize		= inputStatesMessage.inputState.numBinaryStates;
	size_t analogueStateSize	= sizeof(float)* inputStatesMessage.inputState.numAnalogueStates;

	size_t statesPacketSize = inputStateSize + binaryStateSize + analogueStateSize;
	//Copy events into packet.
	{
		std::vector<uint8_t> buffer(statesPacketSize);
		uint8_t* target = buffer.data();
		copy_and_increment(target, &inputStatesMessage, inputStateSize);
		copy_and_increment(target, input.binaryStates.data(), binaryStateSize);
		copy_and_increment(target, input.analogueStates.data(), analogueStateSize);
		// This is a special type of message, with its own queue.
		clientPipeline.inputStateQueue.push(buffer.data(), statesPacketSize);
	}
	{
		size_t binaryEventSize = sizeof(avs::InputEventBinary) * inputEventsMessage.numBinaryEvents;
		size_t analogueEventSize = sizeof(avs::InputEventAnalogue) * inputEventsMessage.numAnalogueEvents;
		size_t motionEventSize = sizeof(avs::InputEventMotion) * inputEventsMessage.numMotionEvents;

		size_t inputEventsSize = sizeof(inputEventsMessage);
		size_t eventsPacketSize = sizeof(inputEventsMessage) + binaryEventSize + analogueEventSize + motionEventSize;


		std::vector<uint8_t> buffer(eventsPacketSize);
		uint8_t* target = buffer.data();
		copy_and_increment(target, &inputEventsMessage, inputEventsSize);
		copy_and_increment(target, input.binaryEvents.data(), binaryEventSize);
		copy_and_increment(target, input.analogueEvents.data(), analogueEventSize);
		copy_and_increment(target, input.motionEvents.data(), motionEventSize);
		SendMessageToServer(buffer.data(), eventsPacketSize);
	}
}

void SessionClient::SendResourceRequests()
{
	std::vector<avs::uid> resourceRequests = geometryCache->GetResourceRequests();
	geometryCache->ClearResourceRequests();
	//Append GeometryTargetBackendInterface's resource requests to SessionClient's resource requests.
	mQueuedResourceRequests.insert(mQueuedResourceRequests.end(), resourceRequests.begin(), resourceRequests.end());

	if(mQueuedResourceRequests.size() != 0)
	{
		resourceRequests.clear();
		teleport::core::ResourceRequestMessage resourceRequestMessage;
		resourceRequestMessage.resourceCount = (uint16_t)mQueuedResourceRequests.size();
		if ((size_t)resourceRequestMessage.resourceCount != mQueuedResourceRequests.size())
		{
			TELEPORT_INTERNAL_CERR("Bad resourceCount {0}", mQueuedResourceRequests.size());
			return;
		}
		ENetPacket* packet = enet_packet_create(&resourceRequestMessage, sizeof(resourceRequestMessage), ENET_PACKET_FLAG_RELIABLE);

		size_t totalSize = sizeof(resourceRequestMessage) + sizeof(avs::uid) * resourceRequestMessage.resourceCount;
		enet_packet_resize(packet, totalSize);
		memcpy(packet->data + sizeof(resourceRequestMessage), mQueuedResourceRequests.data(), sizeof(avs::uid)*resourceRequestMessage.resourceCount);
#ifndef FIX_BROKEN
		SendMessageToServer(packet->data, totalSize);
#endif
		//Store sent resource requests, so we can resend them if it has been too long since the request.
		for(avs::uid id : mQueuedResourceRequests)
		{
			mSentResourceRequests[id] = time;
//			TELEPORT_INTERNAL_COUT("SessionClient::SendResourceRequests Requested {0}",id);
		}
		mQueuedResourceRequests.clear();
	}
}

void SessionClient::SendReceivedResources()
{
	std::vector<avs::uid> receivedResources = geometryCache->GetReceivedResources();
	geometryCache->ClearReceivedResources();

	if(receivedResources.size() != 0)
	{
		//Stop tracking resource requests we have now received.
		for(avs::uid id : receivedResources)
		{
			auto sentRequestIt = mSentResourceRequests.find(id);
			if(sentRequestIt != mSentResourceRequests.end())
			{
//				TELEPORT_INTERNAL_COUT("mSentResourceRequests Received {0}", id);
				mSentResourceRequests.erase(sentRequestIt);
			}
		}

		teleport::core::ReceivedResourcesMessage message(receivedResources.size());

		size_t messageSize = sizeof(teleport::core::ReceivedResourcesMessage);
		size_t receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedResourcesSize);
		memcpy(packet->data + messageSize, receivedResources.data(), receivedResourcesSize);

		SendMessageToServer(packet->data, messageSize + receivedResourcesSize);
	}
}

void SessionClient::SendNodeUpdates()
{
	//Insert completed nodes.
	{
		std::vector<avs::uid> completedNodes = geometryCache->GetCompletedNodes();
		geometryCache->ClearCompletedNodes();
		mReceivedNodes.insert(mReceivedNodes.end(), completedNodes.begin(), completedNodes.end());
	}

	if(mReceivedNodes.size() != 0 || mLostNodes.size() != 0)
	{
		teleport::core::NodeStatusMessage message(mReceivedNodes.size(), mLostNodes.size());

		size_t messageSize = sizeof(teleport::core::NodeStatusMessage);
		size_t receivedSize = sizeof(avs::uid) * mReceivedNodes.size();
		size_t lostSize = sizeof(avs::uid) * mLostNodes.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedSize + lostSize);
		memcpy(packet->data + messageSize, mReceivedNodes.data(), receivedSize);
		memcpy(packet->data + messageSize + receivedSize, mLostNodes.data(), lostSize);

#ifndef FIX_BROKEN
		SendMessageToServer(packet->data, messageSize + receivedSize + lostSize);
#endif
		mReceivedNodes.clear();
		mLostNodes.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
	teleport::core::KeyframeRequestMessage msg;
	SendMessageToServer(&msg, sizeof(msg));
}

bool SessionClient::SendMessageToServer(const void* c, size_t sz) const
{
	if (sz > 16384)
		return false;
	auto b = std::make_shared<std::vector<uint8_t>>(sz);
	memcpy(b->data(), c, sz);
	messageToServerStack.PushBuffer(b);
	return true;
}

void SessionClient::SendHandshake(const teleport::core::Handshake& handshake, const std::vector<avs::uid>& clientResourceIDs)
{
	size_t handshakeSize = sizeof(teleport::core::Handshake);
	size_t resourceListSize = sizeof(avs::uid) * clientResourceIDs.size();

	//Create handshake.
	ENetPacket* packet = enet_packet_create(&handshake, handshakeSize, ENET_PACKET_FLAG_RELIABLE);
	//Append list of resource IDs the client has.
	enet_packet_resize(packet, handshakeSize + resourceListSize);
	memcpy(packet->data + handshakeSize, clientResourceIDs.data(), resourceListSize);

	enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Handshake), packet);
}

void SessionClient::SendStreamingControlMessage(const std::string& msg)
{
	// messages to be sent as text e.g. WebRTC config.
	uint16_t len = (uint16_t)msg.size();
	if ((size_t)len == msg.size())
	{
		size_t sz = sizeof(len);
		ENetPacket* packet = enet_packet_create(nullptr, msg.size() + sz, ENET_PACKET_FLAG_RELIABLE);
		memcpy(packet->data, &len, sz);
		memcpy((packet->data + sz), msg.data(), len);
		if (packet)
			enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_StreamingControl), packet);
	}
}

void SessionClient::ReceiveHandshakeAcknowledgement(const ENetPacket* packet)
{
	if (connectionStatus != ConnectionStatus::HANDSHAKING)
	{
		TELEPORT_INTERNAL_CERR("Received handshake acknowledgement, but not in HANDSHAKING mode.\n");
		return;
	}
	size_t commandSize = sizeof(teleport::core::AcknowledgeHandshakeCommand);

	//Extract command from packet.
	teleport::core::AcknowledgeHandshakeCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	//Extract list of visible nodes.
	std::vector<avs::uid> visibleNodes(command.visibleNodeCount);
	memcpy(visibleNodes.data(), packet->data + commandSize, sizeof(avs::uid) * command.visibleNodeCount);

	mCommandInterface->SetVisibleNodes(visibleNodes);

	connectionStatus = ConnectionStatus::CONNECTED;
}

void SessionClient::ReceiveSetupCommand(const ENetPacket* packet)
{
	size_t commandSize= sizeof(teleport::core::SetupCommand);
	if(packet->dataLength!=commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad SetupCommand. Size should be {0} but it's {1}",commandSize,packet->dataLength);
		return;
	}
	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupCommand), packet->data, commandSize);

	teleport::core::Handshake handshake;
	char server_ip[100];
	enet_address_get_host_ip(&mServerEndpoint, server_ip, 99);
	if (!clientPipeline.Init(setupCommand, server_ip))
		return;
	if(!mCommandInterface->OnSetupCommandReceived(server_ip, setupCommand, handshake))
		return;

	messageToServerEncoder.configure(&messageToServerStack);

	messageToServerPipeline.link({ &messageToServerEncoder, &clientPipeline.messageToServerQueue });
	std::vector<avs::uid> resourceIDs;
	if(setupCommand.server_id == lastServerID)
	{
		resourceIDs = mCommandInterface->GetGeometryResources();
		handshake.resourceCount = resourceIDs.size();
	}
	else
	{
		mCommandInterface->ClearGeometryResources();
	}

	SendHandshake(handshake, resourceIDs);
	lastServerID = setupCommand.server_id;
}

void SessionClient::ReceiveVideoReconfigureCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::ReconfigureVideoCommand);

	//Copy command out of packet.
	teleport::core::ReconfigureVideoCommand reconfigureCommand;
	memcpy(static_cast<void*>(&reconfigureCommand), packet->data, commandSize);
	setupCommand.video_config = reconfigureCommand.video_config;
	mCommandInterface->OnReconfigureVideo(reconfigureCommand);
}

void SessionClient::ReceiveStageSpaceOriginNodeId(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::SetStageSpaceOriginNodeCommand);

	teleport::core::SetStageSpaceOriginNodeCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);
	if(command.valid_counter > receivedInitialPos)
	{
		TELEPORT_INTERNAL_COUT("Received origin node {0} with counter {1}.", command.origin_node, command.valid_counter);
		receivedInitialPos = command.valid_counter;
		mCommandInterface->SetOrigin(command.valid_counter,command.origin_node);
	}
	else
	{
		TELEPORT_INTERNAL_CERR("Received out-of-date origin node {0}, counter was {1}, but last update was {2}.",command.origin_node,command.valid_counter,receivedInitialPos);
	}
}

void SessionClient::ReceiveNodeVisibilityUpdate(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::NodeVisibilityCommand);

	teleport::core::NodeVisibilityCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	size_t enteredSize = sizeof(avs::uid) * command.nodesShowCount;
	size_t leftSize = sizeof(avs::uid) * command.nodesHideCount;

	std::vector<avs::uid> enteredNodes(command.nodesShowCount);
	std::vector<avs::uid> leftNodes(command.nodesHideCount);

	memcpy(enteredNodes.data(), packet->data + commandSize, enteredSize);
	memcpy(leftNodes.data(), packet->data + commandSize + enteredSize, leftSize);

	std::vector<avs::uid> missingNodes;
	//Tell the renderer to show the nodes that have entered the streamable bounds; create resend requests for nodes it does not have the data on, and confirm nodes it does have the data for.
	for(avs::uid node_uid : enteredNodes)
	{
		if(!mCommandInterface->OnNodeEnteredBounds(node_uid))
		{
			missingNodes.push_back(node_uid);
		}
		else
		{
			mReceivedNodes.push_back(node_uid);
		}
	}
	mQueuedResourceRequests.insert(mQueuedResourceRequests.end(), missingNodes.begin(), missingNodes.end());

	//Tell renderer to hide nodes that have left bounds.
	for(avs::uid node_uid : leftNodes)
	{
		if(mCommandInterface->OnNodeLeftBounds(node_uid))
		{
			mLostNodes.push_back(node_uid);
		}
	}
}

void SessionClient::ReceiveNodeMovementUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::UpdateNodeMovementCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength <commandSize )
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void*>(&command), packet->data, commandSize);
	size_t fullSize = commandSize + sizeof(teleport::core::MovementUpdate) * command.updatesCount;
	if (packet->dataLength != fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	std::vector<teleport::core::MovementUpdate> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(teleport::core::MovementUpdate) * command.updatesCount);

	mCommandInterface->UpdateNodeMovement(updateList);
}

void SessionClient::ReceiveNodeEnabledStateUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::UpdateNodeEnabledStateCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	size_t fullSize = commandSize + sizeof(teleport::core::NodeUpdateEnabledState) * command.updatesCount;
	if (packet->dataLength != fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	std::vector<teleport::core::NodeUpdateEnabledState> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(teleport::core::NodeUpdateEnabledState) * command.updatesCount);

	mCommandInterface->UpdateNodeEnabledState(updateList);
}

void SessionClient::ReceiveNodeHighlightUpdate(const ENetPacket* packet)
{
	teleport::core::SetNodeHighlightedCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->SetNodeHighlighted(command.nodeID, command.isHighlighted);
}

void SessionClient::ReceiveNodeAnimationUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::UpdateNodeAnimationCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimation(command.animationUpdate);
}

void SessionClient::ReceiveNodeAnimationControlUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::SetAnimationControlCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimationControl(command.animationControlUpdate);
}

void SessionClient::ReceiveNodeAnimationSpeedUpdate(const ENetPacket* packet)
{
	teleport::core::SetNodeAnimationSpeedCommand command;
	size_t commandSize = command.getCommandSize();
	if (packet->dataLength != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}

	memcpy(static_cast<void*>(&command), packet->data, command.getCommandSize());
	mCommandInterface->SetNodeAnimationSpeed(command.nodeID, command.animationID, command.speed);
}

void SessionClient::ReceiveSetupLightingCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::SetupLightingCommand);
	if (packet->dataLength != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}

	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupLightingCommand), packet->data, commandSize);

	std::vector<avs::uid> uidList((size_t)setupLightingCommand.num_gi_textures);
	memcpy(uidList.data(), packet->data + commandSize, sizeof(avs::uid) * uidList.size());
}

void SessionClient::ReceiveSetupInputsCommand(const ENetPacket* packet)
{
	teleport::core::SetupInputsCommand setupInputsCommand;
	size_t commandSize = sizeof(teleport::core::SetupInputsCommand);
	if (packet->dataLength <commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}

	memcpy(static_cast<void*>(&setupInputsCommand), packet->data, sizeof(teleport::core::SetupInputsCommand));

	size_t fullSize = commandSize + sizeof(teleport::core::InputDefinitionNetPacket) * setupInputsCommand.numInputs;
	if (packet->dataLength< fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	inputDefinitions.resize(setupInputsCommand.numInputs);
	unsigned char* ptr = packet->data + sizeof(teleport::core::SetupInputsCommand);
	for (int i = 0; i < setupInputsCommand.numInputs; i++)
	{
		if (size_t(ptr -packet->data) >= packet->dataLength)
		{
			TELEPORT_CERR << "Bad packet" << std::endl;
			return;
		}
		auto& def = inputDefinitions[i];
		teleport::core::InputDefinitionNetPacket& packetDef = *((teleport::core::InputDefinitionNetPacket*)ptr);
		ptr += sizeof(teleport::core::InputDefinitionNetPacket);
		if (size_t(ptr + packetDef.pathLength - packet->data) > packet->dataLength)
		{
			TELEPORT_CERR << "Bad packet" << std::endl;
			return;
		}
		def.inputId = packetDef.inputId;
		def.inputType = packetDef.inputType;
		def.regexPath.resize(packetDef.pathLength);
		memcpy(def.regexPath.data(), ptr, packetDef.pathLength);
		ptr += packetDef.pathLength;
	}
	if (size_t(ptr - packet->data) != packet->dataLength)
	{
		TELEPORT_CERR << "Bad packet" << std::endl;
		return;
	}
	// Now process the input definitions according to the available hardware:
	mCommandInterface->OnInputsSetupChanged(inputDefinitions);
}

void SessionClient::ReceiveUpdateNodeStructureCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::UpdateNodeStructureCommand);
	//Copy command out of packet.
	teleport::core::UpdateNodeStructureCommand updateNodeStructureCommand;
	memcpy(static_cast<void*>(&updateNodeStructureCommand), packet->data, commandSize);
	mCommandInterface->UpdateNodeStructure(updateNodeStructureCommand);
}

void SessionClient::ReceiveAssignNodePosePathCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::AssignNodePosePathCommand);
	if(packet->dataLength<commandSize)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	//Copy command out of packet.
	teleport::core::AssignNodePosePathCommand assignNodePosePathCommand;
	memcpy(static_cast<void*>(&assignNodePosePathCommand), packet->data, commandSize);
	if(packet->dataLength!=commandSize+ assignNodePosePathCommand.pathLength)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	std::string str;
	str.resize(assignNodePosePathCommand.pathLength);
	memcpy(static_cast<void*>(str.data()), packet->data+commandSize, assignNodePosePathCommand.pathLength);
	nodePosePaths[assignNodePosePathCommand.nodeID] = str;
	TELEPORT_INTERNAL_COUT("Received pose for node {0}: {1}", assignNodePosePathCommand.nodeID, str);
	mCommandInterface->AssignNodePosePath(assignNodePosePathCommand,str);
}

void SessionClient::ReceiveTextCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(uint16_t);
	if (packet->dataLength < commandSize)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	//Copy command out of packet.
	uint16_t count = 0;
	memcpy(static_cast<void*>(&count), packet->data, commandSize);
	if (packet->dataLength != commandSize + count)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	std::string str;
	str.resize(count);
	memcpy(static_cast<void*>(str.data()), packet->data + commandSize, count);
	mCommandInterface->OnStreamingControlMessage( str);
	if (clientPipeline.source)
		clientPipeline.source->receiveStreamingControlMessage(str);
}
//! Reset the session state when connecting to a new server, or when reconnecting without preserving the session:
void SessionClient::ResetSessionState()
{
	memset(&setupCommand,0,sizeof(setupCommand));
	memset(&setupLightingCommand, 0, sizeof(setupLightingCommand));
	inputDefinitions.clear();
	nodePosePaths.clear();
}