// Unit tests for ReplCommandParser: the whitespace-tokenising line parser shared by
// the legacy stdin REPL grammar and CommandProcessor (the local control server's
// command dispatcher, see docs/protocol/local_control.rst). Covers the verbs added
// for the service/CLI split (connect, connections/list, use, disconnect, ping,
// version, shutdown) alongside the pre-existing ones.

#include <catch2/catch_test_macros.hpp>

#include "ReplCommandParser.h"

TEST_CASE("ReplCommandParser: blank input is invalid", "[repl-command-parser]")
{
	ReplCommandParser parser;
	REQUIRE_FALSE(parser.Parse("").IsValid());
	REQUIRE_FALSE(parser.Parse("   ").IsValid());
}

TEST_CASE("ReplCommandParser: verb with no arguments", "[repl-command-parser]")
{
	ReplCommandParser parser;
	auto cmd = parser.Parse("ping");
	REQUIRE(cmd.IsValid());
	CHECK(cmd.verb == "ping");
	CHECK(cmd.args.empty());
}

TEST_CASE("ReplCommandParser: splits verb and arguments on whitespace", "[repl-command-parser]")
{
	ReplCommandParser parser;
	auto cmd = parser.Parse("move 1.0 2.5 -3.25");
	REQUIRE(cmd.IsValid());
	CHECK(cmd.verb == "move");
	REQUIRE(cmd.args.size() == 3);
	CHECK(cmd.args[0] == "1.0");
	CHECK(cmd.args[1] == "2.5");
	CHECK(cmd.args[2] == "-3.25");
}

TEST_CASE("ReplCommandParser: collapses repeated whitespace and trims the ends", "[repl-command-parser]")
{
	ReplCommandParser parser;
	auto cmd = parser.Parse("  use   3  ");
	REQUIRE(cmd.IsValid());
	CHECK(cmd.verb == "use");
	REQUIRE(cmd.args.size() == 1);
	CHECK(cmd.args[0] == "3");
}

TEST_CASE("ReplCommandParser: connection-management verbs", "[repl-command-parser]")
{
	ReplCommandParser parser;

	auto connect = parser.Parse("connect localhost:10000");
	CHECK(connect.verb == "connect");
	REQUIRE(connect.args.size() == 1);
	CHECK(connect.args[0] == "localhost:10000");

	CHECK(parser.Parse("connections").verb == "connections");
	CHECK(parser.Parse("list").verb == "list");

	auto use = parser.Parse("use 2");
	CHECK(use.verb == "use");
	REQUIRE(use.args.size() == 1);
	CHECK(use.args[0] == "2");

	auto disconnect = parser.Parse("disconnect 2");
	CHECK(disconnect.verb == "disconnect");
	REQUIRE(disconnect.args.size() == 1);
	CHECK(disconnect.args[0] == "2");

	CHECK(parser.Parse("disconnect").args.empty());
}

TEST_CASE("ReplCommandParser: liveness and lifecycle verbs", "[repl-command-parser]")
{
	ReplCommandParser parser;
	CHECK(parser.Parse("ping").verb == "ping");
	CHECK(parser.Parse("version").verb == "version");
	CHECK(parser.Parse("shutdown").verb == "shutdown");
	CHECK(parser.Parse("quit").verb == "quit");
	CHECK(parser.Parse("exit").verb == "exit");
	CHECK(parser.Parse("help").verb == "help");
}

TEST_CASE("ReplCommandParser: input subcommand is the first argument, not the verb", "[repl-command-parser]")
{
	ReplCommandParser parser;
	auto cmd = parser.Parse("input binary 12 1");
	CHECK(cmd.verb == "input");
	REQUIRE(cmd.args.size() == 3);
	CHECK(cmd.args[0] == "binary");
	CHECK(cmd.args[1] == "12");
	CHECK(cmd.args[2] == "1");
}
