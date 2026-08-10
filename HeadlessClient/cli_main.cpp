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
//!   --          End of options
//!
//! Bare operands are joined with spaces (ssh-style) into a single command,
//! sent after any -e commands. Quote one operand per command only if you
//! want several commands.
//!
//! With no -e and no operands, commands are read from stdin: an interactive
//! prompt when stdin is a terminal, batch mode when piped.
//!
//! Exit status: 0 = all commands OK, 1 = a command returned ERROR, 2 = usage
//! error or the service is unreachable.

#include "ControlProtocol.h"
#include "SocketUtil.h"

#include <cli/cli.h>
#include <cli/clilocalsession.h>
#include <cli/volatilehistorystorage.h>
#include <cli/loopscheduler.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
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
			"Send one-line commands to a running teleport service (teleportd).\n"
			"  -h <host>   Service host (default 127.0.0.1)\n"
			"  -p <port>   Service port (default %u, or TELEPORT_SERVICE_PORT)\n"
			"  -e <cmds>   Execute ';'-separated commands, then exit\n"
			"  -?          This help\n"
			"Bare operands are joined into one command line (ssh-style).\n",
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
	//! or separate, "--" ends options. Bare operands are collected and joined
	//! (ssh-style) into a single command, appended after any -e commands.
	CliOptions ParseArgs(int argc, char *argv[])
	{
		CliOptions opts;
		std::vector<std::string> operands;
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
				operands.push_back(arg);
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
		// ssh-style: the operands are one command line, not a list of commands.
		if (!operands.empty())
		{
			std::string line;
			for (const std::string &word : operands)
			{
				if (!line.empty())
					line += " ";
				line += word;
			}
			opts.commands.push_back(line);
		}
		return opts;
	}

	//! True if the line is a local quit/exit request.
	bool IsQuit(const std::string &line)
	{
		return line == "quit" || line == "exit";
	}

	//! Send one command and print its framed response into okOut (ERROR responses
	//! into errOut). Returns false if the connection broke. Sets commandError if
	//! the service answered ERROR.
	bool RunCommand(socket_t sock, SocketLineReader &reader, const std::string &line,
		std::ostream &okOut, std::ostream &errOut, bool &commandError)
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
		std::ostream &out = ok ? okOut : errOut;
		size_t first = payload.empty() ? 0 : 1;
		for (size_t i = first; i < payload.size(); i++)
			out << payload[i] << "\n";
		if (!ok)
		{
			if (!payload.empty() && payload[0].rfind(STATUS_ERROR, 0) == 0)
				errOut << payload[0] << "\n";
			else
				errOut << "ERROR (malformed response from service)\n";
			commandError = true;
		}
		return true;
	}

	//! Where the interactive history file lives: the per-user TeleportXR storage
	//! folder, same convention as the service.
	std::string HistoryFilePath()
	{
#ifdef _WIN32
		const char *base = std::getenv("LOCALAPPDATA");
		if (!base)
			base = std::getenv("USERPROFILE");
		std::string dir = base ? std::string(base) + "/TeleportXR" : ".";
#else
		const char *base = std::getenv("HOME");
		std::string dir = base ? std::string(base) + "/.local/share/TeleportXR" : ".";
#endif
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		return dir + "/cli_history.txt";
	}

	//! Verbs registered for tab completion and the library's help listing.
	//! Everything is forwarded to the service verbatim; this list exists only
	//! so completion and help have something to show.
	const char *const KNOWN_VERBS[] = {
		"connect", "connections", "list", "use", "disconnect", "status",
		"move", "turn", "input", "mode", "geometry", "identity", "signin",
		"signout", "ping", "version", "shutdown", "quit",
	};

	//! Interactive session: line editing, history and tab completion come from
	//! the cli library; every entered line is forwarded to the service.
	//! Returns the process exit code.
	int RunInteractive(socket_t sock, const char *argv0)
	{
		SocketLineReader reader(sock);
		bool commandError = false;
		bool connectionOk = true;

		auto forward = [&](std::ostream &out, const std::string &line) {
			connectionOk = RunCommand(sock, reader, line, out, out, commandError);
			if (!connectionOk)
				out << "lost connection to the service\n";
		};

		auto rootMenu = std::make_unique<cli::Menu>("teleport");
		cli::Menu *menu = rootMenu.get();
		cli::Cli cliSession(
			std::move(rootMenu),
			std::make_unique<cli::VolatileHistoryStorage>());

		// Flat command list: no menus, each handler rebuilds the line (the wire
		// grammar is whitespace-separated, so joining is lossless) and forwards.
		// "quit" is deliberately absent: it is handled locally below.
		for (const char *verb : KNOWN_VERBS)
		{
			std::string name = verb;
			if (name == "quit")
				continue;
			menu->Insert(name, [&, name](std::ostream &out, const std::vector<std::string> &args) {
				std::string line = name;
				for (const auto &a : args)
					line += " " + a;
				forward(out, line);
			});
		}
		// Anything unrecognised (new or mistyped verbs) still reaches the
		// service, which answers with its own ERROR line.
		cliSession.WrongCommandHandler([&](std::ostream &out, const std::string &cmd) {
			forward(out, cmd);
		});

		std::cout << "Teleport CLI — 'help' for commands, 'quit' to exit (streams keep running)\n";

		cli::LoopScheduler scheduler;
		cli::CliLocalTerminalSession session(cliSession, scheduler, std::cout, 200);
		session.ExitAction([&scheduler](std::ostream &) { scheduler.Stop(); });
		// quit/exit never leave this process: the library's built-in "exit" and
		// this local "quit" end the session; streaming connections keep running.
		menu->Insert("quit", [&session](std::ostream &, const std::vector<std::string> &) {
			session.Exit();
		});

		scheduler.Run();

		if (!connectionOk)
			return 2;
		return commandError ? 1 : 0;
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
		std::fprintf(stderr, "%s: teleport daemon not running on %s:%u — start teleportd first (%s)\n",
			argv[0], opts.host.c_str(), static_cast<unsigned>(opts.port), LastSocketError().c_str());
		return 2;
	}

	if (opts.commands.empty() && StdinIsTty())
	{
		// Interactive session: line editing, history and completion via the cli
		// library. Batch modes (-e, operands, piped stdin) stay plain POSIX.
		int rc = RunInteractive(sock, argv[0]);
		CloseSocket(sock);
		SocketCleanup();
		return rc;
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
		connectionOk = RunCommand(sock, reader, line, std::cout, std::cerr, commandError);
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
		std::string line;
		bool keepGoing = true;
		while (keepGoing)
		{
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
