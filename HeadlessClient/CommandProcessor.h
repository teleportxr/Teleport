#pragma once

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
	//! Set by the `shutdown` command; the service exits its main loop when it sees this.
	bool shutdownRequested = false;
	//! Set by `quit`/`exit` sent over the socket (e.g. from nc); the server closes
	//! this control connection after sending the response.
	bool closeAfterResponse = false;
};

//! Turns one parsed command line into a response payload. String-in/string-out:
//! the control server handles all socket framing. Replaces the stdin REPL
//! (Repl.cpp) — command behaviour is the same, generalised to N connections.
class CommandProcessor
{
public:
	explicit CommandProcessor(ConnectionManager &manager);

	//! Executes one command; returns the full response payload including the
	//! leading "OK" / "ERROR <message>" status header line.
	std::string Execute(const ReplCommand &cmd, ControlSessionState &state);

private:
	std::string Help() const;
	std::string Identity() const;
	std::string SignIn(const std::string &providerName) const;

	ConnectionManager &manager;
};
