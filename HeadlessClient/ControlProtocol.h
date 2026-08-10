#pragma once

#include <cstdint>
#include <string>

//! Constants and framing helpers for the local control protocol spoken between the
//! teleport service (teleport_terminal) and its command-line front end (teleport_cli).
//! See Teleport/docs/protocol/local_control.rst for the full specification.
//!
//! Wire format, over a TCP connection to 127.0.0.1:
//!   Request:  exactly one line of UTF-8, the same grammar as the legacy stdin REPL.
//!   Response: zero or more dot-stuffed UTF-8 lines, terminated by a line containing
//!             a single '.'. Any payload line beginning with '.' has one extra '.'
//!             prepended (SMTP-style dot-stuffing), so the terminator is unambiguous
//!             and the channel stays drivable from nc for debugging.
//! The first payload line is a status header: "OK" or "ERROR <message>", which the
//! CLI maps onto its process exit code.
namespace teleport_control
{
	//! Default TCP port the service listens on (localhost only). Overridable with
	//! -p on either binary, or the TELEPORT_SERVICE_PORT environment variable.
	inline constexpr uint16_t DEFAULT_PORT = 10510;

	//! A line containing only this character terminates a response.
	inline constexpr char TERMINATOR = '.';

	inline constexpr const char *STATUS_OK = "OK";
	inline constexpr const char *STATUS_ERROR = "ERROR ";

	//! Stuff one payload line for transmission: lines beginning with the terminator
	//! character get one extra copy prepended.
	inline std::string StuffLine(const std::string &line)
	{
		if (!line.empty() && line.front() == TERMINATOR)
			return std::string(1, TERMINATOR) + line;
		return line;
	}

	//! Reverse StuffLine on the receiving side.
	inline std::string UnstuffLine(const std::string &line)
	{
		if (line.size() >= 2 && line.front() == TERMINATOR && line[1] == TERMINATOR)
			return line.substr(1);
		return line;
	}

	//! Serialise a whole payload for transmission: stuffed lines plus the terminator line.
	inline std::string FrameResponse(const std::string &payload)
	{
		std::string out;
		size_t start = 0;
		while (start < payload.size())
		{
			size_t end = payload.find('\n', start);
			if (end == std::string::npos)
				end = payload.size();
			// Strip a trailing '\r' so CRLF-producing payloads frame cleanly.
			std::string line = payload.substr(start, end - start);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			out += StuffLine(line) + "\n";
			start = end + 1;
		}
		out += std::string(1, TERMINATOR) + "\n";
		return out;
	}

	//! Build a successful response payload.
	inline std::string Ok(const std::string &body)
	{
		std::string out = STATUS_OK;
		out += "\n";
		out += body;
		return out;
	}

	//! Build an error response payload.
	inline std::string Error(const std::string &message)
	{
		return std::string(STATUS_ERROR) + message + "\n";
	}
} // namespace teleport_control
