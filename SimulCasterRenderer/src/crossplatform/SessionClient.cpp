// (C) Copyright 2018-2019 Simul Software Ltd
#include "SessionClient.h"

#include "libavstream/common.hpp"

#include "ResourceCreator.h"
#include "Log.h"

SessionClient::SessionClient(SessionCommandInterface* commandInterface, std::unique_ptr<DiscoveryService>&& discoveryService, ResourceCreator& resourceCreator)
	: mCommandInterface(commandInterface), discoveryService(std::move(discoveryService)), mResourceCreator(resourceCreator)
{}

SessionClient::~SessionClient()
{
	Disconnect(0);
}

bool SessionClient::Discover(uint16_t discoveryPort, ENetAddress& remote)
{
	return discoveryService->Discover(discoveryPort, remote);
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
}

void SessionClient::SendClientMessage(const avs::ClientMessage& msg)
{
	size_t sz = avs::GetClientMessageSize(msg.clientMessagePayloadType);
	ENetPacket* packet = enet_packet_create(&msg, sz, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
}

void SessionClient::Frame(const avs::DisplayInfo &displayInfo, const avs::HeadPose &headPose, const avs::HeadPose* controllerPoses,bool poseValid,const ControllerState& controllerState, bool requestKeyframe)
{
	if(mClientHost && mServerPeer)
	{
		if(handshakeAcknowledged)
		{
			SendDisplayInfo(displayInfo);
			if(poseValid)
			{
				SendHeadPose(headPose);
				SendControllerPoses(headPose,controllerPoses);
			}
			SendInput(controllerState);
			SendResourceRequests();
			SendReceivedResources();
			SendActorUpdates();
			if(requestKeyframe) SendKeyframeRequest();
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
	mPrevControllerState = controllerState;
}

bool SessionClient::IsConnected() const
{
	return mServerPeer != nullptr;
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

avs::vec3 SessionClient::GetInitialPos() const
{
	return initialPos;
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
		{
			mCommandInterface->OnVideoStreamClosed();
		}
		break;
		case avs::CommandPayloadType::AcknowledgeHandshake:
			handshakeAcknowledged = true;
			break;
		case avs::CommandPayloadType::Setup:
		{
			size_t commandSize = sizeof(avs::SetupCommand);

			//Copy command out of packet.
			avs::SetupCommand setupCommand;
			memcpy(&setupCommand, packet->data, commandSize);

			//Copy resources the client will need from the packet.
			size_t resourceListSize = sizeof(avs::uid) * setupCommand.resourceCount;
			std::vector<avs::uid> resourcesClientNeeds(setupCommand.resourceCount);
			if(resourceListSize)
			{
				memcpy(resourcesClientNeeds.data(), packet->data + commandSize, resourceListSize);
			}

			avs::Handshake handshake;
			std::vector<avs::uid> outActors;
			mCommandInterface->OnVideoStreamChanged(setupCommand, handshake, setupCommand.server_id != lastServerID, resourcesClientNeeds, outActors);
			//Add the unfound resources to the resource request list.
			mResourceRequests.insert(mResourceRequests.end(), resourcesClientNeeds.begin(), resourcesClientNeeds.end());

			//Confirm the actors the client already has.
			mReceivedActors.insert(mReceivedActors.end(), outActors.begin(), outActors.end());

			lastServerID = setupCommand.server_id;
			SendHandshake(handshake);
		}
		break;
		case avs::CommandPayloadType::SetPosition:
		{
			size_t commandSize = sizeof(avs::SetPositionCommand);
			avs::SetPositionCommand command;
			memcpy(&command, packet->data, commandSize);
			receivedInitialPos=true;
			initialPos=command.position;
		}
		break;
		case avs::CommandPayloadType::ActorBounds:
		{
			size_t commandSize = sizeof(avs::ActorBoundsCommand);

			avs::ActorBoundsCommand command;
			memcpy(&command, packet->data, commandSize);

			size_t enteredSize = sizeof(avs::uid) * command.actorsShowAmount;
			size_t leftSize = sizeof(avs::uid) * command.actorsHideAmount;

			std::vector<avs::uid> enteredActors(command.actorsShowAmount);
			std::vector<avs::uid> leftActors(command.actorsHideAmount);

			memcpy(enteredActors.data(), packet->data + commandSize, enteredSize);
			memcpy(leftActors.data(), packet->data + commandSize + enteredSize, leftSize);

			std::vector<avs::uid> missingActors;
			//Tell the renderer to show the actors that have entered the streamable bounds; create resend requests for actors it does not have the data on, and confirm actors it does have the data for.
			for(avs::uid actor_uid : enteredActors)
			{
				if(!mCommandInterface->OnActorEnteredBounds(actor_uid))
				{
					missingActors.push_back(actor_uid);
				}
				else
				{
					mReceivedActors.push_back(actor_uid);
				}
			}
			mResourceRequests.insert(mResourceRequests.end(), missingActors.begin(), missingActors.end());

			//Tell renderer to hide actors that have left bounds.
			for(avs::uid actor_uid : leftActors)
			{
				if(mCommandInterface->OnActorLeftBounds(actor_uid))
				{
					mLostActors.push_back(actor_uid);
				}
			}
		}

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

void SessionClient::SendHeadPose(const avs::HeadPose& pose)
{
	ENetPacket* packet = enet_packet_create(&pose, sizeof(avs::HeadPose), 0);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_HeadPose), packet);
}

void SessionClient::SendControllerPoses(const avs::HeadPose& headPose,const avs::HeadPose * poses)
{
	if(!poses)
		return;
	avs::ControllerPosesMessage message;
	message.headPose=headPose;
	message.controllerPoses[0]=poses[0];
	message.controllerPoses[1]=poses[1];
	SendClientMessage(message);
}


void SessionClient::SendInput(const ControllerState& controllerState)
{
	avs::InputState inputState = {};

	const uint32_t buttonsDiffMask = mPrevControllerState.mButtons ^ controllerState.mButtons;
	/*auto updateButtonState = [&inputState, &controllerState, buttonsDiffMask](uint32_t button)
	{
		if(buttonsDiffMask & button)
		{
			if(controllerState.mButtons & button)
				inputState.buttonsPressed |= button;
			else
				inputState.buttonsReleased |= button;
		}
	};*/
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
		ENetPacket* packet = enet_packet_create(&inputState, sizeof(inputState), packetFlags);
		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Control), packet);
	}
}

