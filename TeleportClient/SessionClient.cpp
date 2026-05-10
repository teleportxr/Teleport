// (C) Copyright 2018-2022 Simul Software Ltd
#include "SessionClient.h"

#include "Config.h"
#include "DiscoveryService.h"
#include "TabContext.h"
#include "TeleportClient/GeometryCacheBackendInterface.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/CommonNetworking.h"
#include "TeleportCore/CommandLogging.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/InputTypes.h"
#include "TeleportCore/Logging.h"
#include "libavstream/common.hpp"
#include <format>
#include <libavstream/geometry/mesh_interface.hpp>
#include <limits>

static_assert(sizeof(teleport::core::ClientDynamicLighting) == 57, "ClientDynamicLighting Size is not correct");

using namespace teleport;
using namespace client;
using namespace clientrender;
avs::Timestamp tBegin;
using std::string;

using namespace std::string_literals;
static std::map<avs::uid, std::shared_ptr<teleport::client::SessionClient>> sessionClients;
static std::set<avs::uid>													sessionClientIds;

const std::string														   &SessionClient::GetServerURL() const
{
	if (!server_domain_valid)
	{
		server_domain		= domain + "/"s + server_path;
		server_domain_valid = true;
	}
	return server_domain;
}

const std::set<avs::uid> &SessionClient::GetSessionClientIds()
{
	return sessionClientIds;
}

std::shared_ptr<teleport::client::SessionClient> SessionClient::GetSessionClient(avs::uid server_uid)
{
	auto i = sessionClients.find(server_uid);
	if (i == sessionClients.end())
	{
		// We can create client zero, but any other must use CreateSessionClient() via TabContext.
		if (server_uid == 0)
		{
			auto r			  = std::make_shared<client::SessionClient>(0, nullptr, "");
			sessionClients[0] = r;
			sessionClientIds.insert(0);

			return r;
		}
		return nullptr;
	}
	return i->second;
}

avs::uid SessionClient::CreateSessionClient(TabContext *tabContext, const std::string &domain)
{
	avs::uid server_uid = avs::GenerateUid();
	auto	 i			= sessionClients.find(server_uid);
	while (i != sessionClients.end())
	{
		server_uid = avs::GenerateUid();
		i		   = sessionClients.find(server_uid);
	}
	auto r					   = std::make_shared<client::SessionClient>(server_uid, tabContext, domain);
	sessionClients[server_uid] = r;
	sessionClientIds.insert(server_uid);
	return server_uid;
}

void SessionClient::DestroySessionClients()
{
	for (auto c : sessionClients)
	{
		c.second = nullptr;
	}
	sessionClientIds.clear();
	sessionClients.clear();
}

