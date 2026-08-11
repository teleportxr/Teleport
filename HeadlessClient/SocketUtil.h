#pragma once

#include <cstdint>
#include <string>

//! Minimal cross-platform TCP shim over BSD sockets / winsock2. This is the single
//! platform-split boundary for both the service's control server and teleport_cli;
//! everything above it is plain POSIX-style code. Loopback use only.
namespace teleport_control
{
#ifdef _WIN32
	using socket_t = uintptr_t;			// SOCKET
	inline constexpr socket_t INVALID_SOCK = ~static_cast<socket_t>(0); // INVALID_SOCKET
#else
	using socket_t = int;
	inline constexpr socket_t INVALID_SOCK = -1;
#endif

	//! Initialises the socket library (WSAStartup on Windows; no-op elsewhere).
	//! Safe to call more than once.
	bool SocketStartup();
	void SocketCleanup();

	//! Last socket error as a human-readable string (for diagnostics).
	std::string LastSocketError();

	//! Create a listening socket bound to the loopback interface only.
	socket_t ListenLoopback(uint16_t port, int backlog = 8);

	//! Accept one incoming connection. Returns INVALID_SOCK on error (including
	//! the listen socket being closed by another thread during shutdown).
	socket_t Accept(socket_t listenSocket);

	//! Connect to a loopback TCP port. Returns INVALID_SOCK on failure.
	socket_t ConnectLoopback(uint16_t port);

	//! Connect to an arbitrary host (name or dotted address) and port.
	//! Returns INVALID_SOCK on failure.
	socket_t ConnectTcp(const std::string &host, uint16_t port);

	void CloseSocket(socket_t sock);

	//! Shut down both directions without closing: wakes a thread blocked in recv
	//! on the peer, which then closes the socket itself. Used at service stop.
	void ShutdownSocket(socket_t sock);

	//! Send the whole buffer, retrying short writes. False on error/closed peer.
	bool SendAll(socket_t sock, const char *data, size_t size);
	bool SendAll(socket_t sock, const std::string &text);

	//! Buffered line reader over a socket: ReadLine() fills `line` with the next
	//! line without its terminator (a trailing '\r' is stripped). Returns false on
	//! EOF or error.
	class SocketLineReader
	{
	public:
		explicit SocketLineReader(socket_t sock);
		bool ReadLine(std::string &line);

	private:
		socket_t sock;
		std::string buffer;
	};
} // namespace teleport_control
