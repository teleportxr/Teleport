// (C) Copyright 2018-2019 Simul Software Ltd

#include <iostream>
#include <VrApi_Types.h>
#include <Kernel/OVR_LogUtils.h>
#include <OVR_Input.h>
#include <VrApi_Input.h>

#include <string>
#include <time.h>
#include <sys/socket.h>
#include <VrApi_Helpers.h>

#include "SessionClient.h"

#include "ResourceCreator.h"

ovrQuatf QuaternionMultiply(const ovrQuatf &p,const ovrQuatf &q)
{
    ovrQuatf r;
    r.w= p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
    r.x= p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
    r.y= p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
    r.z= p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
    return r;
}

static inline ovrQuatf RelativeQuaternion(const ovrQuatf &p,const ovrQuatf &q)
{
    ovrQuatf iq=q;
    iq.x*=-1.f;
    iq.y*=-1.f;
    iq.z*=-1.f;
    return QuaternionMultiply(p,iq);
}

enum RemotePlaySessionChannel
{
    RPCH_HANDSHAKE = 0,
    RPCH_Control = 1,
    RPCH_HeadPose = 2,
    RPCH_Resource_Request = 3,
    RPCH_Keyframe_Request = 4,
	RPCH_ClientMessage=5,
    RPCH_NumChannels,
};

struct RemotePlayInputState{
    uint32_t buttonsPressed;
    uint32_t buttonsReleased;
    float trackpadAxisX;
    float trackpadAxisY;
    float joystickAxisX;
    float joystickAxisY;
};

struct ServiceDiscoveryResponse {
    uint32_t clientID;
    uint16_t remotePort;
} __attribute__((packed));

SessionClient::SessionClient(SessionCommandInterface* commandInterface, ResourceCreator& resourceCreator)
    : mCommandInterface(commandInterface), mResourceCreator(resourceCreator)
{
    struct timespec timeNow;
    clock_gettime(CLOCK_REALTIME, &timeNow);

    // Generate random client ID
    const unsigned int timeNowMs = static_cast<unsigned int>(timeNow.tv_sec * 1000 + timeNow.tv_nsec / 1000000);
    srand(timeNowMs);
    mClientID = static_cast<uint32_t>(rand());
}

SessionClient::~SessionClient()
{
    Disconnect(0);
    if(mServiceDiscoverySocket)
    {
        close(mServiceDiscoverySocket);
    }
}

bool SessionClient::Discover(uint16_t discoveryPort, ENetAddress& remote)
{
    bool serverDiscovered = false;

    struct sockaddr_in broadcastAddress = { AF_INET, htons(discoveryPort) };
    broadcastAddress.sin_addr.s_addr = INADDR_BROADCAST;

    if(!mServiceDiscoverySocket) {
        mServiceDiscoverySocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(mServiceDiscoverySocket <= 0) {
            OVR_FAIL("Failed to create service discovery UDP socket");
            return false;
        }

        int flagEnable = 1;
        setsockopt(mServiceDiscoverySocket, SOL_SOCKET, SO_REUSEADDR, &flagEnable, sizeof(int));
        setsockopt(mServiceDiscoverySocket, SOL_SOCKET, SO_BROADCAST, &flagEnable, sizeof(int));

        struct sockaddr_in bindAddress = { AF_INET, htons(discoveryPort) };
        if(bind(mServiceDiscoverySocket, (struct sockaddr*)&bindAddress, sizeof(bindAddress)) == -1) {
            OVR_FAIL("Failed to bind to service discovery UDP socket");
            close(mServiceDiscoverySocket);
            mServiceDiscoverySocket = 0;
            return false;
        }
    }

    sendto(mServiceDiscoverySocket, &mClientID, sizeof(mClientID), 0,
           (struct sockaddr*)&broadcastAddress, sizeof(broadcastAddress));

    {
        ServiceDiscoveryResponse response = {};
        struct sockaddr_in responseAddr;
        socklen_t responseAddrSize = sizeof(responseAddr);

        ssize_t bytesRecv;
        do {
            bytesRecv = recvfrom(mServiceDiscoverySocket, &response, sizeof(response),
                                 MSG_DONTWAIT,
                                 (struct sockaddr*)&responseAddr, &responseAddrSize);

            if(bytesRecv == sizeof(response) && mClientID == response.clientID) {
                remote.host = responseAddr.sin_addr.s_addr;
                remote.port = response.remotePort;
                serverDiscovered = true;
            }
        } while(bytesRecv > 0 && !serverDiscovered);
    }

    if(serverDiscovered) {
        char remoteIP[20];
        enet_address_get_host_ip(&remote, remoteIP, sizeof(remoteIP));
        OVR_WARN("Discovered session server: %s:%d", remoteIP, remote.port);

        close(mServiceDiscoverySocket);
        mServiceDiscoverySocket = 0;
    }
    return serverDiscovered;
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
    mClientHost = enet_host_create(nullptr, 1, RPCH_NumChannels, 0, 0);
    if(!mClientHost) {
        OVR_FAIL("Failed to create ENET client host");
        return false;
    }

    mServerPeer = enet_host_connect(mClientHost, &remote, RPCH_NumChannels, 0);
    if(!mServerPeer) {
        OVR_WARN("Failed to initiate connection to the server");
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
        OVR_LOG("Connected to session server: %s:%d", remoteIP, remote.port);
        return true;
    }

    OVR_WARN("Failed to connect to remote session server");

    enet_host_destroy(mClientHost);
    mClientHost = nullptr;
    mServerPeer = nullptr;
    return false;
}

