#include "HeadlessSessionCommandInterface.h"
#include "HeadlessGeometryTarget.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <libavstream/pipeline.hpp>
#include <libavstream/common_maths.h>

HeadlessSessionCommandInterface::HeadlessSessionCommandInterface(std::shared_ptr<teleport::client::SessionClient> sc,
																 HeadlessMode								  mode_,
																 HeadlessGeometryCacheBackend				 *cache,
																 avs::uid									  serverUid_)
	: sessionClient(sc), mode(mode_), geometryCache(cache), serverUid(serverUid_)
{
	// Geometry is tracked in both modes. Neither of these touches a graphics API, and without
	// them the server's geometry stream has nowhere to go and its packets are dropped.
	geometryTarget	= std::make_unique<HeadlessGeometryTarget>(geometryCache);
	geometryDecoder = std::make_unique<HeadlessGeometryDecoder>(geometryCache);
}

bool HeadlessSessionCommandInterface::OnSetupCommandReceived(const char *server_ip, const teleport::core::SetupCommand &setupCommand)
{
	auto sc = sessionClient.lock();
	if (!sc)
	{
		TELEPORT_WARN("SessionClient is null in OnSetupCommandReceived");
		return false;
	}

	try
	{
		auto &cp = sc->GetClientPipeline();

		// Wire outgoing queues (pose/input/acks to server)
		cp.reliableToServerQueue.configure(3000 * 64, "Reliable to server");
		avs::PipelineNode::link(cp.reliableToServerQueue, *(cp.source.get()));

		cp.unreliableToServerQueue.configure(3000 * 64, "Unreliable in");
		avs::PipelineNode::link(cp.unreliableToServerQueue, *(cp.source.get()));

		cp.nodePosesQueue.configure(3000, "Unreliable in");
		cp.inputStateQueue.configure(3000, "Unreliable in");
		avs::PipelineNode::link(cp.nodePosesQueue, *(cp.source.get()));
		avs::PipelineNode::link(cp.inputStateQueue, *(cp.source.get()));

		// Wire incoming reliable command decoder
		cp.reliableFromServerQueue.configure(3000 * 64, "Reliable from server");
		cp.commandDecoder.configure(sc.get(), "Reliable Decoder");
		avs::PipelineNode::link(*(cp.source.get()), cp.reliableFromServerQueue);
		cp.pipeline.link({&cp.reliableFromServerQueue, &cp.commandDecoder});

		// Configure video queue for reception but don't decode (we'll drain it with drop())
		cp.videoQueue.configure(300000, 16, "VideoQueue");
		avs::PipelineNode::link(*(cp.source.get()), cp.videoQueue);

		// Geometry. The queue's name must be "GeometryQueue" to match the stream table in
		// ClientPipeline, which routes stream 80 by the receiving node's display name.
		// avs::GeometryDecoder handles framing and hands our backend the payload type and uid;
		// the backend parses nodes and records pointer URLs without fetching any asset.
		cp.avsGeometryDecoder.configure(80, serverUid, geometryDecoder.get());
		cp.avsGeometryTarget.configure(geometryTarget.get());
		cp.geometryQueue.configure(16000000, 20, "GeometryQueue");
		avs::PipelineNode::link(*(cp.source.get()), cp.geometryQueue);
		avs::PipelineNode::link(cp.geometryQueue, cp.avsGeometryDecoder);
		cp.pipeline.link({&cp.avsGeometryDecoder, &cp.avsGeometryTarget});

		TELEPORT_LOG("Pipeline wiring complete");
		return true;
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("Exception in OnSetupCommandReceived: {}", e.what());
		return false;
	}
}

bool HeadlessSessionCommandInterface::GetHandshake(teleport::core::Handshake &handshake)
{
	auto sc = sessionClient.lock();
	if (!sc)
		return false;

	handshake.isVR = false;
	handshake.startDisplayInfo.width = 1280;
	handshake.startDisplayInfo.height = 720;
	handshake.axesStandard = avs::AxesStandard::EngineeringStyle;
	handshake.MetresPerUnit = 1.0f;
	handshake.FOV = 90.0f;
	handshake.framerate = 20;
	handshake.udpBufferSize = sc->GetClientPipeline().source ? static_cast<uint32_t>(sc->GetClientPipeline().source->getSystemBufferSize()) : 65536;
	handshake.maxBandwidthKpS = 50000;

	return true;
}

void HeadlessSessionCommandInterface::OnStreamingSessionEnded()
{
	TELEPORT_LOG("Streaming session ended");
}

void HeadlessSessionCommandInterface::OnReconnectGaveUp()
{
	TELEPORT_WARN("Reconnect gave up");
}