SessionClient::SessionClient(avs::uid s, TabContext *tc, const std::string &dom) : server_uid(s), domain(dom), tabContext(tc)
{
	if (server_uid == 0)
		setupCommand.startTimestamp_utc_unix_us =
			std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

SessionClient::~SessionClient()
{
	// Disconnect(0); causes crash. trying to access deleted objects.
}

void SessionClient::RequestConnection(const std::string &path, int port)
{
	if (connectionStatus != client::ConnectionStatus::UNCONNECTED)
		return;
	std::string ip = domain + "/"s + path;
	if (server_path != path || server_discovery_port != port)
	{
		SetServerPath(path);
		SetServerDiscoveryPort(port);
	}
	connectionStatus = client::ConnectionStatus::OFFERING;
}

void SessionClient::SetSessionCommandInterface(SessionCommandInterface *s)
{
	mCommandInterface = s;
}

void SessionClient::SetGeometryCache(GeometryCacheBackendInterface *r)
{
	geometryCache = r;
	// should never be necessary to geometryCache->ClearAll();
}

bool SessionClient::HandleConnections()
{
	if (!IsConnected())
	{
		auto &config = Config::GetInstance();
		if (connectionStatus == client::ConnectionStatus::OFFERING || connectionStatus == client::ConnectionStatus::RECONNECTING)
		{
			// In RECONNECTING we hold off discovery until the back-off has elapsed, and
			// we apply a per-attempt deadline so that a stalled discovery counts as a
			// failed attempt rather than blocking forever. The steady_clock is used so
			// that timing advances even though Frame() is not called in this state.
			if (connectionStatus == client::ConnectionStatus::RECONNECTING)
			{
				const auto now = std::chrono::steady_clock::now();
				if (now < nextReconnectTime)
				{
					teleport::client::DiscoveryService::GetInstance().Tick();
					return false;
				}
				if (now > reconnectAttemptDeadline)
				{
					reconnectAttempts++;
					if (reconnectAttempts >= config.options.maxReconnectAttempts)
					{
						TELEPORT_WARN("Reconnect to {0} failed after {1} attempts; giving up.", GetServerURL(), reconnectAttempts);
						GiveUpAndShutDown();
						return false;
					}
					currentReconnectBackoffMs	= std::min(currentReconnectBackoffMs * 2u, config.options.reconnectMaxBackoffMs);
					nextReconnectTime			= now + std::chrono::milliseconds(currentReconnectBackoffMs);
					reconnectAttemptDeadline	= nextReconnectTime + std::chrono::milliseconds(config.options.connectionTimeout);
					TELEPORT_INTERNAL_COUT("Reconnect attempt {0} timed out; next attempt in {1} ms.", reconnectAttempts, currentReconnectBackoffMs);
					return false;
				}
			}
			auto	&disc  = teleport::client::DiscoveryService::GetInstance();
			uint64_t cl_id = disc.Discover(server_uid, GetServerURL().c_str(), server_discovery_port);
			if (cl_id != 0 && Connect(GetServerURL().c_str(), config.options.connectionTimeout, cl_id))
			{
				bool shouldClearGeometry = disc.ShouldClear(server_uid);
				if (geometryCache && shouldClearGeometry)
				{
					geometryCache->ClearAll();
					shouldClearGeometry = false;
				}
				reconnectAttempts		  = 0;
				currentReconnectBackoffMs = 0;
				return true;
			}
		}
	}
	teleport::client::DiscoveryService::GetInstance().Tick();
	return false;
}

bool SessionClient::Connect(const char *remote_ip, uint timeout, avs::uid cl_id)
{
	tBegin	 = avs::Platform::getTimestamp();
	remoteIP = remote_ip;
	// TODO: don't reset this if reconnecting.
	ResetSessionState();

	clientID		 = cl_id;
	connectionStatus = ConnectionStatus::AWAITING_SETUP;
	return true;
}

#include <nlohmann/json.hpp>
using nlohmann::json;
void SessionClient::Disconnect(uint timeout, bool resetClientID)
{
	remoteIP = "";
	mCommandInterface->OnVideoStreamClosed();

	connectionStatus	  = ConnectionStatus::UNCONNECTED;
	receivedInitialPos	  = 0;
	receivedLightingAckId = 0;
	if (resetClientID)
	{
		clientID = 0;
	}
	if (server_uid)
	{
		clientPipeline.pipeline.deconfigure();
		clientPipeline.source->deconfigure();
		DiscoveryService::GetInstance().Disconnect(server_uid);
	}
}

void SessionClient::Frame(const avs::DisplayInfo								&displayInfo,
						  const teleport::core::Pose							&headPose,
						  const std::map<avs::uid, teleport::core::PoseDynamic> &nodePoses,
						  uint64_t												 poseValidCounter,
						  const core::Input										&input,
						  double												 t,
						  double												 deltaTime)
{
	time = t;
	// Source might not yet be configured...
	if (clientPipeline.source)
	{
		std::string str;
		if (clientPipeline.source->getNextStreamingControlMessage(str))
		{
			SendStreamingControlMessage(str);
		}
		std::string msg;
		while (teleport::client::DiscoveryService::GetInstance().GetNextMessage(server_uid, msg))
		{
			clientPipeline.source->receiveStreamingControlMessage(msg);
		}
		std::vector<uint8_t> bin;
		while (teleport::client::DiscoveryService::GetInstance().GetNextBinaryMessage(server_uid, bin))
		{
			ReceiveCommand(bin);
		}
	}
	// Detect transitions of the underlying streaming connection. A move from CONNECTED
	// (or a previously-good state) to DISCONNECTED/FAILED/CLOSED is treated as a drop
	// of an established session, which triggers reconnection. We only do this if we had
	// successfully completed a setup at least once (lastSessionId != 0); otherwise it
	// is an initial-connect failure which is left to the caller to retry.
	if (clientPipeline.source && lastSessionId != 0)
	{
		auto streamingState = clientPipeline.source->GetStreamingConnectionState();
		if (streamingState != lastStreamingState)
		{
			const bool wasUp   = (lastStreamingState == avs::StreamingConnectionState::CONNECTED);
			const bool isDown  = (streamingState == avs::StreamingConnectionState::DISCONNECTED ||
								  streamingState == avs::StreamingConnectionState::FAILED ||
								  streamingState == avs::StreamingConnectionState::CLOSED);
			if (wasUp && isDown && connectionStatus != ConnectionStatus::RECONNECTING)
			{
				TELEPORT_WARN("Streaming connection to {0} dropped ({1}); beginning reconnect.",
							  GetServerURL(), avs::stringOf(streamingState));
				BeginReconnect();
			}
			lastStreamingState = streamingState;
		}
	}
	bool requestKeyframe = clientPipeline.decoder.idrRequired();
	{
		if (connectionStatus == ConnectionStatus::CONNECTED)
		{
			static double sendInterval = 0.1;
			if (time - lastSendTime > sendInterval)
			{
				SendDisplayInfo(displayInfo);
				if (poseValidCounter)
				{
					SendNodePoses(headPose, nodePoses);
				}
				SendInput(input);
				SendReceivedResources();
				SendNodeUpdates();
				if (requestKeyframe)
					SendKeyframeRequest();
				lastSendTime = t;
			}
		}
	}

	mTimeSinceLastServerComm += deltaTime;

	// TODO: These pipelines could be on different threads,
	messageToServerPipeline.process();
}

void SessionClient::SetServerPath(std::string path)
{
	server_path = path;
}

void SessionClient::SetServerDiscoveryPort(int p)
{
	server_discovery_port = p;
}

std::string SessionClient::GetConnectionURL() const
{
	return connected_url;
}

int SessionClient::GetPort() const
{
	return server_discovery_port;
}

std::string SessionClient::GetServerIP() const
{
	return remoteIP;
}

ConnectionStatus SessionClient::GetConnectionStatus() const
{
	return connectionStatus;
}

avs::StreamingConnectionState SessionClient::GetStreamingConnectionState() const
{
	if (!clientPipeline.source)
		return avs::StreamingConnectionState::ERROR_STATE;
	return clientPipeline.source->GetStreamingConnectionState();
}

bool SessionClient::IsConnecting() const
{
	if (connectionStatus == ConnectionStatus::OFFERING || connectionStatus == ConnectionStatus::HANDSHAKING ||
		connectionStatus == ConnectionStatus::AWAITING_SETUP)
		return true;
	return false;
}

bool SessionClient::IsConnected() const
{
	return (connectionStatus == ConnectionStatus::CONNECTED || connectionStatus == ConnectionStatus::HANDSHAKING ||
			connectionStatus == ConnectionStatus::AWAITING_SETUP);
}

bool SessionClient::IsReadyToRender() const
{
	if (IsConnected())
		return true;
	return false;
}

avs::Result SessionClient::decode(const void *buffer, size_t bufferSizeInBytes)
{
	if (!buffer || bufferSizeInBytes < 1)
		return avs::Result::Failed;
	std::vector<uint8_t> packet(bufferSizeInBytes);
	memcpy(packet.data(), (uint8_t *)buffer, bufferSizeInBytes);
	ReceiveCommandPacket(packet);
	return avs::Result::OK;
}

void SessionClient::KillStreaming()
{
	if (clientPipeline.source)
	{
		clientPipeline.source->kill();
	}
}

void SessionClient::SetTimestamp(std::chrono::microseconds t)
{
	session_time_us = t;
}

void SessionClient::ReceiveCommand(const std::vector<uint8_t> &buffer)
{
	ReceiveCommandPacket(buffer);
}

void SessionClient::ReceiveCommandPacket(const std::vector<uint8_t> &packet)
{
	if (packet.size() < sizeof(teleport::core::CommandPayloadType))
	{
		TELEPORT_INTERNAL_CERR("Truncated command packet ({0} bytes)", packet.size());
		return;
	}
	teleport::core::CommandPayloadType commandPayloadType = *(reinterpret_cast<const teleport::core::CommandPayloadType *>(packet.data()));
	switch (commandPayloadType)
	{
	case teleport::core::CommandPayloadType::Shutdown:
		mCommandInterface->OnVideoStreamClosed();
		break;
	case teleport::core::CommandPayloadType::Setup:
		ReceiveSetupCommand(packet);
		break;
	case teleport::core::CommandPayloadType::AcknowledgeHandshake:
		ReceiveHandshakeAcknowledgement(packet);
		break;
	case teleport::core::CommandPayloadType::ReconfigureVideo:
		ReceiveVideoReconfigureCommand(packet);
		break;
	case teleport::core::CommandPayloadType::SetOriginNode:
		ReceiveOriginNodeId(packet);
		break;
	case teleport::core::CommandPayloadType::NodeVisibility:
		ReceiveNodeVisibilityUpdate(packet);
		break;
	case teleport::core::CommandPayloadType::UpdateNodeMovement:
		ReceiveNodeMovementUpdate(packet);
		break;
	case teleport::core::CommandPayloadType::UpdateNodeEnabledState:
		ReceiveNodeEnabledStateUpdate(packet);
		break;
	case teleport::core::CommandPayloadType::SetNodeHighlighted:
		ReceiveNodeHighlightUpdate(packet);
		break;
	case teleport::core::CommandPayloadType::ApplyNodeAnimation:
		ReceiveNodeAnimationUpdate(packet);
		break;
	case teleport::core::CommandPayloadType::SetupLighting:
		ReceiveSetupLightingCommand(packet);
		break;
	case teleport::core::CommandPayloadType::SetupInputs:
		ReceiveSetupInputsCommand(packet);
		break;
	case teleport::core::CommandPayloadType::UpdateNodeStructure:
		ReceiveUpdateNodeStructureCommand(packet);
		break;
	case teleport::core::CommandPayloadType::AssignNodePosePath:
		ReceiveAssignNodePosePathCommand(packet);
		break;
	case teleport::core::CommandPayloadType::PingForLatency:
		ReceivePingForLatencyCommand(packet);
		break;
	default:
		TELEPORT_INTERNAL_CERR("Invalid CommandPayloadType.");
		TELEPORT_INTERNAL_BREAK_ONCE("Invalid payload");
		break;
	};
}

void SessionClient::SendDisplayInfo(const avs::DisplayInfo &displayInfo)
{
	if (connectionStatus != ConnectionStatus::CONNECTED)
		return;
	core::DisplayInfoMessage displayInfoMessage;
	displayInfoMessage.displayInfo = displayInfo;

	SendMessageToServer(&displayInfoMessage, sizeof(displayInfoMessage));
}

void SessionClient::TimestampMessage(teleport::core::ClientMessage &msg)
{
	auto ts				  = avs::Platform::getTimestamp();
	msg.timestamp_session_us = (int64_t)(avs::Platform::getTimeElapsedInMilliseconds(tBegin, ts) * 1000.0);
}

void SessionClient::SendNodePoses(const teleport::core::Pose &headPose, const std::map<avs::uid, teleport::core::PoseDynamic> poses)
{
	teleport::core::NodePosesMessage message;
	TimestampMessage(message);

	message.headPose = headPose;
	message.numPoses = (uint16_t)poses.size();
	if (isnan(headPose.position.x))
	{
		TELEPORT_WARN("Trying to send NaN\n");
		return;
	}
	size_t				 messageSize = sizeof(teleport::core::NodePosesMessage) + message.numPoses * sizeof(teleport::core::NodePose);
	std::vector<uint8_t> packet(messageSize);
	memcpy(packet.data(), &message, sizeof(teleport::core::NodePosesMessage));
	packet.resize(messageSize);
	int						 i = 0;
	teleport::core::NodePose nodePose;
	uint8_t					*target = packet.data() + sizeof(teleport::core::NodePosesMessage);
	for (const auto &p : poses)
	{
		nodePose.uid		 = p.first;
		nodePose.poseDynamic = p.second;
		memcpy(target, &nodePose, sizeof(nodePose));
		target += sizeof(nodePose);
	}
	// This is a special type of message, with its own queue.
	clientPipeline.nodePosesQueue.push(packet.data(), messageSize);
}

static void copy_and_increment(uint8_t *&target, const void *source, size_t size)
{
	if (!size)
		return;
	memcpy(target, source, size);
	target += size;
}

void SessionClient::SendInput(const core::Input &input)
{
	if (connectionStatus != ConnectionStatus::CONNECTED)
		return;
	teleport::core::InputStatesMessage inputStatesMessage;
	teleport::core::InputEventsMessage inputEventsMessage;
	auto							   ts = avs::Platform::getTimestamp();
	inputEventsMessage.timestamp_session_us  = inputStatesMessage.timestamp_session_us;
	// Set event amount.
	if (input.analogueEvents.size() > 50)
	{
		TELEPORT_BREAK_ONCE("That's a lot of events.");
	}
	inputStatesMessage.inputState.numBinaryStates	= static_cast<uint32_t>(input.binaryStates.size());
	inputStatesMessage.inputState.numAnalogueStates = static_cast<uint32_t>(input.analogueStates.size());
	inputEventsMessage.numBinaryEvents				= static_cast<uint32_t>(input.binaryEvents.size());
	inputEventsMessage.numAnalogueEvents			= static_cast<uint32_t>(input.analogueEvents.size());
	inputEventsMessage.numMotionEvents				= static_cast<uint32_t>(input.motionEvents.size());
	// Calculate sizes for memory copy operations.
	size_t inputStateSize							= sizeof(inputStatesMessage);
	size_t binaryStateSize							= inputStatesMessage.inputState.numBinaryStates;
	size_t analogueStateSize						= sizeof(float) * inputStatesMessage.inputState.numAnalogueStates;

	size_t statesPacketSize							= inputStateSize + binaryStateSize + analogueStateSize;
	// Copy events into packet.
	{
		std::vector<uint8_t> buffer(statesPacketSize);
		uint8_t				*target = buffer.data();
		copy_and_increment(target, &inputStatesMessage, inputStateSize);
		copy_and_increment(target, input.binaryStates.data(), binaryStateSize);
		copy_and_increment(target, input.analogueStates.data(), analogueStateSize);
		// This is a special type of message, with its own queue.
		clientPipeline.inputStateQueue.push(buffer.data(), statesPacketSize);
	}
	{
		size_t				 binaryEventSize   = sizeof(teleport::core::InputEventBinary) * inputEventsMessage.numBinaryEvents;
		size_t				 analogueEventSize = sizeof(teleport::core::InputEventAnalogue) * inputEventsMessage.numAnalogueEvents;
		size_t				 motionEventSize   = sizeof(teleport::core::InputEventMotion) * inputEventsMessage.numMotionEvents;

		size_t				 inputEventsSize   = sizeof(inputEventsMessage);
		size_t				 eventsPacketSize  = sizeof(inputEventsMessage) + binaryEventSize + analogueEventSize + motionEventSize;

		std::vector<uint8_t> buffer(eventsPacketSize);
		uint8_t				*target = buffer.data();
		copy_and_increment(target, &inputEventsMessage, inputEventsSize);
		copy_and_increment(target, input.binaryEvents.data(), binaryEventSize);
		copy_and_increment(target, input.analogueEvents.data(), analogueEventSize);
		copy_and_increment(target, input.motionEvents.data(), motionEventSize);
		SendMessageToServer(buffer.data(), eventsPacketSize);
	}
}

void SessionClient::SendReceivedResources()
{
	std::vector<avs::uid> receivedResources = geometryCache->GetReceivedResources();
	geometryCache->ClearReceivedResources();

	if (receivedResources.size() != 0)
	{

		teleport::core::ReceivedResourcesMessage message(receivedResources.size());

		size_t									 messageSize		   = sizeof(teleport::core::ReceivedResourcesMessage);
		size_t									 receivedResourcesSize = sizeof(avs::uid) * receivedResources.size();

		std::vector<uint8_t>					 packet(messageSize + receivedResourcesSize);
		memcpy(packet.data(), &message, messageSize);
		memcpy(packet.data() + messageSize, receivedResources.data(), receivedResourcesSize);

		SendMessageToServer(packet.data(), messageSize + receivedResourcesSize);
	}
}

void SessionClient::SendNodeUpdates()
{
	// Insert completed nodes.

	const std::vector<avs::uid> &completedNodes = geometryCache->GetCompletedNodes();
	if (completedNodes.size() > 0)
	{
		size_t n = mReceivedNodes.size();
		mReceivedNodes.resize(mReceivedNodes.size() + completedNodes.size());
		memcpy(mReceivedNodes.data() + n, completedNodes.data(), completedNodes.size() * sizeof(avs::uid));
		// mReceivedNodes.insert(mReceivedNodes.end(), completedNodes.begin(), completedNodes.end());
		geometryCache->ClearCompletedNodes();
	}

	if (mReceivedNodes.size() != 0 || mLostNodes.size() != 0)
	{
		teleport::core::NodeStatusMessage message(mReceivedNodes.size(), mLostNodes.size());

		size_t							  messageSize  = sizeof(teleport::core::NodeStatusMessage);
		size_t							  receivedSize = sizeof(avs::uid) * mReceivedNodes.size();
		size_t							  lostSize	   = sizeof(avs::uid) * mLostNodes.size();

		std::vector<uint8_t>			  packet(messageSize + receivedSize + lostSize);
		memcpy(packet.data(), &message, messageSize);
		memcpy(packet.data() + messageSize, mReceivedNodes.data(), receivedSize);
		memcpy(packet.data() + messageSize + receivedSize, mLostNodes.data(), lostSize);

#ifndef FIX_BROKEN
		SendMessageToServer(packet.data(), messageSize + receivedSize + lostSize);
#endif
		mReceivedNodes.clear();
		mLostNodes.clear();
	}
}

void SessionClient::SendKeyframeRequest()
{
	teleport::core::KeyframeRequestMessage msg;
	SendMessageToServer(&msg, sizeof(msg));
}

bool SessionClient::SendMessageToServer(const void *c, size_t sz) const
{
	if (sz > 16384)
		return false;
	auto b = std::make_shared<std::vector<uint8_t>>(sz);
	memcpy(b->data(), c, sz);
	messageToServerStack.PushBuffer(b);
	return true;
}

void SessionClient::SendHandshake(const teleport::core::Handshake &handshake, const std::vector<avs::uid> &clientResourceIDs)
{
	TELEPORT_INTERNAL_CERR("Sending handshake via Websockets");
	size_t handshakeSize	= sizeof(teleport::core::Handshake);
	size_t resourceListSize = sizeof(avs::uid) * clientResourceIDs.size();

	// Create handshake.
	// Append list of resource IDs the client has.
	std::vector<uint8_t> bin(handshakeSize + resourceListSize);

	memcpy(bin.data(), &handshake, handshakeSize);
	memcpy(bin.data() + handshakeSize, clientResourceIDs.data(), resourceListSize);
	DiscoveryService::GetInstance().SendBinary(server_uid, bin);
}

void SessionClient::SendStreamingControlMessage(const std::string &msg)
{
	teleport::client::DiscoveryService::GetInstance().Send(server_uid, msg);
}

void SessionClient::ReceiveHandshakeAcknowledgement(const std::vector<uint8_t> &packet)
{
	TELEPORT_INTERNAL_CERR("Received handshake acknowledgement.\n");
	if (connectionStatus != ConnectionStatus::HANDSHAKING)
	{
		TELEPORT_INTERNAL_CERR("Received handshake acknowledgement, but not in HANDSHAKING mode.\n");
		return;
	}
	size_t commandSize = sizeof(teleport::core::AcknowledgeHandshakeCommand);
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad AcknowledgeHandshake. Packet size {0} < command size {1}", packet.size(), commandSize);
		return;
	}

	// Extract command from packet.
	teleport::core::AcknowledgeHandshakeCommand command;
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);

	// Extract list of visible nodes — guard against malformed visibleNodeCount.
	const size_t expectedTrailing = sizeof(avs::uid) * static_cast<size_t>(command.visibleNodeCount);
	if (command.visibleNodeCount > 0 && (packet.size() - commandSize) < expectedTrailing)
	{
		TELEPORT_INTERNAL_CERR("Bad AcknowledgeHandshake. visibleNodeCount {0} exceeds packet bytes ({1} available)",
			command.visibleNodeCount, packet.size() - commandSize);
		return;
	}
	std::vector<avs::uid> visibleNodes(command.visibleNodeCount);
	if (command.visibleNodeCount > 0)
		memcpy(visibleNodes.data(), packet.data() + commandSize, expectedTrailing);

	mCommandInterface->SetVisibleNodes(visibleNodes);

	connectionStatus = ConnectionStatus::CONNECTED;
}

