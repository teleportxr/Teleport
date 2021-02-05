// (C) Copyright 2018-2019 Simul Software Ltd
#include "SessionClient.h"

#include "libavstream/common.hpp"

#include "ResourceCreator.h"
#include "Log.h"
#include <limits>

SessionClient::SessionClient(SessionCommandInterface* commandInterface, std::unique_ptr<DiscoveryService>&& discoveryService)
	: mCommandInterface(commandInterface), discoveryService(std::move(discoveryService))
{}

SessionClient::~SessionClient()
{
	//Disconnect(0); causes crash. trying to access deleted objects.
}

void SessionClient::SetResourceCreator(ResourceCreator *r)
{
	mResourceCreator=r;
}

uint32_t SessionClient::Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote)
{
	uint32_t cl_id=discoveryService->Discover(clientIP, clientDiscoveryPort, serverIP, serverDiscoveryPort, remote);
	if(cl_id!=0)
	{
		clientID=cl_id;
		discovered=true;
	}
	return cl_id;
}

bool SessionClient::Connect(const char* remoteIP, uint16_t remotePort, uint timeout)
{
	ENetAddress remote;
	enet_address_set_host_ip(&remote, remoteIP);
	remote.port = remotePort;

	return Connect(remote, timeout);
}

bool SessionClient::Connect(const ENetAddress& remote, uint timeout)
{
	mClientHost = enet_host_create(nullptr, 1, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0, 0);
	if(!mClientHost)
	{
		FAIL("Failed to create ENET client host");
		return false;
	}

	mServerPeer = enet_host_connect(mClientHost, &remote, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_NumChannels), 0);
	if(!mServerPeer)
	{
		WARN("Failed to initiate connection to the server");
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
		return false;
	}

	ENetEvent event;
	if(enet_host_service(mClientHost, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
	{
		mServerEndpoint = remote;

		char remoteIP[20];
		enet_address_get_host_ip(&mServerEndpoint, remoteIP, sizeof(remoteIP));
		LOG("Connected to session server: %s:%d", remoteIP, remote.port);
		return true;
	}

	WARN("Failed to connect to remote session server");

	enet_host_destroy(mClientHost);
	mClientHost = nullptr;
	mServerPeer = nullptr;
	return false;
}

void SessionClient::Disconnect(uint timeout)
{
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
	// TODO: retain client id for reconnection.
	clientID=0;
	discovered=false;
}

void SessionClient::SetPeerTimeout(uint timeout)
{
	if (IsConnected())
	{
		enet_peer_timeout(mServerPeer, 0, timeout, timeout * 6);
	}
}

void SessionClient::SendClientMessage(const avs::ClientMessage& msg)
{
	size_t sz = avs::GetClientMessageSize(msg.clientMessagePayloadType);
	ENetPacket* packet = enet_packet_create(&msg, sz, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
}

void SessionClient::Frame(const avs::DisplayInfo &displayInfo
	,const avs::Pose &headPose
	,const avs::Pose* controllerPoses
	,uint64_t poseValidCounter
	,const avs::Pose &originPose
	,const ControllerState* controllerStates
	,bool requestKeyframe
	,double t)
{
	time=t;
	if(mClientHost && mServerPeer)
	{
		if(handshakeAcknowledged)
		{
			SendDisplayInfo(displayInfo);
			if(poseValidCounter)
			{
				SendHeadPose(headPose);
				SendControllerPoses(headPose,controllerPoses);
				sendOriginPose(poseValidCounter,originPose);
			}
			SendInput(0,controllerStates[0]);
			SendInput(1,controllerStates[1]);
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
					return;
				case ENET_EVENT_TYPE_RECEIVE:
					DispatchEvent(event);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					Disconnect(0);
					return;
			}
		}
	}
	mPrevControllerState = controllerStates[0];
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
	if(IsConnected())
	{
		char remoteIP[20];
		enet_address_get_host_ip(&mServerEndpoint, remoteIP, sizeof(remoteIP));
		return std::string(remoteIP);
	}
	else
	{
		return std::string{};
	}
}

avs::vec3 SessionClient::GetOriginPos() const
{
	return originPos;
}

avs::vec3 SessionClient::GetOriginToHeadOffset() const
{
	return originToHeadPos;
}

void SessionClient::DispatchEvent(const ENetEvent& event)
{
	switch(event.channelID)
	{
		case static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control) :
			ParseCommandPacket(event.packet);
			break;
		default:
			WARN("Received packet on output-only channel: %d", event.channelID);
			break;
	}

	enet_packet_destroy(event.packet);
}

