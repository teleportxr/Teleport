// (C) Copyright 2018 Simul.co

#include <VrApi_Types.h>
#include <Kernel/OVR_LogUtils.h>
#include <OVR_Input.h>
#include <VrApi_Input.h>

#include <string>
#include <time.h>
#include <sys/socket.h>

#include "SessionClient.h"

enum RemotePlaySessionChannel {
    RPCH_Control  = 0,
    RPCH_HeadPose = 1,
    RPCH_NumChannels,
};

struct RemotePlayInputState {
    uint32_t buttonsPressed;
    uint32_t buttonsReleased;
    float trackpadAxisX;
    float trackpadAxisY;
};

struct ServiceDiscoveryResponse {
    uint32_t clientID;
    uint16_t remotePort;
} __attribute__((packed));

SessionClient::SessionClient(SessionCommandInterface* commandInterface)
    : mCommandInterface(commandInterface)
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
    if(mServiceDiscoverySocket) {
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
        OVR_LOG("Discovered session server: %s:%d", remoteIP, remote.port);

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
    if(enet_host_service(mClientHost, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
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
    if(mClientHost && mServerPeer) {
        if (timeout == 0) {
            enet_peer_disconnect_now(mServerPeer, 0);
        } else {
            enet_peer_disconnect(mServerPeer, 0);

            bool isPeerConnected = true;
            ENetEvent event;
            while (isPeerConnected && enet_host_service(mClientHost, &event, timeout) > 0) {
                switch (event.type) {
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

            if (isPeerConnected) {
                enet_peer_reset(mServerPeer);
            }
        }
        mServerPeer = nullptr;
        mServerEndpoint = {};
    }

    if(mClientHost) {
        enet_host_destroy(mClientHost);
        mClientHost = nullptr;
    }
}

void SessionClient::Frame(const OVR::ovrFrameInput& vrFrame, const ControllerState& controllerState)
{
    if(mClientHost && mServerPeer) {

        SendHeadPose(vrFrame.Tracking.HeadPose);
        SendInput(controllerState);

        ENetEvent event;
        while(enet_host_service(mClientHost, &event, 0) > 0) {
            switch(event.type) {
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
    if(IsConnected()) {
        char remoteIP[20];
        enet_address_get_host_ip(&mServerEndpoint, remoteIP, sizeof(remoteIP));
        return std::string(remoteIP);
    }
    else {
        return std::string{};
    }
}

void SessionClient::DispatchEvent(const ENetEvent& event)
{
    switch(event.channelID) {
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
    // TODO: Sanitize!
    const std::string command(reinterpret_cast<const char*>(packet->data), packet->dataLength);
    OVR_WARN("CMD: %s", command.c_str());
    if(command[0] == 'v') {
        int port, width, height;
        sscanf(command.c_str(), "v %d %d %d", &port, &width, &height);
        if(width == 0 && height == 0) {
            mCommandInterface->OnVideoStreamClosed();
        }
        else {
            mCommandInterface->OnVideoStreamChanged(port, width, height);
        }
    }
    else {
        OVR_WARN("Invalid command: %c", command[0]);
    }
}

void SessionClient::SendHeadPose(const ovrRigidBodyPosef& pose)
{
    // TODO: Use compact representation with only 3 float values for wire format.
    const ovrQuatf orientation = pose.Pose.Orientation;
    ENetPacket* packet = enet_packet_create(&orientation, sizeof(orientation), 0);
    enet_peer_send(mServerPeer, RPCH_HeadPose, packet);
}

void SessionClient::SendInput(const ControllerState& controllerState)
{
    RemotePlayInputState inputState = {};

    const uint32_t buttonsDiffMask = mPrevControllerState.mButtons ^ controllerState.mButtons;
    auto updateButtonState = [&inputState, &controllerState, buttonsDiffMask](uint32_t button)
    {
        if(buttonsDiffMask & button) {
            if(controllerState.mButtons & button) inputState.buttonsPressed |= button;
            else inputState.buttonsReleased |= button;
        }
    };

    // We need to update trackpad axis on the server whenever:
    // (1) User is currently touching the trackpad.
    // (2) User was touching the trackpad previous frame.
    bool updateTrackpadAxis =    controllerState.mTrackpadStatus
                              || controllerState.mTrackpadStatus != mPrevControllerState.mTrackpadStatus;

    bool stateDirty =  updateTrackpadAxis || buttonsDiffMask > 0;
    if(stateDirty) {
        enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;

        updateButtonState(ovrButton_A);
        updateButtonState(ovrButton_Enter); // FIXME: Currently not getting down event for this button.
        updateButtonState(ovrButton_Back);

        // Trackpad axis should be non-zero only if the user is currently touching the trackpad.
        if(controllerState.mTrackpadStatus) {
            // Remap axis value to [-1,1] range.
            inputState.trackpadAxisX = 2.0f * controllerState.mTrackpadX - 1.0f;
            inputState.trackpadAxisY = 2.0f * controllerState.mTrackpadY - 1.0f;

            // If this update does not include button information send it unreliably to improve latency.
            if(buttonsDiffMask == 0) {
                packetFlags = ENET_PACKET_FLAG_UNSEQUENCED;
            }
        }

        ENetPacket* packet = enet_packet_create(&inputState, sizeof(inputState), packetFlags);
        enet_peer_send(mServerPeer, RPCH_Control, packet);
    }
}
