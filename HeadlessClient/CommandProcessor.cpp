#include "CommandProcessor.h"
#include "ConnectionManager.h"
#include "ControlProtocol.h"
#include "TeleportClient/Identity.h"
#include <stdexcept>
#include <utility>

using teleport_control::Error;
using teleport_control::Ok;

CommandProcessor::CommandProcessor(ConnectionManager &manager)
	: manager(manager)
{
}

//! Run fn against the session's selected connection, mapping failure onto an
//! ERROR payload. fn returns the body of an OK response.
template <typename F>
static std::string OnSelected(ConnectionManager &manager, ControlSessionState &state, F &&fn)
{
	if (!state.selectedId)
		return Error("no connection selected (connect first, or 'use <id>')");
	try
	{
		return manager.WithConnection(state.selectedId, [&](HeadlessConnection &conn) {
			return Ok(fn(conn));
		});
	}
	catch (const std::out_of_range &)
	{
		state.selectedId = 0;
		return Error("selected connection no longer exists");
	}
}

std::string CommandProcessor::Execute(const ReplCommand &cmd, ControlSessionState &state)
{
	if (!cmd.IsValid())
		return Error("empty command");

	if (cmd.verb == "help")
	{
		return Ok(Help());
	}
	else if (cmd.verb == "ping")
	{
		return Ok("pong\n");
	}
	else if (cmd.verb == "version")
	{
		return Ok("teleport service, control protocol 1\n");
	}
	else if (cmd.verb == "connect")
	{
		if (cmd.args.empty())
			return Error("usage: connect <host[:port]>");
		std::string url = cmd.args[0];
		if (url.find("://") == std::string::npos)
			url = "teleport://" + url;
		std::string error;
		uint32_t id = manager.Create(url, error);
		if (!id)
			return Error(error);
		state.selectedId = id;
		return Ok("id " + std::to_string(id) + "\nconnecting to " + url + "\n");
	}
	else if (cmd.verb == "connections" || cmd.verb == "list")
	{
		auto infos = manager.List();
		if (infos.empty())
			return Ok("no connections\n");
		std::string body;
		for (const auto &info : infos)
		{
			body += (info.id == state.selectedId) ? "* " : "  ";
			body += std::to_string(info.id) + "\t" + info.statusLine + "\t" + info.url + "\n";
		}
		return Ok(body);
	}
	else if (cmd.verb == "use")
	{
		if (cmd.args.empty())
			return Error("usage: use <id>");
		uint32_t id = 0;
		try
		{
			id = static_cast<uint32_t>(std::stoul(cmd.args[0]));
		}
		catch (const std::exception &)
		{
			return Error("invalid connection id: " + cmd.args[0]);
		}
		if (!manager.Exists(id))
			return Error("no connection with id " + std::to_string(id));
		state.selectedId = id;
		return Ok("selected connection " + std::to_string(id) + "\n");
	}
	else if (cmd.verb == "disconnect")
	{
		uint32_t id = state.selectedId;
		if (!cmd.args.empty())
		{
			try
			{
				id = static_cast<uint32_t>(std::stoul(cmd.args[0]));
			}
			catch (const std::exception &)
			{
				return Error("invalid connection id: " + cmd.args[0]);
			}
		}
		if (!id)
			return Error("no connection selected (disconnect [id])");
		if (!manager.Destroy(id))
			return Error("no connection with id " + std::to_string(id));
		if (state.selectedId == id)
			state.selectedId = 0;
		return Ok("disconnected " + std::to_string(id) + "\n");
	}
	else if (cmd.verb == "status")
	{
		return OnSelected(manager, state, [](HeadlessConnection &conn) {
			std::string body = conn.GetStatus();
			body += "Mode: ";
			body += (conn.GetMode() == HeadlessMode::Minimal ? "minimal" : "simulated");
			body += "\n";
			return body;
		});
	}
	else if (cmd.verb == "move")
	{
		if (cmd.args.size() < 3)
			return Error("usage: move <x> <y> <z>");
		try
		{
			float x = std::stof(cmd.args[0]);
			float y = std::stof(cmd.args[1]);
			float z = std::stof(cmd.args[2]);
			return OnSelected(manager, state, [&](HeadlessConnection &conn) {
				conn.GetInputState().SetPose(x, y, z, 0.0f, 0.0f, 0.0f, 1.0f);
				return "position set to (" + cmd.args[0] + ", " + cmd.args[1] + ", " + cmd.args[2] + ")\n";
			});
		}
		catch (const std::exception &e)
		{
			return Error(std::string("parsing arguments: ") + e.what());
		}
	}
	else if (cmd.verb == "turn")
	{
		if (cmd.args.size() < 4)
			return Error("usage: turn <qx> <qy> <qz> <qw>");
		try
		{
			float qx = std::stof(cmd.args[0]);
			float qy = std::stof(cmd.args[1]);
			float qz = std::stof(cmd.args[2]);
			float qw = std::stof(cmd.args[3]);
			return OnSelected(manager, state, [&](HeadlessConnection &conn) {
				conn.GetInputState().SetOrientation(qx, qy, qz, qw);
				return "orientation set\n";
			});
		}
		catch (const std::exception &e)
		{
			return Error(std::string("parsing arguments: ") + e.what());
		}
	}
	else if (cmd.verb == "input")
	{
		if (cmd.args.empty())
			return Error("usage: input <list|binary|analogue|motion> ...");
		const std::string &sub = cmd.args[0];
		if (sub == "list")
		{
			return OnSelected(manager, state, [](HeadlessConnection &conn) {
				const auto &inputs = conn.GetInputDefinitions();
				std::string body = "available inputs: " + std::to_string(inputs.size()) + "\n";
				for (const auto &input : inputs)
					body += "  " + input.regexPath + "\n";
				return body;
			});
		}
		else if (sub == "binary")
		{
			if (cmd.args.size() < 3)
				return Error("usage: input binary <id> <0|1>");
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				uint8_t value = (cmd.args[2] == "1") ? 1 : 0;
				return OnSelected(manager, state, [&](HeadlessConnection &conn) {
					conn.SendBinaryInput(id, value);
					return "sent binary input " + cmd.args[1] + "=" + std::to_string(value) + "\n";
				});
			}
			catch (const std::exception &e)
			{
				return Error(std::string("parsing arguments: ") + e.what());
			}
		}
		else if (sub == "analogue")
		{
			if (cmd.args.size() < 3)
				return Error("usage: input analogue <id> <value>");
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				float value = std::stof(cmd.args[2]);
				return OnSelected(manager, state, [&](HeadlessConnection &conn) {
					conn.SendAnalogueInput(id, value);
					return "sent analogue input " + cmd.args[1] + "=" + cmd.args[2] + "\n";
				});
			}
			catch (const std::exception &e)
			{
				return Error(std::string("parsing arguments: ") + e.what());
			}
		}
		else if (sub == "motion")
		{
			if (cmd.args.size() < 4)
				return Error("usage: input motion <id> <x> <y>");
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				float x = std::stof(cmd.args[2]);
				float y = std::stof(cmd.args[3]);
				return OnSelected(manager, state, [&](HeadlessConnection &conn) {
					conn.SendMotionInput(id, x, y);
					return "sent motion input " + cmd.args[1] + "=(" + cmd.args[2] + "," + cmd.args[3] + ")\n";
				});
			}
			catch (const std::exception &e)
			{
				return Error(std::string("parsing arguments: ") + e.what());
			}
		}
		return Error("unknown input subcommand: " + sub);
	}
	else if (cmd.verb == "mode")
	{
		if (cmd.args.empty())
			return Error("usage: mode <minimal|simulated>");
		if (cmd.args[0] != "minimal" && cmd.args[0] != "simulated")
			return Error("unknown mode: " + cmd.args[0]);
		HeadlessMode mode = (cmd.args[0] == "minimal") ? HeadlessMode::Minimal : HeadlessMode::Simulated;
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			conn.SetMode(mode);
			return "mode set to " + cmd.args[0] + "\n";
		});
	}
	else if (cmd.verb == "geometry")
	{
		std::string what = cmd.args.empty() ? std::string() : cmd.args[0];
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			return conn.GetGeometryReport(what);
		});
	}
	else if (cmd.verb == "identity")
	{
		return Ok(Identity());
	}
	else if (cmd.verb == "signin")
	{
		return SignIn(cmd.args.empty() ? std::string() : cmd.args[0]);
	}
	else if (cmd.verb == "signout")
	{
		teleport::client::identity.SignOut();
		return Ok("signed out\n");
	}
	else if (cmd.verb == "shutdown")
	{
		state.shutdownRequested = true;
		return Ok("service shutting down\n");
	}
	else if (cmd.verb == "quit" || cmd.verb == "exit")
	{
		// CLIs handle these locally; reaching here means a raw socket client
		// (e.g. nc). Answer politely and hang up this control connection only —
		// streaming connections are unaffected.
		state.closeAfterResponse = true;
		return Ok("bye\n");
	}

	return Error("unknown command: " + cmd.verb);
}

