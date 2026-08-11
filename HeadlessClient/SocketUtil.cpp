#include "SocketUtil.h"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <cerrno>
	#include <cstring>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#include <mutex>

namespace teleport_control
{
#ifdef _WIN32
	using native_socket_t = SOCKET;
	static std::once_flag wsaOnce;
#else
	using native_socket_t = int;
#endif

	static native_socket_t Native(socket_t sock)
	{
		return static_cast<native_socket_t>(sock);
	}

	bool SocketStartup()
	{
#ifdef _WIN32
		static bool ok = false;
		std::call_once(wsaOnce, []() {
			WSADATA wsaData;
			ok = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
		});
		return ok;
#else
		return true;
#endif
	}

	void SocketCleanup()
	{
#ifdef _WIN32
		WSACleanup();
#endif
	}

	std::string LastSocketError()
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		char *msg = nullptr;
		DWORD len = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
		std::string out;
		if (len && msg)
		{
			out.assign(msg, len);
			LocalFree(msg);
			// Strip the trailing CRLF FormatMessage appends.
			while (!out.empty() && (out.back() == '\r' || out.back() == '\n'))
				out.pop_back();
		}
		else
		{
			out = "socket error " + std::to_string(err);
		}
		return out;
#else
		return std::strerror(errno);
#endif
	}

	//! macOS has no MSG_NOSIGNAL; per-socket SO_NOSIGPIPE keeps a closed peer from
	//! killing the process. No-op elsewhere.
	static void SetNoSigpipe(socket_t sock)
	{
#if defined(__APPLE__)
		int one = 1;
		setsockopt(Native(sock), SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
		(void)sock;
#endif
	}

	static int SendFlags()
	{
#if defined(_WIN32) || defined(__APPLE__)
		return 0;
#else
		return MSG_NOSIGNAL;
#endif
	}

	socket_t ListenLoopback(uint16_t port, int backlog)
	{
		socket_t sock = static_cast<socket_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
		if (sock == INVALID_SOCK)
			return INVALID_SOCK;

		int one = 1;
		setsockopt(Native(sock), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one));
		SetNoSigpipe(sock);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		if (bind(Native(sock), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
			listen(Native(sock), backlog) != 0)
		{
			CloseSocket(sock);
			return INVALID_SOCK;
		}
		return sock;
	}

	socket_t Accept(socket_t listenSocket)
	{
		socket_t sock = static_cast<socket_t>(accept(Native(listenSocket), nullptr, nullptr));
		if (sock != INVALID_SOCK)
			SetNoSigpipe(sock);
		return sock;
	}

	socket_t ConnectLoopback(uint16_t port)
	{
		socket_t sock = static_cast<socket_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
		if (sock == INVALID_SOCK)
			return INVALID_SOCK;
		SetNoSigpipe(sock);

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

		if (connect(Native(sock), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
		{
			CloseSocket(sock);
			return INVALID_SOCK;
		}
		return sock;
	}

	socket_t ConnectTcp(const std::string &host, uint16_t port)
	{
		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo *results = nullptr;
		std::string portStr = std::to_string(port);
		if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &results) != 0)
			return INVALID_SOCK;

		socket_t sock = INVALID_SOCK;
		for (addrinfo *ai = results; ai; ai = ai->ai_next)
		{
			sock = static_cast<socket_t>(socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
			if (sock == INVALID_SOCK)
				continue;
			if (connect(Native(sock), ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0)
			{
				SetNoSigpipe(sock);
				break;
			}
			CloseSocket(sock);
			sock = INVALID_SOCK;
		}
		freeaddrinfo(results);
		return sock;
	}

	void CloseSocket(socket_t sock)
	{
		if (sock == INVALID_SOCK)
			return;
#ifdef _WIN32
		closesocket(static_cast<SOCKET>(sock));
#else
		close(Native(sock));
#endif
	}

	void ShutdownSocket(socket_t sock)
	{
		if (sock == INVALID_SOCK)
			return;
#ifdef _WIN32
		shutdown(Native(sock), SD_BOTH);
#else
		shutdown(Native(sock), SHUT_RDWR);
#endif
	}

	bool SendAll(socket_t sock, const char *data, size_t size)
	{
		size_t sent = 0;
		while (sent < size)
		{
			int n = send(Native(sock), data + sent, static_cast<int>(size - sent), SendFlags());
			if (n <= 0)
				return false;
			sent += static_cast<size_t>(n);
		}
		return true;
	}

	bool SendAll(socket_t sock, const std::string &text)
	{
		return SendAll(sock, text.data(), text.size());
	}

	SocketLineReader::SocketLineReader(socket_t sock)
		: sock(sock)
	{
	}

	bool SocketLineReader::ReadLine(std::string &line)
	{
		for (;;)
		{
			size_t nl = buffer.find('\n');
			if (nl != std::string::npos)
			{
				line = buffer.substr(0, nl);
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				buffer.erase(0, nl + 1);
				return true;
			}
			char chunk[4096];
			int n = recv(Native(sock), chunk, sizeof(chunk), 0);
			if (n <= 0)
			{
				// EOF or error: flush a partial final line if there is one.
				if (!buffer.empty())
				{
					line = buffer;
					if (!line.empty() && line.back() == '\r')
						line.pop_back();
					buffer.clear();
					return true;
				}
				return false;
			}
			buffer.append(chunk, static_cast<size_t>(n));
		}
	}
} // namespace teleport_control