void SessionClient::Disconnect(uint timeout)
{
    if(mClientHost && mServerPeer)
    {
        if (timeout == 0)
        {
            enet_peer_disconnect_now(mServerPeer, 0);
        }
        else
        {
            enet_peer_disconnect(mServerPeer, 0);

            bool isPeerConnected = true;
            ENetEvent event;
            while (isPeerConnected && enet_host_service(mClientHost, &event, timeout) > 0)
            {
                switch (event.type)
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

            if (isPeerConnected)
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
}

void SessionClient::SendClientMessage(const avs::ClientMessage &msg)
{
	size_t sz = avs::GetClientMessageSize(msg.clientMessagePayloadType);
	ENetPacket* packet = enet_packet_create(&msg, sz,ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(mServerPeer, RPCH_ClientMessage, packet);
}

void SessionClient::Frame(const HeadPose& headPose, bool poseValid,const ControllerState& controllerState, bool requestKeyframe)
{
    if(mClientHost && mServerPeer)
    {
    	if(poseValid)
	        SendHeadPose(headPose);
        SendInput(controllerState);
        SendResourceRequests();
		SendReceivedResources();
        SendActorUpdates();
        if (requestKeyframe)
            SendKeyframeRequest();

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

void SessionClient::DispatchEvent(const ENetEvent& event)
{
    switch(event.channelID)
    {
        case RPCH_Control:
            ParseCommandPacket(event.packet);
            break;
        default:
            OVR_WARN("Received packet on output-only channel: %d", event.channelID);
            break;
    }

    enet_packet_destroy(event.packet);
}

void SessionClient::ParseCommandPacket(ENetPacket* packet)
{
	avs::CommandPayloadType commandPayloadType = *((avs::CommandPayloadType*)packet->data);
	size_t cmdSize = avs::GetCommandSize(commandPayloadType);
	switch (commandPayloadType)
    {
		case avs::CommandPayloadType::Text:
		{
			const char *txt_utf8 = (const char *)(packet->data + cmdSize);
			assert(txt_utf8[packet->dataLength - cmdSize - 1] == (char)0);
			ParseTextCommand(txt_utf8);
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
            handshake.isReadyToReceivePayloads=true;
            handshake.axesStandard = avs::AxesStandard::GlStyle;
            handshake.MetresPerUnit = 1.0f;
            handshake.usingHands = true;

            std::vector<avs::uid> outActors;
            mCommandInterface->OnVideoStreamChanged(setupCommand, handshake, setupCommand.server_id != lastServer_id, resourcesClientNeeds, outActors);
            //Add the unfound resources to the resource request list.
			mResourceRequests.insert(mResourceRequests.end(), resourcesClientNeeds.begin(), resourcesClientNeeds.end());

            //Confirm the actors the client already has.
            mReceivedActors.insert(mReceivedActors.end(), outActors.begin(), outActors.end());

            lastServer_id = setupCommand.server_id;
            SendHandshake(handshake);
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
                mCommandInterface->OnActorLeftBounds(actor_uid);
            }
            mLostActors.insert(mLostActors.end(), leftActors.begin(), leftActors.end());
        }

            break;
		default:
			break;
	};
}

void SessionClient::ParseTextCommand(const char *txt_utf8)
{
	OVR_WARN("CMD: %s", txt_utf8);
	if (txt_utf8[0] == 'v')
    {
        int port, width, height;
        sscanf(txt_utf8, "v %d %d %d", &port, &width, &height);
        if(width == 0 && height == 0)
		{
            mCommandInterface->OnVideoStreamClosed();
        }
    }
    else
    {
		OVR_WARN("Invalid text command: %c", txt_utf8[0]);
    }
}

void SessionClient::SendHeadPose(const HeadPose& pose)
{
    if(!handshakeAcknowledged) return;

    // TODO: Use compact representation with only 3 float values for wire format.
    const ovrQuatf HeadPoseOVR = *((const ovrQuatf*)&pose.orientation);
    HeadPose headPose2;
    ovrQuatf RootPose = { 0.0f, 0.0f, 0.0f, 1.0f };
    ovrQuatf RelPose = RelativeQuaternion(HeadPoseOVR,RootPose);
    // Convert from Oculus coordinate system (x back, y up, z left) to Simulcaster (x right, y forward, z up).
    headPose2.orientation = scr::vec4(RelPose.x, RelPose.y,RelPose.z, RelPose.w);
    headPose2.position=*((scr::vec3*)&pose.position);
    ENetPacket* packet = enet_packet_create(&headPose2, sizeof(HeadPose), 0);
    enet_peer_send(mServerPeer, RPCH_HeadPose, packet);
}

void SessionClient::SendInput(const ControllerState& controllerState)
{
    if(!handshakeAcknowledged) return;

    RemotePlayInputState inputState = {};

    const uint32_t buttonsDiffMask = mPrevControllerState.mButtons ^ controllerState.mButtons;
    auto updateButtonState = [&inputState, &controllerState, buttonsDiffMask](uint32_t button)
    {
        if(buttonsDiffMask & button)
        {
            if(controllerState.mButtons & button)
                inputState.buttonsPressed |= button;
            else
                inputState.buttonsReleased |= button;
        }
    };
    inputState.joystickAxisX=controllerState.mJoystickAxisX;
    inputState.joystickAxisY=controllerState.mJoystickAxisY;
    // We need to update trackpad axis on the server whenever:
    // (1) User is currently touching the trackpad.
    // (2) User was touching the trackpad previous frame.
    //bool updateTrackpadAxis =    controllerState.mTrackpadStatus || controllerState.mTrackpadStatus != mPrevControllerState.mTrackpadStatus;

    //bool stateDirty =  updateTrackpadAxis || buttonsDiffMask > 0;
    // If there's a joystick, we must send an update every frame.
    //if(stateDirty)
    {
        enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;

        updateButtonState(ovrButton_A);
        updateButtonState(ovrButton_Enter); // FIXME: Currently not getting down event for this button.
        updateButtonState(ovrButton_Back);

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
        enet_peer_send(mServerPeer, RPCH_Control, packet);
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

        enet_peer_send(mServerPeer, RPCH_Resource_Request, packet);
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

		enet_peer_send(mServerPeer, RPCH_ClientMessage, packet);
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

		enet_peer_send(mServerPeer, RPCH_ClientMessage, packet);

		mReceivedActors.clear();
		mLostActors.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
    ENetPacket* packet = enet_packet_create(0x0, sizeof(size_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(mServerPeer, RPCH_Keyframe_Request, packet);
}

void SessionClient::SendHandshake(const avs::Handshake &handshake)
{
    handshakeAcknowledged = false;
    ENetPacket *packet = enet_packet_create(&handshake, sizeof(avs::Handshake), 0);
    enet_peer_send(mServerPeer, RPCH_HANDSHAKE, packet);
}
