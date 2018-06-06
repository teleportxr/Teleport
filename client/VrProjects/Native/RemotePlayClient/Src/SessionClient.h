// (C) Copyright 2018 Simul.co

#pragma once

#include <enet/enet.h>

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

    void Service();

private:
    void DispatchEvent(const ENetEvent& event);
    void ParseCommandPacket(ENetPacket* packet);

    SessionCommandInterface* const mCommandInterface;
    ENetHost* mClientHost;
    ENetPeer* mServerPeer;
};