void SessionClient::ParseCommandPacket(ENetPacket* packet)
{
	avs::CommandPayloadType commandPayloadType = *((avs::CommandPayloadType*)packet->data);
	switch(commandPayloadType)
	{
		case avs::CommandPayloadType::Shutdown:
			mCommandInterface->OnVideoStreamClosed();
			break;
		case avs::CommandPayloadType::AcknowledgeHandshake:
			ReceiveHandshakeAcknowledgement(packet);
			break;
		case avs::CommandPayloadType::Setup:
		{
			size_t commandSize = sizeof(avs::SetupCommand);

			//Copy command out of packet.
			avs::SetupCommand setupCommand;
			memcpy(&setupCommand, packet->data, commandSize);

			avs::Handshake handshake;
			char server_ip[100];
			enet_address_get_host_ip(&mServerEndpoint, server_ip, 99);
			mCommandInterface->OnVideoStreamChanged(server_ip, setupCommand, handshake);
			
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
		break;
		case avs::CommandPayloadType::ReconfigureVideo:
		{
			size_t commandSize = sizeof(avs::ReconfigureVideoCommand);

			//Copy command out of packet.
			avs::ReconfigureVideoCommand reconfigureCommand;
			memcpy(&reconfigureCommand, packet->data, commandSize);

			mCommandInterface->OnReconfigureVideo(reconfigureCommand);
		}
		case avs::CommandPayloadType::SetPosition:
		{
			size_t commandSize = sizeof(avs::SetPositionCommand);
			avs::SetPositionCommand command;
			memcpy(&command, packet->data, commandSize);
			receivedInitialPos = (receivedInitialPos + 1) % ULLONG_MAX;
			originPos=command.origin_pos;
			if(command.set_relative_pos)
				receivedRelativePos = (receivedRelativePos + 1) % ULLONG_MAX;
			originToHeadPos=command.relative_pos;
		}
		break;
		case avs::CommandPayloadType::NodeBounds:
		{
			size_t commandSize = sizeof(avs::NodeBoundsCommand);

			avs::NodeBoundsCommand command;
			memcpy(&command, packet->data, commandSize);

			size_t enteredSize = sizeof(avs::uid) * command.nodesShowAmount;
			size_t leftSize = sizeof(avs::uid) * command.nodesHideAmount;

			std::vector<avs::uid> enteredNodes(command.nodesShowAmount);
			std::vector<avs::uid> leftNodes(command.nodesHideAmount);

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
			mResourceRequests.insert(mResourceRequests.end(), missingNodes.begin(), missingNodes.end());

			//Tell renderer to hide nodes that have left bounds.
			for(avs::uid node_uid : leftNodes)
			{
				if(mCommandInterface->OnNodeLeftBounds(node_uid))
				{
					mLostNodes.push_back(node_uid);
				}
			}
		}

			break;
		case avs::CommandPayloadType::UpdateNodeMovement:
			ReceiveNodeMovementUpdate(packet);
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

void SessionClient::SendHeadPose(const avs::Pose& pose)
{
	ENetPacket* packet = enet_packet_create(&pose, sizeof(avs::Pose), 0);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_HeadPose), packet);
}

void SessionClient::SendControllerPoses(const avs::Pose& headPose,const avs::Pose * poses)
{
	if(!poses)
		return;
	avs::ControllerPosesMessage message;
	message.headPose=headPose;
	message.controllerPoses[0]=poses[0];
	message.controllerPoses[1]=poses[1];
	SendClientMessage(message);
}

void SessionClient::sendOriginPose(uint64_t validCounter,const avs::Pose& originPose)
{
	avs::OriginPoseMessage message;
	message.counter=validCounter;
	message.originPose=originPose;
	SendClientMessage(message);
}

void SessionClient::SendInput(int id,const ControllerState& controllerState)
{
	avs::InputState inputState = {};
	inputState.buttonsDown= controllerState.mButtons;
	const uint32_t buttonsDiffMask = mPrevControllerState.mButtons ^ controllerState.mButtons;
	inputState.controllerId=id;
	inputState.joystickAxisX = controllerState.mJoystickAxisX;
	inputState.joystickAxisY = controllerState.mJoystickAxisY;
	// We need to update trackpad axis on the server whenever:
	// (1) User is currently touching the trackpad.
	// (2) User was touching the trackpad previous frame.
	//bool updateTrackpadAxis =    controllerState.mTrackpadStatus || controllerState.mTrackpadStatus != mPrevControllerState.mTrackpadStatus;

	//bool stateDirty =  updateTrackpadAxis || buttonsDiffMask > 0;
	// If there's a joystick, we must send an update every frame.
	//if(stateDirty)
	{
		enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;

		/*updateButtonState(ovrButton_A);
		updateButtonState(ovrButton_Enter); // FIXME: Currently not getting down event for this button.
		updateButtonState(ovrButton_Back);*/

		// Trackpad axis should be non-zero only if the user is currently touching the trackpad.
		if(controllerState.mTrackpadStatus)
		{
			// Remap axis value to [-1,1] range.
			inputState.trackpadAxisX = 2.0f * controllerState.mTrackpadX - 1.0f;
			inputState.trackpadAxisY = 2.0f * controllerState.mTrackpadY - 1.0f;

			// If this update does not include button information send it unreliably to improve latency.
			if(buttonsDiffMask == 0)
			{
				packetFlags = ENET_PACKET_FLAG_UNSEQUENCED;
			}
		}
		inputState.numEvents=controllerState.inputEvents.size();
	
		inputBuffer.resize(sizeof(avs::InputState)+inputState.numEvents*sizeof(avs::InputEvent));
		memcpy(inputBuffer.data(),&inputState,sizeof(avs::InputState));
		memcpy(inputBuffer.data()+sizeof(avs::InputState),controllerState.inputEvents.data(),inputState.numEvents*sizeof(avs::InputEvent));
		size_t dataLength=inputBuffer.size();
		if(dataLength!=24)
		{
			ENetPacket* packet = enet_packet_create(inputBuffer.data(), dataLength, packetFlags);
			enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);
		}
		else
		{
			ENetPacket* packet = enet_packet_create(inputBuffer.data(), dataLength, packetFlags);
			enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);
		}
	}
}

