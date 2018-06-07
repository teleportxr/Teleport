// (C) Copyright 2018 Simul.co

#pragma once

#include <OVR_Input.h>
#include <enet/enet.h>

#include "Input.h"

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

    void Frame(const OVR::ovrFrameInput& vrFrame, const ControllerState& controllerState);

private:
    void DispatchEvent(const ENetEvent& event);
    void ParseCommandPacket(ENetPacket* packet);

    void SendHeadPose(const ovrRigidBodyPosef& pose);
    void SendInput(const ControllerState& controllerState);

    SessionCommandInterface* const mCommandInterface;
    ENetHost* mClientHost;
    ENetPeer* mServerPeer;

    ControllerState mPrevControllerState;
};

