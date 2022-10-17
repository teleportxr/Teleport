// (C) Copyright 2018-2022 Simul Software Ltd
#include "SessionClient.h"

#include <limits>

#include "libavstream/common.hpp"
#include "libavstream/common_networking.h"
#include "libavstream/common_input.h"
#include <libavstream/geometry/mesh_interface.hpp>

#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

using namespace teleport;
using namespace client;
using namespace clientrender;

SessionClient::SessionClient( std::unique_ptr<DiscoveryService>&& discoveryService)
	:  discoveryService(std::move(discoveryService))
{}

SessionClient::~SessionClient()
{
	//Disconnect(0); causes crash. trying to access deleted objects.
}

void SessionClient::SetSessionCommandInterface(SessionCommandInterface *s)
{
	mCommandInterface=s;
}

void SessionClient::SetResourceCreator(avs::GeometryTargetBackendInterface *r)
{
	mResourceCreator=r;
}

void SessionClient::SetGeometryCache(avs::GeometryCacheBackendInterface* r)
{
	geometryCache = r;
}

uint64_t SessionClient::Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote)
{
	uint64_t cl_id=discoveryService->Discover(clientIP, clientDiscoveryPort, serverIP, serverDiscoveryPort, remote);
	if(cl_id!=0)
	{
		clientID=cl_id;
		discovered=true;
	}
	return cl_id;
}

bool SessionClient::Connect(const char* remote_ip, uint16_t remotePort, uint timeout)
{
	ENetAddress remote;
	enet_address_set_host_ip(&remote, remote_ip);
	remote.port = remotePort;

	return Connect(remote, timeout);
}

