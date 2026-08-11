#pragma once

#include "TeleportClient/SessionClient.h"
#include "HeadlessGeometryTarget.h"
#include "HeadlessGeometryDecoder.h"
#include <memory>
#include <set>

enum class HeadlessMode
{
	Minimal,
	Simulated
};

class HeadlessSessionCommandInterface : public teleport::client::SessionCommandInterface
{
public:
	//! `cache` is owned by HeadlessClient and outlives this object; it receives the geometry
	//! the server streams and supplies the acknowledgement lists SessionClient drains.
	//! `serverUid` identifies the geometry cache to the decoder; SessionClient keeps its own copy
	//! private, so HeadlessClient passes the uid it resolved the session from.
	HeadlessSessionCommandInterface(std::shared_ptr<teleport::client::SessionClient> sc,
									HeadlessMode								   mode		 = HeadlessMode::Minimal,
									HeadlessGeometryCacheBackend				  *cache	 = nullptr,
									avs::uid									   serverUid = 0);
	virtual ~HeadlessSessionCommandInterface() = default;

	bool OnSetupCommandReceived(const char *server_ip, const teleport::core::SetupCommand &setupCommand) override;
	bool GetHandshake(teleport::core::Handshake &handshake) override;
	void OnVideoStreamClosed() override;
	void OnReconnectGaveUp() override;
	void OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand &reconfigureVideoCommand) override;

	bool OnNodeEnteredBounds(avs::uid node_uid) override;
	bool OnNodeLeftBounds(avs::uid node_uid) override;
	void OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions) override;
	void UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &cmd) override;
	void AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &cmd, const std::string &path) override;
	void SetOrigin(unsigned long long ctr, avs::uid origin_node_uid) override;

	std::vector<avs::uid> GetGeometryResources() override;
	void ClearGeometryResources() override;

	void SetVisibleNodes(const std::vector<avs::uid> &visibleNodes) override;
	void UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate> &updateList) override;
	void UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState> &updateList) override;
	void SetNodeHighlighted(avs::uid nodeID, bool isHighlighted) override;
	void UpdateNodeAnimation(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &animationUpdate) override;
	void OnStreamingControlMessage(const std::string &str) override;

	std::weak_ptr<teleport::client::SessionClient> GetSessionClient() const { return sessionClient; }
	void SetMode(HeadlessMode newMode) { mode = newMode; }
	//! The valid_counter from the last SetOriginNodeCommand. SessionClient::Frame gates
	//! pose sending on this being non-zero, so the tick loop reads it from here — it
	//! originates from the server, not from user input.
	uint64_t GetOriginValidCounter() const { return originValidCounter; }

private:
	std::weak_ptr<teleport::client::SessionClient> sessionClient;
	HeadlessMode mode = HeadlessMode::Minimal;
	std::set<avs::uid> nodesInBounds;
	std::vector<teleport::core::InputDefinition> inputDefinitions;
	uint64_t originValidCounter = 0;
	avs::uid originNodeUid = 0;
	size_t visibleNodesCount = 0;
	HeadlessGeometryCacheBackend *geometryCache = nullptr;
	avs::uid					  serverUid	   = 0;
	//! Both are referenced by pipeline nodes for the lifetime of the connection, so they must
	//! not be recreated while the pipeline is running.
	std::unique_ptr<HeadlessGeometryTarget>	 geometryTarget;
	std::unique_ptr<HeadlessGeometryDecoder> geometryDecoder;
};
