#include "ControlServer.h"
#include "ControlProtocol.h"
#include "TeleportCore/Logging.h"

using namespace teleport_control;

ControlServer::ControlServer(ConnectionManager &manager)
	: manager(manager)
	, processor(manager)
{
}

ControlServer::~ControlServer()
{
	Stop();
}

bool ControlServer::Start(uint16_t port)
{
	if (!SocketStartup())
		return false;
	listenSocket = ListenLoopback(port);
	if (listenSocket == INVALID_SOCK)
		return false;
	this->port = port;
	running = true;
	acceptThread = std::thread(&ControlServer::AcceptLoop, this);
	return true;
}

void ControlServer::Stop()
{
	if (!running.exchange(false))
		return;

	// Wake accept() with a throwaway loopback connection so shutdown is prompt;
	// closing a listen socket from another thread does not reliably interrupt
	// a blocked accept on all platforms.
	socket_t wake = ConnectLoopback(port);
	CloseSocket(wake);
	if (acceptThread.joinable())
		acceptThread.join();
	CloseSocket(listenSocket);
	listenSocket = INVALID_SOCK;

	// Nudge every attached client out of recv; each handler closes its own socket.
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		for (socket_t s : clientSockets)
			ShutdownSocket(s);
	}
	for (int i = 0; i < 500 && activeClients.load() > 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	if (activeClients.load() > 0)
		TELEPORT_WARN("ControlServer stopping with {} client(s) still attached", activeClients.load());
}

void ControlServer::AcceptLoop()
{
	while (running)
	{
		socket_t client = Accept(listenSocket);
		if (client == INVALID_SOCK)
			break; // Listen socket closed during Stop().
		if (!running)
		{
			// The wake-up self-connect from Stop(): drop it and get out.
			CloseSocket(client);
			break;
		}
		{
			std::lock_guard<std::mutex> lock(clientsMutex);
			clientSockets.insert(client);
		}
		activeClients++;
		std::thread(&ControlServer::ClientLoop, this, client).detach();
	}
}

void ControlServer::ClientLoop(socket_t sock)
{
	ControlSessionState state;
	SocketLineReader reader(sock);
	ReplCommandParser parser;

	std::string line;
	while (running && reader.ReadLine(line))
	{
		auto cmd = parser.Parse(line);
		std::string payload = processor.Execute(cmd, state);
		if (state.shutdownRequested)
			shutdownRequested = true;
		if (!SendAll(sock, FrameResponse(payload)))
			break;
		if (state.closeAfterResponse)
			break;
	}

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		clientSockets.erase(sock);
	}
	CloseSocket(sock);
	activeClients--;
}