void SessionClient::ReceiveSetupCommand(const std::vector<uint8_t> &packet)
{
	TELEPORT_INTERNAL_CERR("ReceiveSetupCommand");
	if (connectionStatus == client::ConnectionStatus::AWAITING_SETUP || setupCommand.session_id != lastSessionId)
	{
		size_t commandSize = sizeof(teleport::core::SetupCommand);
		if (packet.size() != commandSize)
		{
			TELEPORT_INTERNAL_CERR("Bad SetupCommand. Size should be {0} but it's {1}", commandSize, packet.size());
			return;
		}
		const teleport::core::SetupCommand *s = reinterpret_cast<const teleport::core::SetupCommand *>(packet.data());
		ApplySetup(*s);

		// Log the received setup command for debugging
		TELEPORT_INTERNAL_COUT("\n===== CLIENT RECEIVED SETUPCOMMAND =====\n{}\n===== END SETUPCOMMAND =====",
			teleport::core::SetupCommandToString(setupCommand));
		if (!clientPipeline.Init(setupCommand, remoteIP.c_str()))
			return;
		unreliableToServerEncoder.configure(&messageToServerStack, "Unreliable Message Encoder");
		messageToServerPipeline.link({&unreliableToServerEncoder, &clientPipeline.unreliableToServerQueue});
		if (!mCommandInterface->OnSetupCommandReceived(remoteIP.c_str(), setupCommand))
			return;
		// Set it running.
		clientPipeline.pipeline.processAsync();
	}
	teleport::core::Handshake handshake;
	mCommandInterface->GetHandshake(handshake);
	std::vector<avs::uid> resourceIDs;
	if (setupCommand.session_id == lastSessionId)
	{
		resourceIDs				= mCommandInterface->GetGeometryResources();
		handshake.resourceCount = resourceIDs.size();
	}
	else
	{
		mCommandInterface->ClearGeometryResources();
	}
	if (connectionStatus == client::ConnectionStatus::AWAITING_SETUP)
		connectionStatus = client::ConnectionStatus::HANDSHAKING;
	SendHandshake(handshake, resourceIDs);
	lastSessionId = setupCommand.session_id;
	if (tabContext)
		tabContext->ConnectionComplete(server_uid);
}

