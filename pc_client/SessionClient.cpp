// (C) Copyright 2018 Simul

#include <string>
#include <iostream>

#include "SessionClient.h"
#pragma comment(lib,"enet")
#pragma comment(lib,"C:\\RemotePlay\\libavstream\\build\\v140\\Release\\libavstream.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")

extern void ClientLog( const char * fileTag, int lineno, const char * format_str, ... )
{
	int size=(int)strlen(format_str)+100;
	static std::string str;
	va_list ap;
	int n=-1;
	while(n<0||n>=size)
	{
		str.resize(size);
		va_start(ap, format_str);
		//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
		n = vsnprintf((char *)str.c_str(), size,format_str, ap);
		va_end(ap);
		if(n> -1 && n < size)
		{
			str.resize(n);
		}
		if (n > -1)
			size=n+1;
		else
			size*=2;
	}
	std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<str.c_str()<<std::endl;
}

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

SessionClient::SessionClient(SessionCommandInterface* commandInterface)
	: mCommandInterface(commandInterface)
	, mClientHost(nullptr)
	, mServerPeer(nullptr)
	, mPrevControllerState({})
{}

SessionClient::~SessionClient()
{
	Disconnect(0);
}

bool SessionClient::Connect(const char* ipAddress, uint16_t port, uint timeout)
{
	ENetAddress address;
	enet_address_set_host_ip(&address, ipAddress);
	address.port = port;

	mClientHost = enet_host_create(nullptr, 1, RPCH_NumChannels, 0, 0);
	if(!mClientHost) {
		FAIL("Failed to create ENET client host");
		return false;
	}

	mServerPeer = enet_host_connect(mClientHost, &address, RPCH_NumChannels, 0);
	if(!mServerPeer) {
		WARN("Failed to initiate connection to the server");
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
		return false;
	}

	ENetEvent event;
	if(enet_host_service(mClientHost, &event, timeout) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
		LOG("Connected to session server: %s:%d", ipAddress, port);
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
				}
			}

			if (isPeerConnected) {
				enet_peer_reset(mServerPeer);
			}
		}
		mServerPeer = nullptr;
	}

	if(mClientHost) {
		enet_host_destroy(mClientHost);
		mClientHost = nullptr;
	}
}

void SessionClient::Frame( const ControllerState& controllerState)
{
	if(mClientHost && mServerPeer) {

		//SendHeadPose(vrFrame.Tracking.HeadPose);
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

void SessionClient::DispatchEvent(const ENetEvent& event)
{
	switch(event.channelID) {
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
	// TODO: Sanitize!
	const std::string command(reinterpret_cast<const char*>(packet->data), packet->dataLength);
	WARN("CMD: %s", command.c_str());
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
		WARN("Invalid command: %c", command[0]);
	}
}

/*void SessionClient::SendHeadPose(const ovrRigidBodyPosef& pose)
{
	// TODO: Use compact representation with only 3 float values for wire format.
	const ovrQuatf orientation = pose.Pose.Orientation;
	ENetPacket* packet = enet_packet_create(&orientation, sizeof(orientation), 0);
	enet_peer_send(mServerPeer, RPCH_HeadPose, packet);
}*/

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
	bool updateTrackpadAxis =	controllerState.mTrackpadStatus
							  || controllerState.mTrackpadStatus != mPrevControllerState.mTrackpadStatus;

	bool stateDirty =  updateTrackpadAxis || buttonsDiffMask > 0;
	if(stateDirty) {
		enet_uint32 packetFlags = ENET_PACKET_FLAG_RELIABLE;

	//	updateButtonState(ovrButton_A);
	//	updateButtonState(ovrButton_Enter); // FIXME: Currently not getting down event for this button.
	//	updateButtonState(ovrButton_Back);

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
