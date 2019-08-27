// (C) Copyright 2018 Simul

#include <string>
#include <iostream>
#include <random>

#include "SessionClient.h"

#include "libavstream/common.hpp"
#include <ws2tcpip.h>

#pragma comment(lib,"enet")
#pragma comment(lib,"libavstream.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#pragma comment(lib, "winmm.lib")

//Example: b1 == 192, b2 == 168, b3 == 0, b4 == 100
struct IPv4
{
	unsigned char b1, b2, b3, b4;
};

bool getMyIP(IPv4 & myIP)
{
	char szBuffer[1024];

	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 0);
	if (::WSAStartup(wVersionRequested, &wsaData) != 0)
		return false;

	if (gethostname(szBuffer, sizeof(szBuffer)) == SOCKET_ERROR)
	{
		WSACleanup();
		return false;
	}

	{
		struct addrinfo *result = NULL;
		struct addrinfo *ptr = NULL;
		struct addrinfo hints;
		struct sockaddr_in  *sockaddr_ipv4;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_family = AF_UNSPEC;
		char nodeName[1024];
		char serviceName[1024];
		getaddrinfo(szBuffer, nullptr, &hints, &result);
	}

	struct hostent *host = gethostbyname(szBuffer);
	if (host == NULL)
	{
		WSACleanup();
		return false;
	}

	//Obtain the computer's IP
	myIP.b1 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b1;
	myIP.b2 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b2;
	myIP.b3 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b3;
	myIP.b4 = ((struct in_addr *)(host->h_addr))->S_un.S_un_b.s_b4;

	WSACleanup();
	return true;
}

enum RemotePlaySessionChannel
{
	RPCH_HANDSHAKE = 0,
	RPCH_Control = 1,
	RPCH_HeadPose = 2,
	RPCH_NumChannels,
};

struct RemotePlayInputState {
	uint32_t buttonsPressed;
	uint32_t buttonsReleased;
	float trackpadAxisX;
	float trackpadAxisY;
};

#pragma pack(push, 1) 
struct ServiceDiscoveryResponse {
	uint32_t clientID;
	uint16_t remotePort;
};
#pragma pack(pop)

SessionClient::SessionClient(SessionCommandInterface* commandInterface)
	: mCommandInterface(commandInterface)
{
	if (enet_initialize() != 0)
	{
		FAIL("Failed to initialize ENET library");
	}
	std::random_device rd;  //Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
	std::uniform_int_distribution<> dis(1);

	mClientID = static_cast<uint32_t>(dis(gen));
}

SessionClient::~SessionClient()
{
	Disconnect(0);
	if (mServiceDiscoverySocket)
	{
		enet_socket_destroy(mServiceDiscoverySocket);
		mServiceDiscoverySocket = 0;
	}
}