void SessionClient::SendResourceRequests()
{
	std::vector<avs::uid> resourceRequests = mResourceCreator.TakeResourceRequests();

	//Append session client's resource requests.
	resourceRequests.insert(resourceRequests.end(), mResourceRequests.begin(), mResourceRequests.end());
	mResourceRequests.clear();

	if(resourceRequests.size() != 0)
	{
		size_t resourceAmount = resourceRequests.size();
		ENetPacket* packet = enet_packet_create(&resourceAmount, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);

		enet_packet_resize(packet, sizeof(size_t) + sizeof(avs::uid) * resourceAmount);
		memcpy(packet->data + sizeof(size_t), resourceRequests.data(), sizeof(avs::uid) * resourceAmount);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ResourceRequest), packet);
	}
}

void SessionClient::SendReceivedResources()
{
	std::vector<avs::uid> receivedResources = mResourceCreator.TakeReceivedResources();

	if(receivedResources.size() != 0)
	{
		avs::ReceivedResourcesMessage message(receivedResources.size());

		size_t messageSize = sizeof(avs::ReceivedResourcesMessage);
		size_t receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedResourcesSize);
		memcpy(packet->data + messageSize, receivedResources.data(), receivedResourcesSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);
	}
}

void SessionClient::SendActorUpdates()
{
	//Insert completed actors.
	{
		std::vector<avs::uid> completedActors = mResourceCreator.TakeCompletedActors();
		mReceivedActors.insert(mReceivedActors.end(), completedActors.begin(), completedActors.end());
	}

	if(mReceivedActors.size() != 0 || mLostActors.size() != 0)
	{
		avs::ActorStatusMessage message(mReceivedActors.size(), mLostActors.size());

		size_t messageSize = sizeof(avs::ActorStatusMessage);
		size_t receivedSize = sizeof(avs::uid) * mReceivedActors.size();
		size_t lostSize = sizeof(avs::uid) * mLostActors.size();

		ENetPacket* packet = enet_packet_create(&message, messageSize, ENET_PACKET_FLAG_RELIABLE);
		enet_packet_resize(packet, messageSize + receivedSize + lostSize);
		memcpy(packet->data + messageSize, mReceivedActors.data(), receivedSize);
		memcpy(packet->data + messageSize + receivedSize, mLostActors.data(), lostSize);

		enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_ClientMessage), packet);

		mReceivedActors.clear();
		mLostActors.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
	ENetPacket* packet = enet_packet_create(0x0, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_KeyframeRequest), packet);
}

void SessionClient::SendHandshake(const avs::Handshake& handshake)
{
	ENetPacket* packet = enet_packet_create(&handshake, sizeof(avs::Handshake), 0);
	enet_peer_send(mServerPeer, static_cast<enet_uint8>(avs::RemotePlaySessionChannel::RPCH_Handshake), packet);
}