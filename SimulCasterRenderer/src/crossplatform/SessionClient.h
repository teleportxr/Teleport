// (C) Copyright 2018 Simul.co

#pragma once

#include <string>
#include <vector>
#include <memory>

#include <enet/enet.h>
#include "libavstream/common.hpp"

#include "Input.h"
#include "DiscoveryService.h"
#include "basic_linear_algebra.h"

typedef unsigned int uint;
class ResourceCreator;

class SessionCommandInterface
{
public:
    virtual void OnVideoStreamChanged(const avs::SetupCommand &setupCommand,avs::Handshake &handshake, bool shouldClearEverything, std::vector<avs::uid>& resourcesClientNeeds, std::vector<avs::uid>& outExistingActors) = 0;
    virtual void OnVideoStreamClosed() = 0;

    virtual bool OnActorEnteredBounds(avs::uid actor_uid) = 0;
    virtual bool OnActorLeftBounds(avs::uid actor_uid) = 0;
};

class SessionClient
{
public:
    SessionClient(SessionCommandInterface* commandInterface, std::unique_ptr<DiscoveryService>&& discoveryService);
    ~SessionClient();

    void SetResourceCreator(ResourceCreator *);
    bool Discover(uint16_t discoveryPort, ENetAddress& remote);
    bool Connect(const char* remoteIP, uint16_t remotePort, uint timeout);
    bool Connect(const ENetAddress& remote, uint timeout);
    void Disconnect(uint timeout);

    void SendClientMessage(const avs::ClientMessage &msg);

    void Frame(const avs::DisplayInfo& displayInfo, const avs::HeadPose& headPose, const avs::HeadPose* controllerPoses,bool poseValid, const ControllerState& controllerState, bool requestKeyframe);

    bool IsConnected() const;
    std::string GetServerIP() const;
    
	bool receivedInitialPos = false;

    avs::vec3 GetInitialPos() const;
private:
    void DispatchEvent(const ENetEvent& event);
    void ParseCommandPacket(ENetPacket* packet);

    void SendDisplayInfo(const avs::DisplayInfo& displayInfo);
    void SendHeadPose(const avs::HeadPose& headPose);
    void SendControllerPoses(const avs::HeadPose& headPose,const avs::HeadPose* poses);
    void SendInput(const ControllerState& controllerState);
    void SendResourceRequests();
    void SendReceivedResources();
    void SendActorUpdates();
    void SendKeyframeRequest();
    //Tell server we are ready to receive geometry payloads.
    void SendHandshake(const avs::Handshake &handshake);

    avs::uid lastServerID = 0; //UID of the server we last connected to.

    SessionCommandInterface* const mCommandInterface;
    std::unique_ptr<DiscoveryService> discoveryService;
    ResourceCreator* mResourceCreator=nullptr;

    ENetHost* mClientHost = nullptr;
    ENetPeer* mServerPeer = nullptr;
    ENetAddress mServerEndpoint;

    ControllerState mPrevControllerState = {};

    bool handshakeAcknowledged = false;
    std::vector<avs::uid> mResourceRequests; //Requests the session client has discovered need to be made; currently only for actors.
    std::vector<avs::uid> mReceivedActors; //Actors that have entered bounds, are about to be drawn, and need to be confirmed to the server.
    std::vector<avs::uid> mLostActors; //Actor that have left bounds, are about to be hidden, and need to be confirmed to the server.

    avs::vec3 initialPos;
};