#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <utility>

//! The outcome of one control command, before it is rendered onto the wire.
//!
//! Every verb produces both forms: `text` for a human at a terminal, `data` for a
//! machine client such as the MCP server. ControlServer renders whichever the control
//! session asked for with the `format` verb — see docs/protocol/local_control.rst.
//! Holding both in one struct is what stops the two renderings drifting apart: a verb
//! that fills only one of them is a bug, not a shortcut.
struct CommandResult
{
	bool ok = true;
	//! Human-readable body, excluding the "OK" / "ERROR ..." status header line.
	std::string text;
	//! Message following "ERROR " on the status line. Only meaningful when !ok.
	std::string error;
	//! Machine-readable body. Always an object; errors carry {"error": <error>}.
	nlohmann::json data = nlohmann::json::object();

	static CommandResult Success(std::string text, nlohmann::json data = nlohmann::json::object())
	{
		CommandResult r;
		r.text = std::move(text);
		r.data = std::move(data);
		return r;
	}

	static CommandResult Failure(const std::string &message)
	{
		CommandResult r;
		r.ok	= false;
		r.error = message;
		r.data	= nlohmann::json{{"error", message}};
		return r;
	}
};