bool SessionClient::Connect(const ENetAddress& remote, uint timeout)
{
	mClientHost = enet_host_create(nullptr, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
	if(!mClientHost)
	{
		TELEPORT_CLIENT_FAIL("Failed to create ENET client host");
		remoteIP="";
		return false;
	}

	mServerPeer = enet_host_connect(mClientHost, &remote, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0);
	if(!mServerPeer)
	{
		TELEPORT_CLIENT_WARN("Failed to initiate connection to the server");
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
		webspaceLocation = WebspaceLocation::SERVER;
		return true;
	}

	TELEPORT_CLIENT_WARN("Failed to connect to remote session server");

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
	receivedInitialPos = false;
	discovered = false;
	if (resetClientID)
	{
		clientID = 0;
	}

	webspaceLocation = WebspaceLocation::LOBBY;
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
	,const std::map<avs::uid,avs::Pose> &controllerPoses
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
				SendControllerPoses(headPose,controllerPoses);
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

bool SessionClient::IsConnected() const
{
	return mServerPeer != nullptr;
}

bool SessionClient::HasDiscovered() const
{
	return discovered;
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

avs::Pose SessionClient::GetOriginPose() const
{
	return originPose;
}

void SessionClient::DispatchEvent(const ENetEvent& event)
{
	switch(event.channelID)
	{
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control) :
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
	avs::CommandPayloadType commandPayloadType = *(reinterpret_cast<avs::CommandPayloadType*>(packet->data));
	switch(commandPayloadType)
	{
		case avs::CommandPayloadType::Shutdown:
			mCommandInterface->OnVideoStreamClosed();
			break;
		case avs::CommandPayloadType::Setup:
			ReceiveSetupCommand(packet);
			break;
		case avs::CommandPayloadType::AcknowledgeHandshake:
			ReceiveHandshakeAcknowledgement(packet);
			break;
		case avs::CommandPayloadType::ReconfigureVideo:
			ReceiveVideoReconfigureCommand(packet);
			break;
		case avs::CommandPayloadType::SetPosition:
			ReceivePositionUpdate(packet);
			break;
		case avs::CommandPayloadType::NodeVisibility:
			ReceiveNodeVisibilityUpdate(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeMovement:
			ReceiveNodeMovementUpdate(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeEnabledState:
			ReceiveNodeEnabledStateUpdate(packet);
			break;
		case avs::CommandPayloadType::SetNodeHighlighted:
			ReceiveNodeHighlightUpdate(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeAnimation:
			ReceiveNodeAnimationUpdate(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeAnimationControl:
			ReceiveNodeAnimationControlUpdate(packet);
			break;
		case avs::CommandPayloadType::SetNodeAnimationSpeed:
			ReceiveNodeAnimationSpeedUpdate(packet);
			break;
		case avs::CommandPayloadType::SetupLighting:
			ReceiveSetupLightingCommand(packet);
			break;
		case avs::CommandPayloadType::SetupInputs:
			ReceiveSetupInputsCommand(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeStructure:
			ReceiveUpdateNodeStructureCommand(packet);
			break;
		case avs::CommandPayloadType::UpdateNodeSubtype:
			ReceiveUpdateNodeSubtypeCommand(packet);
			break;
		default:
			break;
	};
}

void SessionClient::SendDisplayInfo (const avs::DisplayInfo &displayInfo)
{
	ENetPacket* packet = enet_packet_create(&displayInfo, sizeof(avs::DisplayInfo), 0);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_DisplayInfo), packet);
}

void SessionClient::SendControllerPoses(const avs::Pose& headPose,const std::map<avs::uid,avs::Pose> poses)
{
	avs::ControllerPosesMessage message;
	message.headPose=headPose;
	message.numPoses=(uint16_t)poses.size();
	if(isnan(headPose.position.x))
	{
		TELEPORT_CLIENT_WARN("Trying to send NaN");
		return;
	}
	size_t messageSize = sizeof(avs::ControllerPosesMessage)+message.numPoses*sizeof(avs::NodePose);
	ENetPacket* packet = enet_packet_create(&message, sizeof(avs::ControllerPosesMessage), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
	enet_packet_resize(packet, messageSize);
	int i=0;
	avs::NodePose nodePose;
	uint8_t *target=packet->data+sizeof(avs::ControllerPosesMessage);
	for(const auto &p:poses)
	{
		nodePose.uid=p.first;
		nodePose.pose=p.second;
		memcpy(target,&nodePose,sizeof(nodePose));
		target+=sizeof(nodePose);
	}
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
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
	size_t inputStateSize = sizeof(avs::InputState);

	if (packet->dataLength < inputStateSize)
	{
		TELEPORT_CERR << "Error on receive input for Client_! Received malformed InputState packet of length " << packet->dataLength << "; less than minimum size of " << inputStateSize << "!\n";
		return;
	}

	avs::InputState receivedInputState;
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
	avs::InputState inputState = {};
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
	size_t inputStateSize		= sizeof(avs::InputState);
	size_t binaryStateSize		= inputState.numBinaryStates;
	size_t analogueStateSize	= sizeof(float)*inputState.numAnalogueStates;
	size_t binaryEventSize		= sizeof(avs::InputEventBinary) * inputState.numBinaryEvents;
	size_t analogueEventSize	= sizeof(avs::InputEventAnalogue) * inputState.numAnalogueEvents;
	size_t motionEventSize		= sizeof(avs::InputEventMotion) * inputState.numMotionEvents;

	size_t packetSize=inputStateSize +binaryStateSize+ binaryEventSize +analogueStateSize+ analogueEventSize + motionEventSize;
	std::cout<<"SendInput size "<<packetSize<<" with "<<inputState.numAnalogueStates<<" states.\n";
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

	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);
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

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ResourceRequest), packet);

		//Store sent resource requests, so we can resend them if it has been too long since the request.
		for(avs::uid id : mQueuedResourceRequests)
		{
			mSentResourceRequests[id] = time;
			TELEPORT_COUT<<"SessionClient::SendResourceRequests Requested "<<id<<std::endl;
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

		avs::ReceivedResourcesMessage message(receivedResources.size());

		size_t messageSize = sizeof(avs::ReceivedResourcesMessage);
		size_t receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedResourcesSize);
		memcpy(packet->data + messageSize, receivedResources.data(), receivedResourcesSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
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
		avs::NodeStatusMessage message(mReceivedNodes.size(), mLostNodes.size());

		size_t messageSize = sizeof(avs::NodeStatusMessage);
		size_t receivedSize = sizeof(avs::uid) * mReceivedNodes.size();
		size_t lostSize = sizeof(avs::uid) * mLostNodes.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedSize + lostSize);
		memcpy(packet->data + messageSize, mReceivedNodes.data(), receivedSize);
		memcpy(packet->data + messageSize + receivedSize, mLostNodes.data(), lostSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);

		mReceivedNodes.clear();
		mLostNodes.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
	ENetPacket* packet = enet_packet_create(0x0, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_KeyframeRequest), packet);
}

void SessionClient::SendHandshake(const avs::Handshake& handshake, const std::vector<avs::uid>& clientResourceIDs)
{
	size_t handshakeSize = sizeof(avs::Handshake);
	size_t resourceListSize = sizeof(avs::uid) * clientResourceIDs.size();

	//Create handshake.
	ENetPacket* packet = enet_packet_create(&handshake, handshakeSize, ENET_PACKET_FLAG_RELIABLE);
	//Append list of resource IDs the client has.
	enet_packet_resize(packet, handshakeSize + resourceListSize);
	memcpy(packet->data + handshakeSize, clientResourceIDs.data(), resourceListSize);

	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Handshake), packet);
}

void SessionClient::ReceiveHandshakeAcknowledgement(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::AcknowledgeHandshakeCommand);

	//Extract command from packet.
	avs::AcknowledgeHandshakeCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	//Extract list of visible nodes.
	std::vector<avs::uid> visibleNodes(command.visibleNodeCount);
	memcpy(visibleNodes.data(), packet->data + commandSize, sizeof(avs::uid) * command.visibleNodeCount);

	mCommandInterface->SetVisibleNodes(visibleNodes);

	handshakeAcknowledged = true;
}

void SessionClient::ReceiveSetupCommand(const ENetPacket* packet)
{
	size_t commandSize= sizeof(avs::SetupCommand);

	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupCommand), packet->data, commandSize);

	avs::Handshake handshake;
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
	size_t commandSize = sizeof(avs::ReconfigureVideoCommand);

	//Copy command out of packet.
	avs::ReconfigureVideoCommand reconfigureCommand;
	memcpy(static_cast<void*>(&reconfigureCommand), packet->data, commandSize);

	mCommandInterface->OnReconfigureVideo(reconfigureCommand);
}

