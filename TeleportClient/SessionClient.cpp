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

void SessionClient::ConnectButtonHandler(avs::uid server_uid,const std::string& url)
{
	IpPort ipP=GetIpPort(url.c_str());
	auto sc=GetSessionClient(server_uid);
	sc->RequestConnection(ipP.ip,ipP.port?ipP.port:TELEPORT_SERVER_DISCOVERY_PORT);
}

void SessionClient::CancelConnectButtonHandler(avs::uid server_uid)
{
	auto sc=GetSessionClient(server_uid);
	sc->connectionRequest= client::ConnectionStatus::UNCONNECTED;
}

SessionClient::SessionClient(avs::uid s)
	:server_uid(s)
{}

SessionClient::~SessionClient()
{
	//Disconnect(0); causes crash. trying to access deleted objects.
}

void SessionClient::RequestConnection(const std::string &ip,int port)
{
	connectionRequest= client::ConnectionStatus::CONNECTED;
	if(server_ip==ip&&server_discovery_port==port&&(mServerPeer))
		return;
	SetServerIP(ip);
	SetServerDiscoveryPort(port);
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
		bool canConnect = connectionRequest == client::ConnectionStatus::CONNECTED;
		if (canConnect)
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
	ENetAddress remote;
	enet_address_set_host_ip(&remote, remote_ip);
	remote.port = remotePort;

	return Connect(remote, timeout,cl_id);
}

bool SessionClient::Connect(const ENetAddress& remote, uint timeout,avs::uid cl_id)
{
	mClientHost = enet_host_create(nullptr, 1, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
	if(!mClientHost)
	{
		TELEPORT_CLIENT_FAIL("Failed to create ENET client host");
		connectionRequest=ConnectionStatus::UNCONNECTED;
		remoteIP="";
		return false;
	}
	clientID=cl_id;
	mServerPeer = enet_host_connect(mClientHost, &remote, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_NumChannels), 0);
	if(!mServerPeer)
	{
		TELEPORT_CLIENT_WARN("Failed to initiate connection to the server");
		connectionRequest=ConnectionStatus::UNCONNECTED;
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
		remoteIP="";
		return false;
	}

	ENetEvent event;
	if(enet_host_service(mClientHost, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
	{
		mServerEndpoint = remote;

		char remote_ip[20];
		enet_address_get_host_ip(&mServerEndpoint, remote_ip, sizeof(remote_ip));
		TELEPORT_CLIENT_LOG("Connected to session server: %s:%d", remote_ip, remote.port);
		remoteIP=remote_ip;
		return true;
	}

	TELEPORT_CLIENT_WARN("Failed to connect to remote session server");
	connectionRequest=ConnectionStatus::UNCONNECTED;

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

	handshakeAcknowledged = false;
	receivedInitialPos = 0;
	if (resetClientID)
	{
		clientID = 0;
	}
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
	,bool requestKeyframe
	,double t
	,double deltaTime)
{
	time = t;

	if(mClientHost && mServerPeer)
	{
		if(handshakeAcknowledged)
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
		}

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
			TELEPORT_COUT << "Requesting resource " << sentResource.first << " again, as it has been " << timeSinceSent << " seconds since we sent the last request." << std::endl;
			mQueuedResourceRequests.push_back(sentResource.first);
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
	return mServerPeer?ConnectionStatus::CONNECTED:ConnectionStatus::UNCONNECTED;
}

bool SessionClient::IsConnecting() const
{
	if(connectionRequest==ConnectionStatus::CONNECTED&&mServerPeer==nullptr)
		return true;
	return false;
}

bool SessionClient::IsConnected() const
{
	return (mServerPeer!=nullptr);
}

void SessionClient::DispatchEvent(const ENetEvent& event)
{
	switch(event.channelID)
	{
		case static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control) :
			ReceiveCommandPacket(event.packet);
			break;
		default:
			TELEPORT_CLIENT_WARN("Received packet on output-only channel: %d", event.channelID);
			break;
	}

	enet_packet_destroy(event.packet);
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
			ReceivePositionUpdate(packet);
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
			break;
	};
}

void SessionClient::SendDisplayInfo (const avs::DisplayInfo &displayInfo)
{
	ENetPacket* packet = enet_packet_create(&displayInfo, sizeof(avs::DisplayInfo), 0);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_DisplayInfo), packet);
}

