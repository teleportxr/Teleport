#pragma once

#include "CommandProcessor.h"
#include "ConnectionManager.h"
#include "SocketUtil.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>

//! TCP control server: listens on the loopback interface for CLI clients speaking
//! the local control protocol (see ControlProtocol.h / docs/protocol/local_control.rst).
//! One handler thread per attached client; all command execution is serialised
//! against the tick thread by ConnectionManager's mutex.
class ControlServer
{
public:
	explicit ControlServer(ConnectionManager &manager);
	~ControlServer();

	//! Bind, listen and spawn the accept thread. False if the port is unavailable.
	bool Start(uint16_t port);

	//! Stop accepting, detach all clients and join the accept thread. Idempotent.
	void Stop();

	//! True once any client has issued the `shutdown` command.
	bool ShutdownRequested() const { return shutdownRequested.load(); }

private:
	void AcceptLoop();
	void ClientLoop(teleport_control::socket_t sock);

	ConnectionManager &manager;
	CommandProcessor processor;

	uint16_t port = 0;
	teleport_control::socket_t listenSocket = teleport_control::INVALID_SOCK;
	std::atomic<bool> running{false};
	std::atomic<bool> shutdownRequested{false};

	std::thread acceptThread;
	std::mutex clientsMutex;
	std::set<teleport_control::socket_t> clientSockets;
	std::atomic<int> activeClients{0};
};