void HeadlessSessionCommandInterface::OnReconfigureVideo(const teleport::core::ReconfigureVideoCommand &reconfigureVideoCommand)
{
	TELEPORT_LOG("ReconfigureVideo command received");
}

bool HeadlessSessionCommandInterface::OnNodeEnteredBounds(avs::uid node_uid)
{
	nodesInBounds.insert(node_uid);
	return true;
}

bool HeadlessSessionCommandInterface::OnNodeLeftBounds(avs::uid node_uid)
{
	nodesInBounds.erase(node_uid);
	return true;
}

void HeadlessSessionCommandInterface::OnInputsSetupChanged(const std::vector<teleport::core::InputDefinition> &inputDefinitions)
{
	this->inputDefinitions = inputDefinitions;
	TELEPORT_LOG("Inputs setup changed: {} inputs", inputDefinitions.size());
}

void HeadlessSessionCommandInterface::UpdateNodeStructure(const teleport::core::UpdateNodeStructureCommand &cmd)
{
	TELEPORT_LOG("Node structure updated");
}

void HeadlessSessionCommandInterface::AssignNodePosePath(const teleport::core::AssignNodePosePathCommand &cmd, const std::string &path)
{
	TELEPORT_LOG("Assigned node pose path");
}

void HeadlessSessionCommandInterface::SetOrigin(unsigned long long ctr, avs::uid origin_node_uid)
{
	originValidCounter = ctr;
	this->originNodeUid = origin_node_uid;
}

std::vector<avs::uid> HeadlessSessionCommandInterface::GetGeometryResources()
{
	return std::vector<avs::uid>();
}

void HeadlessSessionCommandInterface::ClearGeometryResources()
{
}

void HeadlessSessionCommandInterface::SetVisibleNodes(const std::vector<avs::uid> &visibleNodes)
{
	visibleNodesCount = visibleNodes.size();
}

void HeadlessSessionCommandInterface::UpdateNodeMovement(const std::vector<teleport::core::MovementUpdate> &updateList)
{
	// Log the values, not just the count: server-driven motion is the one thing a headless
	// client can verify end to end that a rendering client can only be watched for, and
	// "Nodes moved: 1" is true of a follower that is working and of one stuck at the origin.
	// Rate-limited, because these arrive at 20 Hz per moving node for the whole session.
	movementUpdateCount += updateList.size();
	const bool report	 = (movementLogCountdown-- <= 0);
	if (report)
		movementLogCountdown = 40; // roughly every two seconds at a 20 Hz motion tick
	for (const auto &u : updateList)
	{
		if (!report)
			break;
		TELEPORT_LOG("Node {} moved to ({:.2f}, {:.2f}, {:.2f}) rot ({:.2f}, {:.2f}, {:.2f}, {:.2f}){} [{} updates so far]",
					 u.nodeID,
					 u.position.x, u.position.y, u.position.z,
					 u.rotation.x, u.rotation.y, u.rotation.z, u.rotation.w,
					 u.isGlobal ? " global" : " parent-local",
					 movementUpdateCount);
	}
}

void HeadlessSessionCommandInterface::UpdateNodeEnabledState(const std::vector<teleport::core::NodeUpdateEnabledState> &updateList)
{
	TELEPORT_LOG("Node enabled state updated: {}", updateList.size());
}

void HeadlessSessionCommandInterface::SetNodeHighlighted(avs::uid nodeID, bool isHighlighted)
{
}

void HeadlessSessionCommandInterface::UpdateNodeAnimation(std::chrono::microseconds timestampUs, const teleport::core::ApplyAnimation &animationUpdate)
{
	// Every field, unconditionally: a well-behaved server emits these only when the
	// locomotion state changes, so there are few of them and each one matters. The lead is
	// the interesting part — it is the cross-fade duration, and a server sending "now"
	// produces a visible snap that nothing else here would reveal.
	animationUpdateCount++;
	const double leadMs = double(animationUpdate.timestampUs - timestampUs.count()) / 1000.0;
	TELEPORT_LOG("Animation on node {}: clip {} cache {} layer {} at t={} us ({:+.0f} ms from now), "
				 "start {:.2f} s, rate {:.2f}, loop {} [{} so far]",
				 animationUpdate.nodeID,
				 animationUpdate.animationID,
				 animationUpdate.cacheID,
				 animationUpdate.animLayer,
				 animationUpdate.timestampUs,
				 leadMs,
				 animationUpdate.animTimeAtTimestamp,
				 animationUpdate.speedUnitsPerSecond,
				 animationUpdate.loop ? "yes" : "no",
				 animationUpdateCount);
}

void HeadlessSessionCommandInterface::OnStreamingControlMessage(const std::string &str)
{
	TELEPORT_LOG("Streaming control message: {}", str);
}