void SessionClient::SendNodePoses(const avs::Pose& headPose,const std::map<avs::uid,avs::PoseDynamic> poses)
{
	teleport::core::ControllerPosesMessage message;
	message.headPose=headPose;
	message.numPoses=(uint16_t)poses.size();
	if(isnan(headPose.position.x))
	{
		TELEPORT_CLIENT_WARN("Trying to send NaN");
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
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
}


static void copy_and_increment(uint8_t *&target,const void *source,size_t size)
{
	if(!size)
		return;
	memcpy(target, source, size);
	target+=size;
}

void receiveInput(const ENetPacket* packet)
{
	size_t inputStateSize = sizeof(teleport::core::InputState);

	if (packet->dataLength < inputStateSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_! Received malformed InputState packet of length " << packet->dataLength << "; less than minimum size of " << inputStateSize << "!\n";
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
		TELEPORT_CERR << "Error on receive input for Client_! Received malformed InputState packet of length " << packet->dataLength << "; expected size of " << inputStateSize + binaryEventSize + analogueEventSize + motionEventSize << "!\n" <<
			"     InputState Size: " << inputStateSize << "\n" <<
			"  Binary States Size:" << binaryStateSize << "(" << receivedInputState.numBinaryStates << ")\n" <<
			"Analogue States Size:" << analogueStateSize << "(" << receivedInputState.numAnalogueStates << ")\n" <<
			"  Binary Events Size:" << binaryEventSize << "(" << receivedInputState.numBinaryEvents << ")\n" <<
			"Analogue Events Size:" << analogueEventSize << "(" << receivedInputState.numAnalogueEvents << ")\n" <<
			"  Motion Events Size:" << motionEventSize << "(" << receivedInputState.numMotionEvents << ")\n";

		return;
	}
	core::Input latestInputStateAndEvents;
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
void SessionClient::SendInput(const core::Input& input)
{
	teleport::core::InputState inputState = {};
	enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;
	//Set event amount.
	if(input.analogueEvents.size()>50)
	{
		TELEPORT_BREAK_ONCE("That's a lot of events.");
	}
	inputState.numBinaryStates		= static_cast<uint32_t>(input.binaryStates.size());
	inputState.numAnalogueStates	= static_cast<uint32_t>(input.analogueStates.size());
	inputState.numBinaryEvents		= static_cast<uint32_t>(input.binaryEvents.size());
	inputState.numAnalogueEvents	= static_cast<uint32_t>(input.analogueEvents.size());
	inputState.numMotionEvents		= static_cast<uint32_t>(input.motionEvents.size());
	//Calculate sizes for memory copy operations.
	size_t inputStateSize		= sizeof(teleport::core::InputState);
	size_t binaryStateSize		= inputState.numBinaryStates;
	size_t analogueStateSize	= sizeof(float)*inputState.numAnalogueStates;
	size_t binaryEventSize		= sizeof(avs::InputEventBinary) * inputState.numBinaryEvents;
	size_t analogueEventSize	= sizeof(avs::InputEventAnalogue) * inputState.numAnalogueEvents;
	size_t motionEventSize		= sizeof(avs::InputEventMotion) * inputState.numMotionEvents;

	size_t packetSize=inputStateSize +binaryStateSize+ binaryEventSize +analogueStateSize+ analogueEventSize + motionEventSize;
	//std::cout<<"SendInput size "<<packetSize<<" with "<<inputState.numAnalogueStates<<" states.\n";
	//Size packet to final size, but initially only put the InputState struct inside.
	ENetPacket* packet = enet_packet_create(nullptr, packetSize, packetFlags);

	//Copy events into packet.
	uint8_t *target=packet->data;
	copy_and_increment(target,&inputState,inputStateSize);
	copy_and_increment(target,input.binaryStates.data(),binaryStateSize);
	if(input.analogueStates.size()>0&&input.analogueStates[0]!=0)
	{
		copy_and_increment(target,input.analogueStates.data(),analogueStateSize);
	}
	else
	{
		copy_and_increment(target,input.analogueStates.data(),analogueStateSize);
	}
	copy_and_increment(target,input.binaryEvents.data(),binaryEventSize);
	copy_and_increment(target,input.analogueEvents.data(),analogueEventSize);
	copy_and_increment(target,input.motionEvents.data(),motionEventSize);

	enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_Control), packet);
	//receiveInput(packet);
}