void SessionClient::ReceivePositionUpdate(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::SetPositionCommand);

	avs::SetPositionCommand command;
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	if(command.valid_counter > receivedInitialPos)
	{
		receivedInitialPos = command.valid_counter;
		originPose.position = command.origin_pos;
		originPose.orientation = command.orientation;
	}
}

void SessionClient::ReceiveNodeVisibilityUpdate(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::NodeVisibilityCommand);

	avs::NodeVisibilityCommand command;
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
	avs::UpdateNodeMovementCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	std::vector<avs::MovementUpdate> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(avs::MovementUpdate) * command.updatesCount);

	mCommandInterface->UpdateNodeMovement(updateList);
}

void SessionClient::ReceiveNodeEnabledStateUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	avs::UpdateNodeEnabledStateCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	std::vector<avs::NodeUpdateEnabledState> updateList(command.updatesCount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(avs::NodeUpdateEnabledState) * command.updatesCount);

	mCommandInterface->UpdateNodeEnabledState(updateList);
}

void SessionClient::ReceiveNodeHighlightUpdate(const ENetPacket* packet)
{
	avs::SetNodeHighlightedCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->SetNodeHighlighted(command.nodeID, command.isHighlighted);
}

void SessionClient::ReceiveNodeAnimationUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	avs::UpdateNodeAnimationCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimation(command.animationUpdate);
}

void SessionClient::ReceiveNodeAnimationControlUpdate(const ENetPacket* packet)
{
	//Extract command from packet.
	avs::SetAnimationControlCommand command;
	size_t commandSize = command.getCommandSize();
	memcpy(static_cast<void*>(&command), packet->data, commandSize);

	mCommandInterface->UpdateNodeAnimationControl(command.animationControlUpdate);
}

void SessionClient::ReceiveNodeAnimationSpeedUpdate(const ENetPacket* packet)
{
	avs::SetNodeAnimationSpeedCommand command;
	memcpy(static_cast<void*>(&command), packet->data, command.getCommandSize());

	mCommandInterface->SetNodeAnimationSpeed(command.nodeID, command.animationID, command.speed);
}

void SessionClient::ReceiveSetupLightingCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::SetupLightingCommand);
	//Copy command out of packet.
	memcpy(static_cast<void*>(&setupLightingCommand), packet->data, commandSize);

	std::vector<avs::uid> uidList((size_t)setupLightingCommand.num_gi_textures);
	memcpy(uidList.data(), packet->data + commandSize, sizeof(avs::uid) * uidList.size());
	mCommandInterface->OnLightingSetupChanged(setupLightingCommand);
}

void SessionClient::ReceiveSetupInputsCommand(const ENetPacket* packet)
{
	avs::SetupInputsCommand setupInputsCommand;
	memcpy(static_cast<void*>(&setupInputsCommand), packet->data, sizeof(avs::SetupInputsCommand));

	inputDefinitions.resize(setupInputsCommand.numInputs);
	unsigned char* ptr = packet->data + sizeof(avs::SetupInputsCommand);
	for (int i = 0; i < setupInputsCommand.numInputs; i++)
	{
		if (size_t(ptr -packet->data) >= packet->dataLength)
		{
			TELEPORT_CERR << "Bad packet" << std::endl;
			return;
		}
		auto& def = inputDefinitions[i];
		avs::InputDefinitionNetPacket& packetDef = *((avs::InputDefinitionNetPacket*)ptr);
		ptr += sizeof(avs::InputDefinitionNetPacket);
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
	size_t commandSize = sizeof(avs::UpdateNodeStructureCommand);
	//Copy command out of packet.
	avs::UpdateNodeStructureCommand updateNodeStructureCommand;
	memcpy(static_cast<void*>(&updateNodeStructureCommand), packet->data, commandSize);
	mCommandInterface->UpdateNodeStructure(updateNodeStructureCommand);
}

void SessionClient::ReceiveUpdateNodeSubtypeCommand(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::UpdateNodeSubtypeCommand);
	if(packet->dataLength<commandSize)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	//Copy command out of packet.
	avs::UpdateNodeSubtypeCommand updateNodeSubtypeCommand;
	memcpy(static_cast<void*>(&updateNodeSubtypeCommand), packet->data, commandSize);
	if(packet->dataLength!=commandSize+updateNodeSubtypeCommand.pathLength)
	{
		TELEPORT_CERR << "Bad packet." << std::endl;
		return;
	}
	std::string str;
	str.resize(updateNodeSubtypeCommand.pathLength);
	memcpy(static_cast<void*>(str.data()), packet->data+commandSize,updateNodeSubtypeCommand.pathLength);
	mCommandInterface->UpdateNodeSubtype(updateNodeSubtypeCommand,str);
}


void SessionClient::SetDiscoveryClientID(uint64_t clientID)
{
	discoveryService->SetClientID(clientID);
}