bool SessionClient::Discover(uint16_t discoveryPort, ENetAddress& remote)
{
	bool serverDiscovered = false;

	ENetAddress  broadcastAddress = { ENET_HOST_BROADCAST, discoveryPort };

	if (!mServiceDiscoverySocket)
	{
		mServiceDiscoverySocket = enet_socket_create(ENetSocketType::ENET_SOCKET_TYPE_DATAGRAM);// PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (mServiceDiscoverySocket <= 0)
		{
			FAIL("Failed to create service discovery UDP socket");
			return false;
		}

		int flagEnable = 1;
		enet_socket_set_option(mServiceDiscoverySocket, ENET_SOCKOPT_REUSEADDR, 1);
		enet_socket_set_option(mServiceDiscoverySocket, ENET_SOCKOPT_BROADCAST, 1);
		// We don't want to block, just check for packets.
		enet_socket_set_option(mServiceDiscoverySocket, ENET_SOCKOPT_NONBLOCK, 1);

		// Here we BIND the socket to the local address that we want to be identified with.
		// e.g. our OWN local IP.
		ENetAddress bindAddress = { ENET_HOST_ANY, discoveryPort };
		enet_address_set_host(&(bindAddress), "127.0.0.1");
		if (enet_socket_bind(mServiceDiscoverySocket, &bindAddress) != 0)
		{
			FAIL("Failed to bind to service discovery UDP socket");
			enet_socket_destroy(mServiceDiscoverySocket);
			mServiceDiscoverySocket = 0;
			return false;
		}
	}
	ENetBuffer buffer = { sizeof(mClientID) ,(void*)&mClientID };
	ServiceDiscoveryResponse response = {};
	ENetAddress  responseAddress = { 0xffffffff, 0 };
	ENetBuffer responseBuffer = { sizeof(response),&response };
	// Send our client id to the server on the discovery port.
	enet_socket_send(mServiceDiscoverySocket, &broadcastAddress,&buffer, 1);
	{
		static size_t bytesRecv;
		do
		{
			// This will change responseAddress from 0xffffffff into the address of the server
			bytesRecv = enet_socket_receive(mServiceDiscoverySocket,&responseAddress,&responseBuffer, 1);

			if (bytesRecv == sizeof(response) && mClientID == response.clientID)
			{
				remote.host = responseAddress.host;
				remote.port = response.remotePort;
				serverDiscovered = true;
			}
		} while (bytesRecv > 0 && !serverDiscovered);
	}

	if (serverDiscovered)
	{
		char remoteIP[20];
		enet_address_get_host_ip(&remote, remoteIP, sizeof(remoteIP));
		LOG("Discovered session server: %s:%d", remoteIP, remote.port);

		enet_socket_destroy(mServiceDiscoverySocket);
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
	if (!mClientHost)
	{
		FAIL("Failed to create ENET client host");
		return false;
	}

	mServerPeer = enet_host_connect(mClientHost, &remote, RPCH_NumChannels, 0);
	if (!mServerPeer)
	{
		WARN("Failed to initiate connection to the server");
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
		return false;
	}

	ENetEvent event;
	if (enet_host_service(mClientHost, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
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
	if (mClientHost && mServerPeer)
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
	}

	if (mClientHost)
	{
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
	}
}

void SessionClient::Frame(const float HeadPose[4],const ControllerState& controllerState)
{
	if (mClientHost && mServerPeer)
	{
		SendHeadPose(HeadPose);
		SendInput(controllerState);

		ENetEvent event;
		while (enet_host_service(mClientHost, &event, 0) > 0)
		{
			switch (event.type)
			{
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
	if (IsConnected())
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
	switch (event.channelID)
	{
	case RPCH_Control:
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
		case avs::CommandPayloadType::Setup:
		{
			const avs::SetupCommand &setupCommand = *((const avs::SetupCommand*)packet->data);
			mCommandInterface->OnVideoStreamChanged(setupCommand);
			SendHandshake();
		}
		break;
		default:
			break;
	};
}

void SessionClient::ParseTextCommand(const char *txt_utf8)
{
	WARN("CMD: %s", txt_utf8);
	if (txt_utf8[0] == 'v')
	{
		int port, width, height;
		sscanf_s(txt_utf8, "v %d %d %d", &port, &width, &height);
		if (width == 0 && height == 0)
		{
			mCommandInterface->OnVideoStreamClosed();
		}
		else
		{
			avs::SetupCommand setupCommand;
			setupCommand.port = port;
			setupCommand.video_width = width;
			setupCommand.video_height = height/2;
			setupCommand.depth_width = width;
			setupCommand.depth_height = height/2;
			mCommandInterface->OnVideoStreamChanged(setupCommand);
			SendHandshake();
		}
	}
	else
	{
		WARN("Invalid text command: %c", txt_utf8[0]);
	}
}

void SessionClient::SendHeadPose(const float quat[4])
{
	ENetPacket* packet = enet_packet_create(quat, 4*sizeof(float), 0);
	enet_peer_send(mServerPeer, RPCH_HeadPose, packet);
}

void SessionClient::SendInput(const ControllerState& controllerState)
{
	RemotePlayInputState inputState = {};

	const uint32_t buttonsDiffMask = mPrevControllerState.mButtons ^ controllerState.mButtons;
	auto updateButtonState = [&inputState, &controllerState, buttonsDiffMask](uint32_t button)
	{
		if (buttonsDiffMask & button)
		{
			if (controllerState.mButtons & button) inputState.buttonsPressed |= button;
			else inputState.buttonsReleased |= button;
		}
	};

	// We need to update trackpad axis on the server whenever:
	// (1) User is currently touching the trackpad.
	// (2) User was touching the trackpad previous frame.
	bool updateTrackpadAxis = controllerState.mTrackpadStatus
		|| controllerState.mTrackpadStatus != mPrevControllerState.mTrackpadStatus;

	bool stateDirty = updateTrackpadAxis || buttonsDiffMask > 0;
	if (stateDirty)
	{
		enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;

		//	updateButtonState(ovrButton_A);
		//	updateButtonState(ovrButton_Enter); // FIXME: Currently not getting down event for this button.
		//	updateButtonState(ovrButton_Back);

			// Trackpad axis should be non-zero only if the user is currently touching the trackpad.
		if (controllerState.mTrackpadStatus)
		{
			// Remap axis value to [-1,1] range.
			inputState.trackpadAxisX = 2.0f * controllerState.mTrackpadX - 1.0f;
			inputState.trackpadAxisY = 2.0f * controllerState.mTrackpadY - 1.0f;

			// If this update does not include button information send it unreliably to improve latency.
			if (buttonsDiffMask == 0)
			{
				packetFlags = ENET_PACKET_FLAG_UNSEQUENCED;
			}
		}

		ENetPacket* packet = enet_packet_create(&inputState, sizeof(inputState), packetFlags);
		enet_peer_send(mServerPeer, RPCH_Control, packet);
	}
}

void SessionClient::SendHandshake()
{
	isReadyToReceivePayloads = true;
	ENetPacket *packet = enet_packet_create(&isReadyToReceivePayloads, sizeof(bool), 0);
	enet_peer_send(mServerPeer, RPCH_HANDSHAKE, packet);
}
