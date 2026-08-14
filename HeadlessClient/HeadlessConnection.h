#pragma once

#include "ConnectionReport.h"
#include "TeleportClient/TabContext.h"
#include "TeleportClient/SessionClient.h"
#include "HeadlessSessionCommandInterface.h"
#include "HeadlessGeometryCacheBackend.h"
#include "HeadlessInputState.h"
#include <memory>
#include <string>

//! One streaming connection to one Teleport server. Formerly the monolithic
//! HeadlessClient; now owned in multiples by ConnectionManager, one instance per
//! open connection. Access from any thread is serialised by the manager's mutex.
class HeadlessConnection
{
public:
	HeadlessConnection();
	~HeadlessConnection();

	bool Connect(const std::string &url);
	void Disconnect();

	void TickOnce(double time, double dt);

	bool IsConnected() const;
	const std::string &GetUrl() const { return url; }

	//! State of the underlying session. Rendering - prose or JSON - is the caller's
	//! business; see ConnectionReport.h.
	ConnectionStatus GetStatusData() const;

	//! Everything the server has streamed to us. Which sections a caller renders is
	//! decided by the `geometry` command's argument, not here.
	GeometryReport GetGeometryData() const;

	void SetMode(HeadlessMode mode)
	{
		currentMode = mode;
		if (commandInterface)
			commandInterface->SetMode(mode);
	}
	HeadlessMode GetMode() const { return currentMode; }

	HeadlessInputState &GetInputState() { return inputState; }
	const std::vector<teleport::core::InputDefinition> &GetInputDefinitions() const;

	void SendBinaryInput(avs::uid id, uint8_t value);
	void SendAnalogueInput(avs::uid id, float value);
	void SendMotionInput(avs::uid id, float x, float y);

private:
	void ProcessVideo();

	HeadlessMode currentMode = HeadlessMode::Minimal;
	std::string url;
	teleport::client::TabContext tabContext;
	std::shared_ptr<teleport::client::SessionClient> sessionClient;
	//! Uid that sessionClient was resolved from, so we notice when TabContext promotes
	//! next_server_uid to server_uid on connection completion.
	avs::uid activeServerUid = 0;
	std::unique_ptr<HeadlessSessionCommandInterface> commandInterface;
	std::unique_ptr<HeadlessGeometryCacheBackend> geometryBackend;
	HeadlessInputState inputState;

	double lastUpdateTime = 0.0;
};
