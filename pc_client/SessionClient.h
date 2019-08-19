// (C) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <string>
#include <enet/enet.h>

#include "Input.h"
#include "Config.h"

extern void ClientLog( const char * fileTag, int lineno,const char *msg_type, const char * fmt, ... );

#define LOG( ... ) 	ClientLog( __FILE__, __LINE__,"info", __VA_ARGS__ )
#define WARN( ... ) ClientLog( __FILE__, __LINE__, "warning",__VA_ARGS__ )
#define FAIL( ... ) {ClientLog( __FILE__, __LINE__, "error",__VA_ARGS__ );exit(0);}

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

    bool Discover(uint16_t discoveryPort, ENetAddress& remote);
    bool Connect(const char* remoteIP, uint16_t remotePort, uint timeout);
    bool Connect(const ENetAddress& remote, uint timeout);
    void Disconnect(uint timeout);

    void Frame(const float HeadPose[4],const ControllerState &controllerState);

    bool IsConnected() const;
    std::string GetServerIP() const;

private:
	void DispatchEvent(const ENetEvent& event);
	void ParseCommandPacket(ENetPacket* packet);

	void SendHeadPose(const float quat[4]);
	void SendInput(const ControllerState& controllerState);
	//Tell server we are ready to receive geometry payloads.
	void SendHandshake();

    uint32_t mClientID = 0;
	ENetSocket mServiceDiscoverySocket = 0;

    SessionCommandInterface* const mCommandInterface;
    ENetHost* mClientHost = nullptr;
    ENetPeer* mServerPeer = nullptr;
    ENetAddress mServerEndpoint;

    ControllerState mPrevControllerState = {};

	bool isReadyToReceivePayloads = false;
};

