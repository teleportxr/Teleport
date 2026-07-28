#include "HeadlessSessionCommandInterface.h"
#include "HeadlessGeometryTarget.h"
#include "TeleportClient/ClientPipeline.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <libavstream/pipeline.hpp>
#include <libavstream/common_maths.h>

HeadlessSessionCommandInterface::HeadlessSessionCommandInterface(std::shared_ptr<teleport::client::SessionClient> sc, HeadlessMode mode_)
	: sessionClient(sc), mode(mode_)
{
	if (mode == HeadlessMode::Simulated)
	{
		geometryTarget = std::make_unique<HeadlessGeometryTarget>();
		TELEPORT_LOG("HeadlessSessionCommandInterface: simulated mode, geometry target created");
	}
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
	handshake.framerate = 20.0f;
	handshake.udpBufferSize = sc->GetClientPipeline().source ? sc->GetClientPipeline().source->getSystemBufferSize() : 65536;
	handshake.maxBandwidthKpS = 50000;

	return true;
}

void HeadlessSessionCommandInterface::OnVideoStreamClosed()
{
	TELEPORT_LOG("Video stream closed");
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
	TELEPORT_LOG("Nodes moved: {}", updateList.size());
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
}

void HeadlessSessionCommandInterface::OnStreamingControlMessage(const std::string &str)
{
	TELEPORT_LOG("Streaming control message: {}", str);
}
