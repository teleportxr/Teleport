#pragma once

#include "CommandResult.h"
#include "ReplCommandParser.h"
#include <cstdint>
#include <string>

class ConnectionManager;

//! Per-control-client session state: one instance per attached CLI connection,
//! owned by the control server. "Selected" connection is per-CLI, like mysql's
//! current database — two CLIs never disturb each other's selection.
struct ControlSessionState
{
	//! Connection subsequent commands act on; 0 = none selected.
	uint32_t selectedId = 0;
	//! Response rendering for this control connection, set by the `format` verb.
	//! Text is the default so that a bare `nc` session, and every existing script,
	//! see exactly what they always did.
	bool jsonOutput = false;
	//! Set by the `shutdown` command; the service exits its main loop when it sees this.
	bool shutdownRequested = false;
	//! Set by `quit`/`exit` sent over the socket (e.g. from nc); the server closes
	//! this control connection after sending the response.
	bool closeAfterResponse = false;
};

//! Turns one parsed command line into a CommandResult. No I/O and no framing: the
//! control server renders the result and handles the socket. Replaces the stdin REPL
//! (Repl.cpp) — command behaviour is the same, generalised to N connections.
class CommandProcessor
{
public:
	explicit CommandProcessor(ConnectionManager &manager);

	//! Executes one command. Both CommandResult::text and CommandResult::data are
	//! filled regardless of the session's current output format.
	CommandResult Execute(const ReplCommand &cmd, ControlSessionState &state);

private:
	CommandResult Help() const;
	CommandResult Identity() const;
	CommandResult SignIn(const std::string &providerName) const;

	ConnectionManager &manager;
};
