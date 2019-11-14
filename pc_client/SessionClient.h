// (C) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <string>
#include <enet/enet.h>
#include <libavstream/common.hpp>

#include "Input.h"
#include "Config.h"
#include "Log.h"
#include "crossplatform/basic_linear_algebra.h"

typedef unsigned int uint;
namespace avs
{
	struct SetupCommand;
	struct Handshake;
	typedef unsigned long long uid;
}

struct HeadPose
{
	scr::vec4 orientation;
	scr::vec3 position;
};

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
	SessionClient(SessionCommandInterface* commandInterface, ResourceCreator& resourceCreator);
	~SessionClient();

    bool Discover(uint16_t discoveryPort, ENetAddress& remote);
    bool Connect(const char* remoteIP, uint16_t remotePort, uint timeout);
    bool Connect(const ENetAddress& remote, uint timeout);
    void Disconnect(uint timeout);

	void SendClientMessage(const avs::ClientMessage &msg);

    void Frame(const HeadPose &headPose,bool pose_valid,const ControllerState &controllerState, bool requestKeyframe);

    bool IsConnected() const;
    std::string GetServerIP() const;

private:
	void DispatchEvent(const ENetEvent& event);
	void ParseCommandPacket(ENetPacket* packet);
	void ParseTextCommand(const char *txt_utf8);

	void SendHeadPose(const HeadPose& h);
	void SendInput(const ControllerState &controllerState);
	void SendResourceRequests();
	void SendReceivedResources();
	void SendActorUpdates();
	void SendKeyframeRequest();

	//Tell server we are ready to receive geometry payloads.
	void SendHandshake(const avs::Handshake& handshake);

	avs::uid lastServer_id = 0; //UID of the server we last connected to.
    uint32_t mClientID = 0;
	ENetSocket mServiceDiscoverySocket = 0;

    SessionCommandInterface* const mCommandInterface;
	ResourceCreator &mResourceCreator;
    ENetHost* mClientHost = nullptr;
    ENetPeer* mServerPeer = nullptr;
    ENetAddress mServerEndpoint;

    ControllerState mPrevControllerState = {};

	bool isReadyToReceivePayloads = false;
	bool handshakeAcknowledged = false;
	std::vector<avs::uid> mResourceRequests; //Requests the session client has discovered need to be made; currently only for actors.
	std::vector<avs::uid> mReceivedActors; //Actors that have entered bounds, are about to be drawn, and need to be confirmed to the server.
	std::vector<avs::uid> mLostActors; //Actor that have left bounds, are about to be hidden, and need to be confirmed to the server.
};

