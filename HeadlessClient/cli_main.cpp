//! teleport_cli — POSIX-style command-line front end for the teleport service.
//!
//! Sends one-line commands to the service over the local control protocol and
//! prints the responses. Exiting the CLI never disconnects a streaming session.
//!
//! Usage (POSIX.1 utility conventions — single-letter options only):
//!   teleport_cli [-h host] [-p port] [-?] [-e commands] [command ...]
//!
//!   -h <host>   Service host (default 127.0.0.1)
//!   -p <port>   Service port (default 10510, or TELEPORT_SERVICE_PORT)
//!   -e <cmds>   Execute ';'-separated commands, then exit
//!   -?          This help
//!   --          End of options; remaining operands are commands
//!
//! With no -e and no operands, commands are read from stdin: an interactive
//! prompt when stdin is a terminal, batch mode when piped.
//!
//! Exit status: 0 = all commands OK, 1 = a command returned ERROR, 2 = usage
//! error or the service is unreachable.

#include "ControlProtocol.h"
#include "SocketUtil.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
	#include <io.h>
#else
	#include <unistd.h>
#endif

using namespace teleport_control;

namespace
{
	bool StdinIsTty()
	{
#ifdef _WIN32
		return _isatty(_fileno(stdin)) != 0;
#else
		return isatty(fileno(stdin)) != 0;
#endif
	}

	void PrintUsage(const char *argv0)
	{
		std::fprintf(stderr,
			"Usage: %s [-h host] [-p port] [-?] [-e commands] [command ...]\n"
			"Send one-line commands to a running teleport service (teleport_terminal).\n"
			"  -h <host>   Service host (default 127.0.0.1)\n"
			"  -p <port>   Service port (default %u, or TELEPORT_SERVICE_PORT)\n"
			"  -e <cmds>   Execute ';'-separated commands, then exit\n"
			"  -?          This help\n",
			argv0, static_cast<unsigned>(DEFAULT_PORT));
	}

	struct CliOptions
	{
		std::string host = "127.0.0.1";
		uint16_t port = DEFAULT_PORT;
		bool usageRequested = false;
		bool parseError = false;
		std::vector<std::string> commands; //! From -e and operands; empty = read stdin.
	};

	void SplitCommands(const std::string &text, char delimiter, std::vector<std::string> &out)
	{
		size_t start = 0;
		while (start <= text.size())
		{
			size_t end = text.find(delimiter, start);
			if (end == std::string::npos)
				end = text.size();
			std::string part = text.substr(start, end - start);
			// Trim surrounding whitespace.
			size_t first = part.find_first_not_of(" \t\r\n");
			if (first != std::string::npos)
			{
				size_t last = part.find_last_not_of(" \t\r\n");
				out.push_back(part.substr(first, last - first + 1));
			}
			if (end == text.size())
				break;
			start = end + 1;
		}
	}

