// Behavioural tests for CommandProcessor: the command dispatcher shared by every
// control-socket connection (see docs/protocol/local_control.rst). Runs against a
// real ConnectionManager with no connections ever created, so nothing here touches
// the network. The "connect" verb itself kicks off a real (asynchronous) signalling
// attempt via HeadlessConnection/TabContext and is exercised instead by the
// integration test, test/control_integration.sh.

#include <catch2/catch_test_macros.hpp>

#include "CommandProcessor.h"
#include "ConnectionManager.h"
#include "ControlProtocol.h"
#include "ReplCommandParser.h"

namespace
{
	// A CommandResult flattened into what each rendering puts on the wire: the status
	// header line, the prose body, and the machine-readable object.
	struct Response
	{
		std::string	   status;
		std::string	   body;
		nlohmann::json data;
	};

	Response Run(CommandProcessor &processor, ControlSessionState &state, const std::string &line)
	{
		ReplCommandParser parser;
		CommandResult	  result = processor.Execute(parser.Parse(line), state);
		std::string		  status = result.ok ? std::string(teleport_control::STATUS_OK) : teleport_control::STATUS_ERROR + result.error;
		return {status, result.text, result.data};
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

	CHECK_FALSE(processor.Execute(parser.Parse(""), state).ok);
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

// The JSON rendering is a published contract (docs/protocol/local_control.rst) that the
// MCP server in teleport-mcp/ codes against, so its shape is asserted here rather than
// left to whatever the prose happened to say.

TEST_CASE("CommandProcessor: format switches the session's rendering and validates its argument", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK_FALSE(state.jsonOutput);
	CHECK(Run(processor, state, "format").status == "ERROR usage: format <text|json>");
	CHECK(Run(processor, state, "format yaml").status == "ERROR unknown format: yaml");
	CHECK_FALSE(state.jsonOutput);

	Response toJson = Run(processor, state, "format json");
	CHECK(toJson.status == "OK");
	CHECK(toJson.data["format"] == "json");
	CHECK(state.jsonOutput);

	CHECK(Run(processor, state, "format text").data["format"] == "text");
	CHECK_FALSE(state.jsonOutput);
}

TEST_CASE("CommandProcessor: every result carries both renderings", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	// A verb that fills only one of text/data is the failure this guards against.
	for (const std::string &line : {"ping", "version", "help", "connections", "identity", "format text"})
	{
		INFO("command: " << line);
		Response r = Run(processor, state, line);
		CHECK(r.status == "OK");
		CHECK_FALSE(r.body.empty());
		CHECK(r.data.is_object());
		CHECK_FALSE(r.data.empty());
	}
}

TEST_CASE("CommandProcessor: errors carry the message in data as well as the status line", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	Response r = Run(processor, state, "frobnicate");
	CHECK(r.status == "ERROR unknown command: frobnicate");
	CHECK(r.data["error"] == "unknown command: frobnicate");
}

TEST_CASE("CommandProcessor: info verbs match the documented schema", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	CHECK(Run(processor, state, "ping").data["pong"] == true);

	nlohmann::json version = Run(processor, state, "version").data;
	CHECK(version["service"] == "teleportd");
	CHECK(version["protocol"].is_number_integer());

	nlohmann::json connections = Run(processor, state, "connections").data;
	CHECK(connections["selected"] == 0);
	CHECK(connections["connections"].is_array());
	CHECK(connections["connections"].empty());

	nlohmann::json identity = Run(processor, state, "identity").data;
	CHECK(identity["signedIn"].is_boolean());
	CHECK(identity["providers"].is_array());
	// Null rather than absent, so a client can distinguish "no sign-in pending" from
	// a field this build forgot to send.
	CHECK(identity["pendingSignIn"].is_null());

	nlohmann::json help = Run(processor, state, "help").data;
	REQUIRE(help["commands"].is_array());
	CHECK_FALSE(help["commands"].empty());
	for (const auto &entry : help["commands"])
	{
		CHECK(entry["verb"].is_string());
		CHECK(entry["usage"].is_string());
		CHECK(entry["summary"].is_string());
		CHECK(entry["aliases"].is_array());
	}
}

TEST_CASE("CommandProcessor: the help text and the help data come from one table", "[command-processor]")
{
	ConnectionManager manager;
	CommandProcessor processor(manager);
	ControlSessionState state;

	Response help = Run(processor, state, "help");
	// The alignment rule: usages narrower than the column are padded, wider ones get a
	// single space. Both cases, so a change to either stops here rather than in a diff
	// nobody reads.
	CHECK(help.body.find("  connect <host[:port]>  - Connect to a server") != std::string::npos);
	CHECK(help.body.find("  turn <qx> <qy> <qz> <qw> - Set avatar orientation") != std::string::npos);

	for (const auto &entry : help.data["commands"])
	{
		INFO("usage: " << entry["usage"].get<std::string>());
		CHECK(help.body.find("  " + entry["usage"].get<std::string>()) != std::string::npos);
		CHECK(help.body.find("- " + entry["summary"].get<std::string>()) != std::string::npos);
	}
}
