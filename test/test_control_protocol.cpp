// Unit tests for the local control protocol framing (ControlProtocol.h) and the
// command parser shared by the teleport service and teleport_cli.
// Framing is SMTP-style dot-stuffing: a response is a block of payload lines
// terminated by a line containing a single '.'; payload lines beginning with
// '.' carry one extra leading dot on the wire.

#include <catch2/catch_test_macros.hpp>

#include "ControlProtocol.h"
#include "ReplCommandParser.h"

using namespace teleport_control;

TEST_CASE("dot-stuffing round-trips payload lines", "[control]")
{
	REQUIRE(StuffLine("hello") == "hello");
	REQUIRE(StuffLine(".") == "..");
	REQUIRE(StuffLine(".hidden") == "..hidden");
	REQUIRE(UnstuffLine("..hidden") == ".hidden");
	REQUIRE(UnstuffLine("plain") == "plain");
	REQUIRE(UnstuffLine("..") == ".");
	// A lone terminator is never a payload line, so UnstuffLine is not
	// expected to map it back to anything.
}

TEST_CASE("FrameResponse terminates with a sentinel line", "[control]")
{
	REQUIRE(FrameResponse("OK\npong\n") == "OK\npong\n.\n");
	// Blank payload lines are preserved.
	REQUIRE(FrameResponse("OK\n\nbody\n") == "OK\n\nbody\n.\n");
	// Payload lines starting with '.' are stuffed on the wire.
	REQUIRE(FrameResponse("OK\n..data\n") == "OK\n...data\n.\n");
	// Empty payload is just the terminator.
	REQUIRE(FrameResponse("") == ".\n");
	// CRLF in payloads is normalised to LF on the wire.
	REQUIRE(FrameResponse("OK\r\nbody\r\n") == "OK\nbody\n.\n");
}

TEST_CASE("status payload builders", "[control]")
{
	REQUIRE(Ok("pong\n") == "OK\npong\n");
	REQUIRE(Error("no such connection") == "ERROR no such connection\n");
	REQUIRE(std::string(STATUS_ERROR).back() == ' ');
}

TEST_CASE("parser splits verb and args on whitespace", "[control]")
{
	ReplCommandParser parser;

	auto cmd = parser.Parse("connect example.com:8080");
	REQUIRE(cmd.IsValid());
	REQUIRE(cmd.verb == "connect");
	REQUIRE(cmd.args == std::vector<std::string>{"example.com:8080"});

	cmd = parser.Parse("  move   1 2 3  ");
	REQUIRE(cmd.verb == "move");
	REQUIRE(cmd.args == std::vector<std::string>{"1", "2", "3"});

	REQUIRE_FALSE(parser.Parse("").IsValid());
	REQUIRE_FALSE(parser.Parse("   ").IsValid());
}