void SessionClient::ApplySetup(const teleport::core::SetupCommand &s)
{
	// Copy command out of packet.
	memcpy(static_cast<void *>(&setupCommand), &s, sizeof(s));
}

void SessionClient::ReceiveVideoReconfigureCommand(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(teleport::core::ReconfigureVideoCommand);
	// Copy command out of packet.
	teleport::core::ReconfigureVideoCommand reconfigureCommand;
	memcpy(static_cast<void *>(&reconfigureCommand), packet.data(), commandSize);
	setupCommand.video_config = reconfigureCommand.video_config;
	mCommandInterface->OnReconfigureVideo(reconfigureCommand);
}

void SessionClient::ReceiveOriginNodeId(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(teleport::core::SetOriginNodeCommand);
	if (packet.size() != commandSize)
	{
		TELEPORT_INTERNAL_CERR("SetOriginNodeCommand size is wrong. Struct is {0}, but packet was {1}.", commandSize, packet.size());
		return;
	}
	teleport::core::SetOriginNodeCommand command;
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);
	if (command.valid_counter > receivedInitialPos)
	{
		//TELEPORT_INTERNAL_COUT("Received origin node {0} with counter {1}.", command.origin_node, command.valid_counter);
		receivedInitialPos = command.valid_counter;
		mCommandInterface->SetOrigin(command.valid_counter, command.origin_node);
	}
	else
	{
		TELEPORT_INTERNAL_CERR(
			"Received out-of-date origin node {0}, counter was {1}, but last update was {2}.", command.origin_node, command.valid_counter, receivedInitialPos);
	}
	// And acknowledge it.
	Ack(command.ack_id);
}

