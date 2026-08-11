// Behavioural tests for CommandProcessor: the command dispatcher shared by every
// control-socket connection (see docs/protocol/local_control.rst). Runs against a
// real ConnectionManager with no connections ever created, so nothing here touches
// the network. The "connect" verb itself kicks off a real (asynchronous) signalling
// attempt via HeadlessConnection/TabContext and is exercised instead by the
// integration test, test/control_integration.sh.

#include <catch2/catch_test_macros.hpp>

#include "CommandProcessor.h"
#include "ConnectionManager.h"
#include "ReplCommandParser.h"

namespace
{
	// Splits a response payload into its OK/ERROR status line and the rest.
	struct Response
	{
		std::string status;
		std::string body;
	};

	Response Split(const std::string &payload)
	{
		size_t nl = payload.find('\n');
		if (nl == std::string::npos)
			return {payload, ""};
		return {payload.substr(0, nl), payload.substr(nl + 1)};
	}

	Response Run(CommandProcessor &processor, ControlSessionState &state, const std::string &line)
	{
		ReplCommandParser parser;
		return Split(processor.Execute(parser.Parse(line), state));
	}
} // namespace

TEST_CASE("CommandProcessor: liveness and info verbs need no selected connection", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "ping").body == "pong\n");
	CHECK(Run(processor, state, "version").status == "OK");
	CHECK(Run(processor, state, "help").status == "OK");
}

TEST_CASE("CommandProcessor: empty and unknown commands are errors", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;
	ReplCommandParser parser;

	CHECK(processor.Execute(parser.Parse(""), state).rfind("ERROR", 0) == 0);
	CHECK(Run(processor, state, "frobnicate").status == "ERROR unknown command: frobnicate");
}

TEST_CASE("CommandProcessor: commands needing a selected connection fail cleanly when none is selected",
	"[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	for (const std::string &line : {"status", "move 1 2 3", "turn 0 0 0 1", "input list", "mode minimal", "geometry"})
	{
		INFO("command: " << line);
		CHECK(Run(processor, state, line).status == "ERROR no connection selected (connect first, or 'use <id>')");
	}
}

TEST_CASE("CommandProcessor: move/turn/mode/input validate their arguments before touching a connection",
	"[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "move 1 2").status == "ERROR usage: move <x> <y> <z>");
	CHECK(Run(processor, state, "turn 0 0 0").status == "ERROR usage: turn <qx> <qy> <qz> <qw>");
	CHECK(Run(processor, state, "mode").status == "ERROR usage: mode <minimal|simulated>");
	CHECK(Run(processor, state, "mode sideways").status == "ERROR unknown mode: sideways");
	CHECK(Run(processor, state, "input").status == "ERROR usage: input <list|binary|analogue|motion> ...");
}

TEST_CASE("CommandProcessor: connections/list report an empty manager", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "connections").body == "no connections\n");
	CHECK(Run(processor, state, "list").body == "no connections\n");
}

TEST_CASE("CommandProcessor: use/disconnect reject ids that don't exist", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "use").status == "ERROR usage: use <id>");
	CHECK(Run(processor, state, "use abc").status == "ERROR invalid connection id: abc");
	CHECK(Run(processor, state, "use 7").status == "ERROR no connection with id 7");

	CHECK(Run(processor, state, "disconnect").status == "ERROR no connection selected (disconnect [id])");
	CHECK(Run(processor, state, "disconnect abc").status == "ERROR invalid connection id: abc");
	CHECK(Run(processor, state, "disconnect 7").status == "ERROR no connection with id 7");
}

TEST_CASE("CommandProcessor: shutdown flags the session without closing it", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "shutdown").status == "OK");
	CHECK(state.shutdownRequested);
	CHECK_FALSE(state.closeAfterResponse);
}

TEST_CASE("CommandProcessor: quit/exit close this session but never request shutdown", "[command-processor]")
{
	for (const std::string &verb : {"quit", "exit"})
	{
		INFO("verb: " << verb);
		ConnectionManager manager;
		CommandProcessor processor(manager);
		ControlSessionState state;

		CHECK(Run(processor, state, verb).status == "OK");
		CHECK(state.closeAfterResponse);
		CHECK_FALSE(state.shutdownRequested);
	}
}

TEST_CASE("CommandProcessor: identity and signout are safe with no providers registered", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "identity").status == "OK");
	CHECK(Run(processor, state, "signout").body == "signed out\n");
	CHECK(Run(processor, state, "signin").status == "ERROR no identity providers are available in this build");
}
