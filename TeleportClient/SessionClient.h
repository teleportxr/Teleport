// (C) Copyright 2018 Simul.co

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <enet/enet.h>
#include "libavstream/common.hpp"
#include "libavstream/common_networking.h"
#include <libavstream/libavstream.hpp>

#include "TeleportCore/Input.h"
#include "TeleportClient/DiscoveryService.h"
#include "TeleportClient/basic_linear_algebra.h"

typedef unsigned int uint;

namespace avs
{
	class GeometryTargetBackendInterface;
	class GeometryCacheBackendInterface;
}

class SessionCommandInterface
{
public:
    virtual bool OnSetupCommandReceived(const char* server_ip, const avs::SetupCommand& setupCommand, avs::Handshake& handshake) = 0;
    virtual void OnVideoStreamClosed() = 0;

    virtual void OnReconfigureVideo(const avs::ReconfigureVideoCommand& reconfigureVideoCommand) = 0;

    virtual bool OnNodeEnteredBounds(avs::uid node_uid) = 0;
    virtual bool OnNodeLeftBounds(avs::uid node_uid) = 0;
	virtual void OnLightingSetupChanged(const avs::SetupLightingCommand &) =0;
	virtual void OnInputsSetupChanged(const std::vector<avs::InputDefinition>& inputDefinitions) =0;
	virtual void UpdateNodeStructure(const avs::UpdateNodeStructureCommand& ) =0;
	virtual void UpdateNodeSubtype(const avs::UpdateNodeSubtypeCommand &,const std::string &)=0;
    
    virtual std::vector<avs::uid> GetGeometryResources() = 0;
    virtual void ClearGeometryResources() = 0;

    virtual void SetVisibleNodes(const std::vector<avs::uid>& visibleNodes) = 0;
    virtual void UpdateNodeMovement(const std::vector<avs::MovementUpdate>& updateList) = 0;
	virtual void UpdateNodeEnabledState(const std::vector<avs::NodeUpdateEnabledState>& updateList) = 0;
	virtual void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) = 0;
	virtual void UpdateNodeAnimation(const avs::ApplyAnimation& animationUpdate) = 0;
	virtual void UpdateNodeAnimationControl(const avs::NodeUpdateAnimationControl& animationControlUpdate) = 0;
	virtual void SetNodeAnimationSpeed(avs::uid nodeID, avs::uid animationID, float speed) = 0;
};

class SessionClient
{
public:
	SessionClient(
			SessionCommandInterface *commandInterface,
			std::unique_ptr<teleport::client::DiscoveryService> &&discoveryService);

	~SessionClient();

	void SetResourceCreator(avs::GeometryTargetBackendInterface *);
	void SetGeometryCache(avs::GeometryCacheBackendInterface* r);

	uint32_t Discover(
			std::string clientIP, uint16_t clientDiscoveryPort, std::string serverIP,
			uint16_t serverDiscoveryPort, ENetAddress &remote);

	bool Connect(const char *remoteIP, uint16_t remotePort, uint timeout);

	bool Connect(const ENetAddress &remote, uint timeout);

	void Disconnect(uint timeout, bool resetClientID = true);

	void SetPeerTimeout(uint timeout);

	void SendClientMessage(const avs::ClientMessage &msg);

	void Frame(
			const avs::DisplayInfo &displayInfo, const avs::Pose &headPose,
			const avs::Pose *controllerPoses, uint64_t originValidCounter,
			const avs::Pose &originPose, const teleport::core::Input& input,
			bool requestKeyframe, double time, double deltaTime);

	bool IsConnected() const;

	bool HasDiscovered() const;

	std::string GetServerIP() const;

	int GetPort() const;

	unsigned long long receivedInitialPos = 0;
	unsigned long long receivedRelativePos = 0;

	avs::Pose GetOriginPose() const;

	avs::vec3 GetOriginToHeadOffset() const;

	uint32_t GetClientID() const
	{
		return clientID;
	}

	void SetDiscoveryClientID(uint32_t clientID);

private:
    void DispatchEvent(const ENetEvent& event);
    void ParseCommandPacket(ENetPacket* packet);

    void SendDisplayInfo(const avs::DisplayInfo& displayInfo);
	void sendOriginPose(uint64_t validCounter,const avs::Pose& headPose);
    void SendControllerPoses(const avs::Pose& headPose,const avs::Pose* poses);
    void SendInput(const teleport::core::Input& input);
    void SendResourceRequests();
    void SendReceivedResources();
    void SendNodeUpdates();
    void SendKeyframeRequest();
    //Tell server we are ready to receive geometry payloads.
    void SendHandshake(const avs::Handshake &handshake, const std::vector<avs::uid>& clientResourceIDs);
	
    void ReceiveHandshakeAcknowledgement(const ENetPacket* packet);
	void ReceiveSetupCommand(const ENetPacket* packet);
	void ReceiveVideoReconfigureCommand(const ENetPacket* packet);
	void ReceivePositionUpdate(const ENetPacket* packet);
	void ReceiveNodeBoundsUpdate(const ENetPacket* packet);
    void ReceiveNodeMovementUpdate(const ENetPacket* packet);
	void ReceiveNodeEnabledStateUpdate(const ENetPacket* packet);
	void ReceiveNodeHighlightUpdate(const ENetPacket* packet);
	void ReceiveNodeAnimationUpdate(const ENetPacket* packet);
	void ReceiveNodeAnimationControlUpdate(const ENetPacket* packet);
	void ReceiveNodeAnimationSpeedUpdate(const ENetPacket* packet);
	void ReceiveSetupLightingCommand(const ENetPacket* packet);
	void ReceiveSetupInputsCommand(const ENetPacket* packet);
	void ReceiveUpdateNodeStructureCommand(const ENetPacket* packet);
	void ReceiveUpdateNodeSubtypeCommand(const ENetPacket* packet);

	static constexpr double RESOURCE_REQUEST_RESEND_TIME = 10.0; //Seconds we wait before resending a resource request.

    avs::uid lastServerID = 0; //UID of the server we last connected to.

    SessionCommandInterface* const mCommandInterface;
    std::unique_ptr<teleport::client::DiscoveryService> discoveryService;
	avs::GeometryTargetBackendInterface* mResourceCreator=nullptr;
	avs::GeometryCacheBackendInterface* geometryCache = nullptr;

    ENetHost* mClientHost = nullptr;
    ENetPeer* mServerPeer = nullptr;
	ENetAddress mServerEndpoint{};

    bool handshakeAcknowledged = false;
    std::vector<avs::uid> mResourceRequests; //Requests the session client has discovered need to be made; currently only for nodes.
	std::map<avs::uid, double> mSentResourceRequests; //<ID of requested resource, time we sent the request> Resource requests we have received, but have yet to receive confirmation of their receival.
    std::vector<avs::uid> mReceivedNodes; //Nodes that have entered bounds, are about to be drawn, and need to be confirmed to the server.
    std::vector<avs::uid> mLostNodes; //Node that have left bounds, are about to be hidden, and need to be confirmed to the server.

    avs::Pose originPose;
    avs::vec3 originToHeadPos;

	uint32_t clientID=0;

    double time=0.0;
    bool discovered=false;
	avs::SetupCommand setupCommand;
	avs::SetupLightingCommand setupLightingCommand;
	std::string remoteIP;
	double mTimeSinceLastServerComm = 0;
	std::vector<avs::InputDefinition> inputDefinitions;
};