void SessionClient::Ack(uint64_t ack_id)
{
	teleport::core::AcknowledgementMessage msg;
	TimestampMessage(msg);
	msg.ack_id = ack_id;
	sendMessageToServer(msg);
}

void SessionClient::ReceiveNodeVisibilityUpdate(const std::vector<uint8_t> &packet)
{
	size_t								  commandSize = sizeof(teleport::core::NodeVisibilityCommand);

	teleport::core::NodeVisibilityCommand command;
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);

	size_t				  enteredSize = sizeof(avs::uid) * command.nodesShowCount;
	size_t				  leftSize	  = sizeof(avs::uid) * command.nodesHideCount;

	std::vector<avs::uid> enteredNodes(command.nodesShowCount);
	std::vector<avs::uid> leftNodes(command.nodesHideCount);

	memcpy(enteredNodes.data(), packet.data() + commandSize, enteredSize);
	memcpy(leftNodes.data(), packet.data() + commandSize + enteredSize, leftSize);

	std::vector<avs::uid> missingNodes;
	// Tell the renderer to show the nodes that have entered the streamable bounds; create resend requests for nodes it does not have the data on, and confirm
	// nodes it does have the data for.
	for (avs::uid node_uid : enteredNodes)
	{
		if (!mCommandInterface->OnNodeEnteredBounds(node_uid))
		{
			missingNodes.push_back(node_uid);
		}
		else
		{
			mReceivedNodes.push_back(node_uid);
		}
	}
	// Tell renderer to hide nodes that have left bounds.
	for (avs::uid node_uid : leftNodes)
	{
		if (mCommandInterface->OnNodeLeftBounds(node_uid))
		{
			mLostNodes.push_back(node_uid);
		}
	}
}

