// (C) Copyright 2018 Simul.co

#pragma once

#include <enet/enet.h>

#include "Input.h"


extern void ClientLog( const char * fileTag, int lineno, const char * fmt, ... );

#define LOG( ... ) 	ClientLog( __FILE__, __LINE__, __VA_ARGS__ )
#define WARN( ... ) ClientLog( __FILE__, __LINE__, __VA_ARGS__ )
#define FAIL( ... ) {ClientLog( __FILE__, __LINE__, __VA_ARGS__ );exit(0);}

typedef unsigned int uint;

class SessionCommandInterface
{
public:
	virtual void OnVideoStreamChanged(uint port, uint width, uint height) = 0;
	virtual void OnVideoStreamClosed() = 0;
};

class SessionClient
{
public:
	SessionClient(SessionCommandInterface* commandInterface);
	~SessionClient();

	bool Connect(const char* ipAddress, uint16_t port, uint timeout);
	void Disconnect(uint timeout);

	void Frame( const ControllerState& controllerState);

private:
	void DispatchEvent(const ENetEvent& event);
	void ParseCommandPacket(ENetPacket* packet);

	//void SendHeadPose(const ovrRigidBodyPosef& pose);
	void SendInput(const ControllerState& controllerState);

	SessionCommandInterface* const mCommandInterface;
	ENetHost* mClientHost;
	ENetPeer* mServerPeer;

	ControllerState mPrevControllerState;
};

