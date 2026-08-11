#include "HeadlessConnection.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include <libavstream/queue.hpp>

HeadlessConnection::HeadlessConnection()
	: geometryBackend(std::make_unique<HeadlessGeometryCacheBackend>())
{
	// geometryBackend must exist before the command interface, which holds a pointer to it.
	commandInterface = std::make_unique<HeadlessSessionCommandInterface>(nullptr, currentMode, geometryBackend.get());
}

HeadlessConnection::~HeadlessConnection()
{
	Disconnect();
}

bool HeadlessConnection::Connect(const std::string &url)
{
	if (sessionClient && sessionClient->IsConnected())
	{
		TELEPORT_WARN("Already connected");
		return false;
	}

	try
	{
		tabContext.ConnectTo(url);
		this->url = url;
		TELEPORT_LOG("Initiated connection to {}", url);
		return true;
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("Connection failed: {}", e.what());
		return false;
	}
}

void HeadlessConnection::Disconnect()
{
	if (sessionClient)
	{
		sessionClient->Disconnect(5000, true);
		sessionClient = nullptr;
	}
	// A connection still in progress lives under the tab's "next" uid, which the line above
	// would miss; cancel through the tab context so a pending attempt is dropped too.
	tabContext.CancelConnection();
	activeServerUid = 0;
	url.clear();
}

void HeadlessConnection::TickOnce(double time, double dt)
{
	// Note: identity.Update() is deliberately NOT here — identity is process-global, so the
	// service's main loop applies it once per tick rather than once per connection.
	inputState.UpdateTime(time);

	// A connection in progress lives under the tab's "next" uid; TabContext only promotes it to
	// the main server uid once ConnectionComplete() fires. We must therefore re-resolve every tick
	// and drive whichever client is live, or the pending connection is never ticked and can never
	// complete. Note GetSessionClient(0) would auto-create a domainless placeholder, so never ask
	// for uid 0 here.
	avs::uid activeUid = tabContext.GetServerUid();
	if (!activeUid)
		activeUid = tabContext.GetNextServerUid();

	if (!activeUid)
	{
		sessionClient	= nullptr;
		activeServerUid = 0;
		return;
	}

	if (!sessionClient || activeServerUid != activeUid)
	{
		sessionClient = teleport::client::SessionClient::GetSessionClient(activeUid);
		if (!sessionClient)
			return;
		activeServerUid	 = activeUid;
		commandInterface = std::make_unique<HeadlessSessionCommandInterface>(sessionClient, currentMode, geometryBackend.get(), activeUid);
		sessionClient->SetSessionCommandInterface(commandInterface.get());
		sessionClient->SetGeometryCache(geometryBackend.get());
	}

	// Handle connections (e.g., WebSocket signaling)
	sessionClient->HandleConnections();

	// Drain video queue if connected (stats accumulation without actual decode)
	ProcessVideo();

	// If connected, send pose/input
	if (sessionClient->IsConnected())
	{
		auto snapshot = inputState.GetSnapshot();
		sessionClient->Frame(
			snapshot.displayInfo,
			snapshot.headPose,
			snapshot.controllerPoses,
			snapshot.originValidCounter,
			snapshot.input,
			snapshot.time,
			dt
		);
	}
}

bool HeadlessConnection::IsConnected() const
{
	return sessionClient && sessionClient->IsConnected();
}

ConnectionStatus HeadlessConnection::GetStatusData() const
{
	ConnectionStatus out;
	if (!sessionClient)
	{
		out.state  = "DISCONNECTED";
		out.detail = "no session";
		return out;
	}

	switch (sessionClient->GetConnectionStatus())
	{
	case teleport::client::ConnectionStatus::UNCONNECTED:
		out.state = "UNCONNECTED";
		break;
	case teleport::client::ConnectionStatus::OFFERING:
		out.state = "OFFERING";
		break;
	case teleport::client::ConnectionStatus::AWAITING_SETUP:
		out.state = "AWAITING_SETUP";
		break;
	case teleport::client::ConnectionStatus::HANDSHAKING:
		out.state = "HANDSHAKING";
		break;
	case teleport::client::ConnectionStatus::CONNECTED:
		out.state = "CONNECTED";
		break;
	case teleport::client::ConnectionStatus::RECONNECTING:
		out.state = "RECONNECTING";
		break;
	default:
		out.state = "UNKNOWN";
	}

	out.hasSession		= true;
	out.server			= sessionClient->GetServerIP();
	out.port			= static_cast<int>(sessionClient->GetPort());
	out.latencyMs		= static_cast<int>(sessionClient->GetLatencyMs());
	out.inputsAvailable = GetInputDefinitions().size();
	return out;
}

GeometryReport HeadlessConnection::GetGeometryData() const
{
	if (!geometryBackend)
		return GeometryReport{};
	return geometryBackend->GetReport();
}

const std::vector<teleport::core::InputDefinition> &HeadlessConnection::GetInputDefinitions() const
{
	if (sessionClient)
		return sessionClient->GetInputDefinitions();
	static std::vector<teleport::core::InputDefinition> empty;
	return empty;
}

void HeadlessConnection::ProcessVideo()
{
	if (!sessionClient)
		return;

	auto &cp = sessionClient->GetClientPipeline();
	cp.videoQueue.drop();
}

void HeadlessConnection::SendBinaryInput(avs::uid id, uint8_t value)
{
	if (!sessionClient || !sessionClient->IsConnected())
	{
		TELEPORT_WARN("Cannot send input: not connected");
		return;
	}
	inputState.AddBinaryEvent(static_cast<teleport::core::InputId>(id), value != 0);
}

void HeadlessConnection::SendAnalogueInput(avs::uid id, float value)
{
	if (!sessionClient || !sessionClient->IsConnected())
	{
		TELEPORT_WARN("Cannot send input: not connected");
		return;
	}
	inputState.AddAnalogueEvent(static_cast<teleport::core::InputId>(id), value);
}

void HeadlessConnection::SendMotionInput(avs::uid id, float x, float y)
{
	if (!sessionClient || !sessionClient->IsConnected())
	{
		TELEPORT_WARN("Cannot send input: not connected");
		return;
	}
	inputState.AddMotionEvent(static_cast<teleport::core::InputId>(id), x, y);
}