void SessionClient::ReceiveNodeMovementUpdate(const std::vector<uint8_t> &packet)
{
	// Extract command from packet.
	teleport::core::UpdateNodeMovementCommand command;
	size_t									  commandSize = command.getCommandSize();
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);
	size_t fullSize = commandSize + sizeof(teleport::core::MovementUpdate) * command.updatesCount;
	if (packet.size() != fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	std::vector<teleport::core::MovementUpdate> updateList(command.updatesCount);
	memcpy(updateList.data(), packet.data() + commandSize, sizeof(teleport::core::MovementUpdate) * command.updatesCount);

	mCommandInterface->UpdateNodeMovement(updateList);
}

void SessionClient::ReceiveNodeEnabledStateUpdate(const std::vector<uint8_t> &packet)
{
	// Extract command from packet.
	teleport::core::UpdateNodeEnabledStateCommand command;
	size_t										  commandSize = command.getCommandSize();
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);

	size_t fullSize = commandSize + sizeof(teleport::core::NodeUpdateEnabledState) * command.updatesCount;
	if (packet.size() != fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	std::vector<teleport::core::NodeUpdateEnabledState> updateList(command.updatesCount);
	memcpy(updateList.data(), packet.data() + commandSize, sizeof(teleport::core::NodeUpdateEnabledState) * command.updatesCount);

	mCommandInterface->UpdateNodeEnabledState(updateList);
}