void SessionClient::SendResourceRequests()
{
	std::vector<avs::uid> resourceRequests = geometryCache->GetResourceRequests();
	geometryCache->ClearResourceRequests();
	//Append GeometryTargetBackendInterface's resource requests to SessionClient's resource requests.
	mQueuedResourceRequests.insert(mQueuedResourceRequests.end(), resourceRequests.begin(), resourceRequests.end());
	resourceRequests.clear();

	if(mQueuedResourceRequests.size() != 0)
	{
		size_t resourceCount = mQueuedResourceRequests.size();
		ENetPacket* packet = enet_packet_create(&resourceCount, sizeof(size_t) , ENET_PACKET_FLAG_RELIABLE);

		enet_packet_resize(packet, sizeof(size_t) + sizeof(avs::uid) * resourceCount);
		memcpy(packet->data + sizeof(size_t), mQueuedResourceRequests.data(), sizeof(avs::uid) * resourceCount);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_ResourceRequest), packet);

		//Store sent resource requests, so we can resend them if it has been too long since the request.
		for(avs::uid id : mQueuedResourceRequests)
		{
			mSentResourceRequests[id] = time;
			TELEPORT_INTERNAL_COUT("SessionClient::SendResourceRequests Requested {0}",id);
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
				mSentResourceRequests.erase(sentRequestIt);
			}
		}

		teleport::core::ReceivedResourcesMessage message(receivedResources.size());

		size_t messageSize = sizeof(teleport::core::ReceivedResourcesMessage);
		size_t receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedResourcesSize);
		memcpy(packet->data + messageSize, receivedResources.data(), receivedResourcesSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
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

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_ClientMessage), packet);

		mReceivedNodes.clear();
		mLostNodes.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
	ENetPacket* packet = enet_packet_create(0x0, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(teleport::core::RemotePlaySessionChannel::RPCH_KeyframeRequest), packet);
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

void SessionClient::ReceiveHandshakeAcknowledgement(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::AcknowledgeHandshakeCommand);

	//Extract command from packet.
	teleport::core::AcknowledgeHandshakeCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	//Extract list of visible nodes.
	std::vector<avs::uid> visibleNodes(command.visibleNodeCount);
	memcpy(visibleNodes.data(), packet->data + commandSize, sizeof(avs::uid) * command.visibleNodeCount);

	mCommandInterface->SetVisibleNodes(visibleNodes);

	handshakeAcknowledged = true;
}

void SessionClient::ReceiveSetupCommand(const ENetPacket* packet)
{
	size_t commandSize= sizeof(teleport::core::SetupCommand);

	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupCommand), packet->data, commandSize);

	teleport::core::Handshake handshake;
	char server_ip[100];
	enet_address_get_host_ip(&mServerEndpoint, server_ip, 99);
	if(!mCommandInterface->OnSetupCommandReceived(server_ip, setupCommand, handshake))
		return;

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

	mCommandInterface->OnReconfigureVideo(reconfigureCommand);
}

void SessionClient::ReceivePositionUpdate(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::SetStageSpaceOriginNodeCommand);

	teleport::core::SetStageSpaceOriginNodeCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	if(command.valid_counter > receivedInitialPos)
	{
		receivedInitialPos = command.valid_counter;
		mCommandInterface->SetOrigin(command.valid_counter,command.origin_node);
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
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	std::vector<teleport::core::MovementUpdate> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(teleport::core::MovementUpdate) * command.updatesCount);

	mCommandInterface->UpdateNodeMovement(updateList);
}

void SessionClient::ReceiveNodeEnabledStateUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::UpdateNodeEnabledStateCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	std::vector<teleport::core::NodeUpdateEnabledState> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(teleport::core::NodeUpdateEnabledState) * command.updatesCount);

	mCommandInterface->UpdateNodeEnabledState(updateList);
}

void SessionClient::ReceiveNodeHighlightUpdate(const ENetPacket* packet)
{
	teleport::core::SetNodeHighlightedCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->SetNodeHighlighted(command.nodeID, command.isHighlighted);
}

void SessionClient::ReceiveNodeAnimationUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::UpdateNodeAnimationCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimation(command.animationUpdate);
}

void SessionClient::ReceiveNodeAnimationControlUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	teleport::core::SetAnimationControlCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimationControl(command.animationControlUpdate);
}

void SessionClient::ReceiveNodeAnimationSpeedUpdate(const ENetPacket* packet)
{
	teleport::core::SetNodeAnimationSpeedCommand command;
	memcpy(static_cast<void*>(&command), packet->data, command.getCommandSize());

	mCommandInterface->SetNodeAnimationSpeed(command.nodeID, command.animationID, command.speed);
}

void SessionClient::ReceiveSetupLightingCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(teleport::core::SetupLightingCommand);
	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupLightingCommand), packet->data, commandSize);

	std::vector<avs::uid> uidList((size_t)setupLightingCommand.num_gi_textures);
	memcpy(uidList.data(), packet->data + commandSize, sizeof(avs::uid) * uidList.size());
	mCommandInterface->OnLightingSetupChanged(setupLightingCommand);
}

void SessionClient::ReceiveSetupInputsCommand(const ENetPacket* packet)
{
	teleport::core::SetupInputsCommand setupInputsCommand;
	memcpy(static_cast<void*>(&setupInputsCommand), packet->data, sizeof(teleport::core::SetupInputsCommand));

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
	mCommandInterface->AssignNodePosePath(assignNodePosePathCommand,str);
}