void SessionClient::SendResourceRequests()
{
	std::vector<avs::uid> resourceRequests = mResourceCreator->TakeResourceRequests();

	//Append to session client's resource requests.
	mResourceRequests.insert(mResourceRequests.end(), resourceRequests.begin(), resourceRequests.end());
	resourceRequests.clear();

	if(mResourceRequests.size() != 0)
	{
		size_t resourceAmount = mResourceRequests.size();
		ENetPacket* packet = enet_packet_create(&resourceAmount, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);

		enet_packet_resize(packet, sizeof(size_t) + sizeof(avs::uid) * resourceAmount);
		memcpy(packet->data + sizeof(size_t), mResourceRequests.data(), sizeof(avs::uid) * resourceAmount);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ResourceRequest), packet);
		for(auto i:mResourceRequests)
		{
			mSentResourceRequests[i]=time;
		}
		mResourceRequests.clear();
	}
	// Have we been waiting too long for any resources?
	for(auto r:mSentResourceRequests)
	{
		if(time-r.second>10.0)
		{
			SCR_COUT<<"Re-requesting resource "<<r.first<<std::endl;
			mResourceRequests.push_back(r.first);
		}
	}
}

void SessionClient::SendReceivedResources()
{
	std::vector<avs::uid> receivedResources = mResourceCreator->TakeReceivedResources();

	if(receivedResources.size() != 0)
	{
		avs::ReceivedResourcesMessage message(receivedResources.size());

		size_t messageSize = sizeof(avs::ReceivedResourcesMessage);
		size_t receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedResourcesSize);
		memcpy(packet->data + messageSize, receivedResources.data(), receivedResourcesSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);

		for(auto r:receivedResources)
		{
			auto q=mSentResourceRequests.find(r);
			if(q!=mSentResourceRequests.end())
				mSentResourceRequests.erase(r);
		}
	}
}

void SessionClient::SendNodeUpdates()
{
	//Insert completed nodes.
	{
		std::vector<avs::uid> completedNodes = mResourceCreator->TakeCompletedNodes();
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
	memcpy(&command, packet->data, commandSize);

	//Extract list of visible nodes.
	std::vector<avs::uid> visibleNodes(command.visibleNodeAmount);
	memcpy(visibleNodes.data(), packet->data + commandSize, sizeof(avs::uid) * command.visibleNodeAmount);

	mCommandInterface->SetVisibleNodes(visibleNodes);

	handshakeAcknowledged = true;
}

void SessionClient::ReceiveNodeMovementUpdate(const ENetPacket* packet)
{
	size_t commandSize = sizeof(avs::UpdateNodeMovementCommand);

	//Extract command from packet.
	avs::UpdateNodeMovementCommand command;
	memcpy(&command, packet->data, commandSize);

	std::vector<avs::MovementUpdate> updateList(command.updatesAmount);
	memcpy(updateList.data(), packet->data + commandSize, sizeof(avs::MovementUpdate) * command.updatesAmount);

	mCommandInterface->UpdateNodeMovement(updateList);
}
