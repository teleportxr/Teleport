// (C) Copyright 2018 Simul.co

#pragma once

#include <string>
#include <vector>
#include <map>
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
    virtual void OnVideoStreamChanged(const char* server_ip, const avs::SetupCommand& setupCommand, avs::Handshake& handshake) = 0;
    virtual void OnVideoStreamClosed() = 0;

    virtual void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) = 0;

    virtual bool OnActorEnteredBounds(avs::uid actor_uid) = 0;
    virtual bool OnActorLeftBounds(avs::uid actor_uid) = 0;
    
    virtual std::vector<avs::uid> GetGeometryResources() = 0;
    virtual void ClearGeometryResources() = 0;

    virtual void SetVisibleActors(const std::vector<avs::uid>& visibleActors) = 0;
    virtual void UpdateActorMovement(const std::vector<avs::MovementUpdate>& updateList) = 0;
};

class SessionClient
{
public:
    SessionClient(SessionCommandInterface* commandInterface, std::unique_ptr<DiscoveryService>&& discoveryService);
    ~SessionClient();

    void SetResourceCreator(ResourceCreator *);
	uint32_t Discover(std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP, uint16_t serverDiscoveryPort, ENetAddress& remote);
    bool Connect(const char* remoteIP, uint16_t remotePort, uint timeout);
    bool Connect(const ENetAddress& remote, uint timeout);
    void Disconnect(uint timeout);

    void SendClientMessage(const avs::ClientMessage &msg);

    void Frame(const avs::DisplayInfo& displayInfo
        , const avs::Pose& headPose
        , const avs::Pose* controllerPoses
        , bool poseValid
        , const ControllerState* controllerState
        , bool requestKeyframe
	    , double time);

    bool IsConnected() const;
    std::string GetServerIP() const;
    int GetPort() const;
    
	unsigned long long receivedInitialPos = 0;

    avs::vec3 GetInitialPos() const;

    uint32_t GetClientID() const
	{
    	return clientID;
	}

private:
    void DispatchEvent(const ENetEvent& event);
    void ParseCommandPacket(ENetPacket* packet);

    void SendDisplayInfo(const avs::DisplayInfo& displayInfo);
    void SendHeadPose(const avs::Pose& headPose);
    void SendControllerPoses(const avs::Pose& headPose,const avs::Pose* poses);
    void SendInput(const ControllerState& controllerState);
    void SendResourceRequests();
    void SendReceivedResources();
    void SendActorUpdates();
    void SendKeyframeRequest();
    //Tell server we are ready to receive geometry payloads.
    void SendHandshake(const avs::Handshake &handshake, const std::vector<avs::uid>& clientResourceIDs);

    void ReceiveHandshakeAcknowledgement(const ENetPacket* packet);
    void ReceiveActorMovementUpdate(const ENetPacket* packet);

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
    std::map<avs::uid,double> mSentResourceRequests;
    std::vector<avs::uid> mReceivedActors; //Actors that have entered bounds, are about to be drawn, and need to be confirmed to the server.
    std::vector<avs::uid> mLostActors; //Actor that have left bounds, are about to be hidden, and need to be confirmed to the server.

    avs::vec3 initialPos;

	uint32_t clientID=0;

    double time=0.0;
};