	//! POSIX.1-style option scan: single-letter options, option-arguments attached
	//! or separate, "--" ends options, remaining operands are commands.
	CliOptions ParseArgs(int argc, char *argv[])
	{
		CliOptions opts;
		if (const char *envPort = std::getenv("TELEPORT_SERVICE_PORT"))
		{
			if (int p = std::atoi(envPort); p > 0 && p < 65536)
				opts.port = static_cast<uint16_t>(p);
		}

		bool noMoreOptions = false;
		for (int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];
			if (noMoreOptions || arg.size() < 2 || arg[0] != '-')
			{
				opts.commands.push_back(arg);
				continue;
			}
			if (arg == "--")
			{
				noMoreOptions = true;
				continue;
			}
			for (size_t j = 1; j < arg.size(); j++)
			{
				char c = arg[j];
				if (c == '?')
				{
					opts.usageRequested = true;
					continue;
				}
				if (c == 'h' || c == 'p' || c == 'e')
				{
					std::string value;
					if (j + 1 < arg.size())
					{
						value = arg.substr(j + 1);
					}
					else if (i + 1 < argc)
					{
						value = argv[++i];
					}
					else
					{
						std::fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], c);
						opts.parseError = true;
						return opts;
					}
					if (c == 'h')
						opts.host = value;
					else if (c == 'p')
						opts.port = static_cast<uint16_t>(std::atoi(value.c_str()));
					else
						SplitCommands(value, ';', opts.commands);
					break; // Option-argument consumed the rest of this argv element.
				}
				std::fprintf(stderr, "%s: unknown option -- %c\n", argv[0], c);
				opts.parseError = true;
				return opts;
			}
		}
		return opts;
	}

	//! True if the line is a local quit/exit request.
	bool IsQuit(const std::string &line)
	{
		return line == "quit" || line == "exit";
	}

	//! Send one command and print its framed response. Returns false if the
	//! connection broke. Sets commandError if the service answered ERROR.
	bool RunCommand(socket_t sock, SocketLineReader &reader, const std::string &line, bool &commandError)
	{
		if (!SendAll(sock, line + "\n"))
			return false;

		std::vector<std::string> payload;
		std::string wireLine;
		for (;;)
		{
			if (!reader.ReadLine(wireLine))
				return false;
			if (wireLine.size() == 1 && wireLine[0] == TERMINATOR)
				break;
			payload.push_back(UnstuffLine(wireLine));
		}

		bool ok = !payload.empty() && payload[0] == STATUS_OK;
		std::ostream &out = ok ? std::cout : std::cerr;
		size_t first = payload.empty() ? 0 : 1;
		for (size_t i = first; i < payload.size(); i++)
			out << payload[i] << "\n";
		if (!ok)
		{
			if (!payload.empty() && payload[0].rfind(STATUS_ERROR, 0) == 0)
				std::cerr << payload[0] << "\n";
			else
				std::cerr << "ERROR (malformed response from service)\n";
			commandError = true;
		}
		return true;
	}
} // namespace

int main(int argc, char *argv[])
{
	CliOptions opts = ParseArgs(argc, argv);
	if (opts.usageRequested)
	{
		PrintUsage(argv[0]);
		return opts.parseError ? 2 : 0;
	}
	if (opts.parseError)
	{
		PrintUsage(argv[0]);
		return 2;
	}
	if (opts.port == 0)
	{
		std::fprintf(stderr, "%s: invalid port\n", argv[0]);
		return 2;
	}

	if (!SocketStartup())
	{
		std::fprintf(stderr, "%s: could not initialise sockets\n", argv[0]);
		return 2;
	}

	socket_t sock = ConnectTcp(opts.host, opts.port);
	if (sock == INVALID_SOCK)
	{
		std::fprintf(stderr, "%s: teleport service not running on %s:%u — start teleport_terminal first (%s)\n",
			argv[0], opts.host.c_str(), static_cast<unsigned>(opts.port), LastSocketError().c_str());
		return 2;
	}

	SocketLineReader reader(sock);
	bool commandError = false;
	bool connectionOk = true;

	auto runLine = [&](const std::string &line) -> bool {
		// Returns false when processing should stop (quit, or broken connection).
		if (line.empty())
			return true;
		if (IsQuit(line))
			return false;
		connectionOk = RunCommand(sock, reader, line, commandError);
		if (!connectionOk)
			std::fprintf(stderr, "%s: lost connection to the service\n", argv[0]);
		return connectionOk;
	};

	if (!opts.commands.empty())
	{
		for (const std::string &cmd : opts.commands)
		{
			if (!runLine(cmd))
				break;
		}
	}
	else
	{
		bool interactive = StdinIsTty();
		if (interactive)
			std::cout << "Teleport CLI — 'help' for commands, 'quit' to exit (streams keep running)\n";
		std::string line;
		bool keepGoing = true;
		while (keepGoing)
		{
			if (interactive)
			{
				std::cout << "teleport> " << std::flush;
			}
			if (!std::getline(std::cin, line))
				break;
			// Trim CR/whitespace edges.
			size_t first = line.find_first_not_of(" \t\r");
			if (first == std::string::npos)
				continue;
			size_t last = line.find_last_not_of(" \t\r");
			keepGoing = runLine(line.substr(first, last - first + 1));
		}
	}

	CloseSocket(sock);
	SocketCleanup();

	if (!connectionOk)
		return 2;
	return commandError ? 1 : 0;
}