std::string CommandProcessor::Help() const
{
	return "Commands:\n"
		   "  connect <host[:port]>  - Connect to a server; prints 'id N' and selects it\n"
		   "  connections            - List connections (* marks the selected one)\n"
		   "  use <id>               - Select the connection other commands act on\n"
		   "  disconnect [id]        - Disconnect (default: selected connection)\n"
		   "  status                 - Show selected connection status\n"
		   "  move <x> <y> <z>       - Set avatar position\n"
		   "  turn <qx> <qy> <qz> <qw> - Set avatar orientation (quaternion)\n"
		   "  input list             - List available inputs\n"
		   "  input binary <id> <0|1> - Send binary input event\n"
		   "  input analogue <id> <f> - Send analogue input event\n"
		   "  input motion <id> <x> <y> - Send motion input event\n"
		   "  mode <minimal|simulated> - Set client mode\n"
		   "  geometry [nodes|resources] - Report on streamed geometry\n"
		   "  identity               - Show who this service is signed in as\n"
		   "  signin [provider]      - Sign in; prints a URL and a code to enter elsewhere\n"
		   "  signout                - Forget the current identity\n"
		   "  ping                   - Liveness check (answers 'pong')\n"
		   "  version                - Service and protocol version\n"
		   "  shutdown               - Stop the service (streaming connections included)\n"
		   "  quit / exit            - Detach this control client (streams keep running)\n"
		   "  help                   - Show this help\n";
}