void SessionClient::ReceiveNodeHighlightUpdate(const std::vector<uint8_t> &packet)
{
	teleport::core::SetNodeHighlightedCommand command;
	size_t									  commandSize = command.getCommandSize();
	if (packet.size() != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);

	mCommandInterface->SetNodeHighlighted(command.nodeID, command.isHighlighted);
}

void SessionClient::ReceiveNodeAnimationUpdate(const std::vector<uint8_t> &packet)
{
	// Extract command from packet.
	teleport::core::ApplyAnimationCommand command;
	size_t								  commandSize = command.getCommandSize();
	if (packet.size() != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	memcpy(static_cast<void *>(&command), packet.data(), commandSize);
	mCommandInterface->UpdateNodeAnimation(GetTimestamp(), command.animationUpdate);
}

void SessionClient::ReceiveSetupLightingCommand(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(teleport::core::SetLightingCommand);
	if (packet.size() < commandSize)
	{
		TELEPORT_WARN_INTERNAL("Bad packet size");
		return;
	}

	// Copy command out of packet.
	memcpy(static_cast<void *>(&setLightingCommand), packet.data(), commandSize);
	size_t fullSize = commandSize;
	if (packet.size() != fullSize)
	{
		TELEPORT_WARN_INTERNAL("Bad packet size");
		return;
	}
	if (setLightingCommand.ack_id > receivedLightingAckId)
	{
		TELEPORT_INFO("Received lighting setup {}.\n{}", setLightingCommand.ack_id,
			teleport::core::SetLightingCommandToString(setLightingCommand));
		receivedLightingAckId = setLightingCommand.ack_id;
		clientDynamicLighting = setLightingCommand.clientDynamicLighting;
	}
	else
	{
		TELEPORT_WARN_INTERNAL(
			"Received out-of-date lighting setup, counter was {}, but last update was {}.", setLightingCommand.ack_id, receivedLightingAckId);
	}
	// And acknowledge it.
	Ack(setLightingCommand.ack_id);
}

void SessionClient::ReceiveSetupInputsCommand(const std::vector<uint8_t> &packet)
{
	teleport::core::SetupInputsCommand setupInputsCommand;
	size_t							   commandSize = sizeof(teleport::core::SetupInputsCommand);
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}

	memcpy(static_cast<void *>(&setupInputsCommand), packet.data(), sizeof(teleport::core::SetupInputsCommand));

	size_t fullSize = commandSize + sizeof(teleport::core::InputDefinitionNetPacket) * setupInputsCommand.numInputs;
	if (packet.size() < fullSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet size");
		return;
	}
	inputDefinitions.resize(setupInputsCommand.numInputs);
	const unsigned char *ptr = packet.data() + sizeof(teleport::core::SetupInputsCommand);
	for (int i = 0; i < setupInputsCommand.numInputs; i++)
	{
		if (size_t(ptr - packet.data()) >= packet.size())
		{
			TELEPORT_INTERNAL_CERR("Bad packet");
			return;
		}
		auto									 &def		= inputDefinitions[i];
		teleport::core::InputDefinitionNetPacket &packetDef = *((teleport::core::InputDefinitionNetPacket *)ptr);
		ptr += sizeof(teleport::core::InputDefinitionNetPacket);
		if (size_t(ptr + packetDef.pathLength - packet.data()) > packet.size())
		{
			TELEPORT_INTERNAL_CERR("Bad packet");
			return;
		}
		def.inputId	  = packetDef.inputId;
		def.inputType = packetDef.inputType;
		def.regexPath.resize(packetDef.pathLength);
		memcpy(def.regexPath.data(), ptr, packetDef.pathLength);
		ptr += packetDef.pathLength;
	}
	if (size_t(ptr - packet.data()) != packet.size())
	{
		TELEPORT_INTERNAL_CERR("Bad packet");
		return;
	}
	// Now process the input definitions according to the available hardware:
	mCommandInterface->OnInputsSetupChanged(inputDefinitions);
}

void SessionClient::ReceiveUpdateNodeStructureCommand(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(teleport::core::UpdateNodeStructureCommand);
	// Copy command out of packet.
	teleport::core::UpdateNodeStructureCommand updateNodeStructureCommand;
	memcpy(static_cast<void *>(&updateNodeStructureCommand), packet.data(), commandSize);
	mCommandInterface->UpdateNodeStructure(updateNodeStructureCommand);

	ConfirmOrthogonalStateToClient(updateNodeStructureCommand.confirmationNumber);
}

void SessionClient::ConfirmOrthogonalStateToClient(uint64_t confNumber)
{
	// TODO: use reliable channel for this.
	teleport::core::OrthogonalAcknowledgementMessage msg(confNumber);
	SendMessageToServer(&msg, sizeof(msg));
}