std::string CommandProcessor::Identity() const
{
	auto &identity = teleport::client::identity;
	std::string body = identity.GetDisplayText() + "\n";
	if (identity.IsSignedIn())
	{
		teleport::client::IdentityProfile profile = identity.GetProfile();
		body += "Provider: " + profile.provider + "\n";
		body += "Subject:  " + profile.subject + "\n";
		if (!profile.email.empty())
			body += "Email:    " + profile.email + " (kept locally; not sent to servers)\n";
	}
	const std::string error = identity.GetLastError();
	if (!error.empty())
		body += "Last error: " + error + "\n";
	body += "Providers:";
	for (const auto &p : identity.GetProviders())
	{
		body += " ";
		body += p->GetName();
	}
	body += "\n";
	return body;
}

std::string CommandProcessor::SignIn(const std::string &providerName) const
{
	auto &identity = teleport::client::identity;
	const auto &providers = identity.GetProviders();
	// Default to the first provider that needs the user, which is the interesting one.
	std::string name = providerName;
	if (name.empty())
	{
		for (const auto &p : providers)
		{
			if (p->RequiresInteraction())
			{
				name = p->GetName();
				break;
			}
		}
	}
	if (name.empty())
		return Error("no identity providers are available in this build");
	if (identity.SignIn(name))
	{
		// The flow runs on a worker thread and logs its instructions when Google replies.
		return Ok("signing in with " + name + "... (watch the service log for the code)\n");
	}
	return Error("could not start sign-in (already in progress, or no such provider: " + name + ")");
}