void SessionClient::ReceiveAssignNodePosePathCommand(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(teleport::core::AssignNodePosePathCommand);
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet.");
		return;
	}
	// Copy command out of packet.
	teleport::core::AssignNodePosePathCommand assignNodePosePathCommand;
	memcpy(static_cast<void *>(&assignNodePosePathCommand), packet.data(), commandSize);
	if (packet.size() != commandSize + assignNodePosePathCommand.pathLength)
	{
		TELEPORT_INTERNAL_CERR("Bad packet.");
		return;
	}
	std::string str;
	str.resize(assignNodePosePathCommand.pathLength);
	memcpy(static_cast<void *>(str.data()), packet.data() + commandSize, assignNodePosePathCommand.pathLength);
	nodePosePaths[assignNodePosePathCommand.nodeID] = str;
	TELEPORT_INTERNAL_COUT("Received pose for node {0}: {1}", assignNodePosePathCommand.nodeID, str);
	mCommandInterface->AssignNodePosePath(assignNodePosePathCommand, str);
}

void SessionClient::ReceivePingForLatencyCommand(const std::vector<uint8_t> &packet)
{
	teleport::core::PingForLatencyCommand cmd;
	size_t								  commandSize = sizeof(cmd);
	if (packet.size() != commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet.");
		return;
	}
	memcpy(static_cast<void *>(&cmd), packet.data(), commandSize);
	int64_t unix_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	int64_t diff_us		 = unix_time_us - cmd.unix_time_us;
	latency_milliseconds = float(double(diff_us) * 0.001);

	core::PongForLatencyMessage pongForLatencyMessage;
	pongForLatencyMessage.unix_time_us				  = unix_time_us;
	pongForLatencyMessage.server_to_client_latency_us = diff_us;
	SendMessageToServer(&pongForLatencyMessage, sizeof(pongForLatencyMessage));
}

float SessionClient::GetLatencyMs() const
{
	return latency_milliseconds;
}

void SessionClient::ReceiveTextCommand(const std::vector<uint8_t> &packet)
{
	size_t commandSize = sizeof(uint16_t);
	if (packet.size() < commandSize)
	{
		TELEPORT_INTERNAL_CERR("Bad packet.");
		return;
	}
	// Copy command out of packet.
	uint16_t count = 0;
	memcpy(static_cast<void *>(&count), packet.data(), commandSize);
	if (packet.size() != commandSize + count)
	{
		TELEPORT_INTERNAL_CERR("Bad packet.");
		return;
	}
	std::string str;
	str.resize(count);
	memcpy(static_cast<void *>(str.data()), packet.data() + commandSize, count);
	mCommandInterface->OnStreamingControlMessage(str);
	if (clientPipeline.source)
		clientPipeline.source->receiveStreamingControlMessage(str);
}

//! Reset the session state when connecting to a new server, or when reconnecting without preserving the session:
void SessionClient::ResetSessionState()
{
	memset(&setupCommand, 0, sizeof(setupCommand));
	memset(&clientDynamicLighting, 0, sizeof(clientDynamicLighting));
	inputDefinitions.clear();
	nodePosePaths.clear();
}

void SessionClient::TearDownStreamingPipeline()
{
	if (clientPipeline.source)
	{
		clientPipeline.pipeline.deconfigure();
		clientPipeline.source->deconfigure();
	}
	lastStreamingState = avs::StreamingConnectionState::UNINITIALIZED;
}

void SessionClient::BeginReconnect()
{
	auto &config = Config::GetInstance();
	// If reconnection is disabled, just give up immediately.
	if (config.options.maxReconnectAttempts == 0)
	{
		GiveUpAndShutDown();
		return;
	}
	TearDownStreamingPipeline();
	// Note: we deliberately do NOT call DiscoveryService::Disconnect or
	// SignalingServer::QueueDisconnectionMessage here. The server must keep our
	// previous clientID/serverID associated with this session so that the next
	// connect-response can be matched as a continuation (see SignalingServer
	// connect-response handling).
	connectionStatus		  = ConnectionStatus::RECONNECTING;
	reconnectAttempts		  = 0;
	currentReconnectBackoffMs = config.options.reconnectInitialBackoffMs;
	const auto now			  = std::chrono::steady_clock::now();
	nextReconnectTime		  = now + std::chrono::milliseconds(currentReconnectBackoffMs);
	reconnectAttemptDeadline  = nextReconnectTime + std::chrono::milliseconds(config.options.connectionTimeout);
	TELEPORT_INTERNAL_COUT("Reconnecting to {0}; first attempt in {1} ms.", GetServerURL(), currentReconnectBackoffMs);
}

void SessionClient::GiveUpAndShutDown()
{
	TELEPORT_WARN("Giving up on connection to {0}.", GetServerURL());
	TearDownStreamingPipeline();
	if (mCommandInterface)
	{
		mCommandInterface->OnReconnectGaveUp();
	}
	if (geometryCache)
	{
		geometryCache->ClearAll();
	}
	// Now perform a clean disconnect: this resets clientID and notifies the server.
	Disconnect(0, true);
	reconnectAttempts		  = 0;
	currentReconnectBackoffMs = 0;
	lastSessionId			  = 0;
}