#include "CommandProcessor.h"
#include "ConnectionManager.h"
#include "ConnectionReport.h"
#include "TeleportClient/Identity.h"
#include <stdexcept>
#include <utility>

using nlohmann::json;

namespace
{
	//! avs::uid is 64-bit and Teleport's uids routinely exceed 2^53, which a JSON number
	//! cannot carry into a JavaScript client without silent precision loss. Every uid on
	//! the wire is therefore a decimal string, matching what teleport-web-client and
	//! teleport-nodejs already do (they hold uids as BigInt and serialise them as strings).
	//! Connection ids are small uint32 counters and stay as numbers.
	std::string UidStr(avs::uid uid)
	{
		return std::to_string(uid);
	}

	//! One row of the command table: the single source for `help` in both renderings,
	//! and the list teleport_cli offers for tab completion.
	struct CommandDoc
	{
		const char *verb;
		const char *aliases; //!< Comma-separated, empty when there are none.
		const char *usage;
		const char *summary;
	};

	constexpr CommandDoc COMMANDS[] = {
		{"connect", "", "connect <host[:port]>", "Connect to a server; prints 'id N' and selects it"},
		{"connections", "list", "connections", "List connections (* marks the selected one)"},
		{"use", "", "use <id>", "Select the connection other commands act on"},
		{"disconnect", "", "disconnect [id]", "Disconnect (default: selected connection)"},
		{"status", "", "status", "Show selected connection status"},
		{"move", "", "move <x> <y> <z>", "Set avatar position"},
		{"turn", "", "turn <qx> <qy> <qz> <qw>", "Set avatar orientation (quaternion)"},
		{"input", "", "input list", "List available inputs"},
		{"input", "", "input binary <id> <0|1>", "Send binary input event"},
		{"input", "", "input analogue <id> <f>", "Send analogue input event"},
		{"input", "", "input motion <id> <x> <y>", "Send motion input event"},
		{"mode", "", "mode <minimal|simulated>", "Set client mode"},
		{"geometry", "", "geometry [nodes|resources]", "Report on streamed geometry"},
		{"identity", "", "identity", "Show who this service is signed in as"},
		{"signin", "", "signin [provider]", "Sign in; prints a URL and a code to enter elsewhere"},
		{"signout", "", "signout", "Forget the current identity"},
		{"format", "", "format <text|json>", "Set this control connection's response format"},
		{"ping", "", "ping", "Liveness check (answers 'pong')"},
		{"version", "", "version", "Service and protocol version"},
		{"shutdown", "", "shutdown", "Stop the service (streaming connections included)"},
		{"quit", "exit", "quit / exit", "Detach this control client (streams keep running)"},
		{"help", "", "help", "Show this help"},
	};

	//! Column at which the " - <summary>" part of a help line begins. Usages longer than
	//! this get a single space instead, which is how the help has always looked.
	constexpr size_t HELP_USAGE_WIDTH = 23;

	//! Version of the local control protocol. Bump on any incompatible change to framing,
	//! the status header, or a documented `data` schema.
	constexpr int PROTOCOL_VERSION = 1;

	//! Run fn against the session's selected connection, mapping a vanished connection
	//! onto an error. fn returns the whole CommandResult.
	template <typename F>
	CommandResult OnSelected(ConnectionManager &manager, ControlSessionState &state, F &&fn)
	{
		if (!state.selectedId)
			return CommandResult::Failure("no connection selected (connect first, or 'use <id>')");
		try
		{
			return manager.WithConnection(state.selectedId, [&](HeadlessConnection &conn) { return fn(conn); });
		}
		catch (const std::out_of_range &)
		{
			state.selectedId = 0;
			return CommandResult::Failure("selected connection no longer exists");
		}
	}

	//! Parse a connection id argument, or report why it is not one.
	bool ParseId(const std::string &arg, uint32_t &out)
	{
		try
		{
			out = static_cast<uint32_t>(std::stoul(arg));
			return true;
		}
		catch (const std::exception &)
		{
			return false;
		}
	}

	json StatusJson(uint32_t id, const ConnectionStatus &status, HeadlessMode mode)
	{
		json j;
		j["id"]			   = id;
		j["state"]		   = status.state;
		j["hasSession"]	   = status.hasSession;
		j["server"]		   = status.server;
		j["port"]		   = status.port;
		j["latencyMs"]	   = status.latencyMs;
		j["inputsAvailable"] = status.inputsAvailable;
		j["mode"]		   = (mode == HeadlessMode::Minimal) ? "minimal" : "simulated";
		return j;
	}

	json GeometrySummaryJson(const GeometryReport &report)
	{
		json j;
		j["hasCache"]			 = report.hasCache;
		j["nodes"]				 = report.counts.nodes;
		j["nodesRemoved"]		 = report.counts.nodesRemoved;
		j["skeletons"]			 = report.counts.skeletons;
		j["resourcesReceived"]	 = report.counts.resourcesReceived;
		j["pointers"]			 = report.counts.pointers;
		j["referencedUnsent"]	 = report.counts.referencedUnsent;
		j["pendingResourceAcks"] = report.counts.pendingResourceAcks;
		j["pendingNodeAcks"]	 = report.counts.pendingNodeAcks;
		json unparsed			 = json::object();
		for (const auto &u : report.unparsed)
			unparsed[u.type] = u.count;
		j["unparsed"] = unparsed;
		return j;
	}

	json GeometryNodesJson(const GeometryReport &report)
	{
		json nodes = json::array();
		for (const auto &n : report.nodes)
		{
			json e;
			e["uid"]		= UidStr(n.uid);
			e["name"]		= n.name;
			e["type"]		= n.dataType;
			e["data"]		= UidStr(n.data);
			e["parent"]		= UidStr(n.parent);
			e["skeleton"]	= UidStr(n.skeleton);
			e["materials"]	= n.materials;
			e["animations"] = n.animations;
			e["url"]		= n.url;
			nodes.push_back(std::move(e));
		}
		return json{{"hasCache", report.hasCache}, {"nodes", std::move(nodes)}};
	}

	json GeometryResourcesJson(const GeometryReport &report)
	{
		json pointers = json::array();
		for (const auto &p : report.pointers)
			pointers.push_back(json{{"uid", UidStr(p.uid)}, {"type", p.type}, {"url", p.url}});
		json missing = json::array();
		for (avs::uid u : report.referencedUnsent)
			missing.push_back(UidStr(u));
		return json{{"hasCache", report.hasCache}, {"pointers", std::move(pointers)}, {"missing", std::move(missing)}};
	}
} // namespace

CommandProcessor::CommandProcessor(ConnectionManager &manager)
	: manager(manager)
{
}

CommandResult CommandProcessor::Execute(const ReplCommand &cmd, ControlSessionState &state)
{
	if (!cmd.IsValid())
		return CommandResult::Failure("empty command");

	if (cmd.verb == "help")
	{
		return Help();
	}
	else if (cmd.verb == "ping")
	{
		return CommandResult::Success("pong\n", json{{"pong", true}});
	}
	else if (cmd.verb == "version")
	{
		return CommandResult::Success("teleport service, control protocol " + std::to_string(PROTOCOL_VERSION) + "\n",
			json{{"service", "teleportd"}, {"protocol", PROTOCOL_VERSION}});
	}
	else if (cmd.verb == "format")
	{
		if (cmd.args.empty())
			return CommandResult::Failure("usage: format <text|json>");
		if (cmd.args[0] != "text" && cmd.args[0] != "json")
			return CommandResult::Failure("unknown format: " + cmd.args[0]);
		state.jsonOutput = (cmd.args[0] == "json");
		return CommandResult::Success("format set to " + cmd.args[0] + "\n", json{{"format", cmd.args[0]}});
	}
	else if (cmd.verb == "connect")
	{
		if (cmd.args.empty())
			return CommandResult::Failure("usage: connect <host[:port]>");
		std::string url = cmd.args[0];
		if (url.find("://") == std::string::npos)
			url = "teleport://" + url;
		std::string error;
		uint32_t	id = manager.Create(url, error);
		if (!id)
			return CommandResult::Failure(error);
		state.selectedId = id;
		return CommandResult::Success("id " + std::to_string(id) + "\nconnecting to " + url + "\n", json{{"id", id}, {"url", url}});
	}
	else if (cmd.verb == "connections" || cmd.verb == "list")
	{
		auto infos		 = manager.List();
		json connections = json::array();
		for (const auto &info : infos)
		{
			connections.push_back(json{
				{"id", info.id},
				{"url", info.url},
				{"state", info.status.state},
				{"connected", info.connected},
				{"selected", info.id == state.selectedId},
			});
		}
		json data{{"selected", state.selectedId}, {"connections", std::move(connections)}};
		if (infos.empty())
			return CommandResult::Success("no connections\n", std::move(data));
		std::string body;
		for (const auto &info : infos)
		{
			body += (info.id == state.selectedId) ? "* " : "  ";
			body += std::to_string(info.id) + "\t" + RenderStatusLine(info.status) + "\t" + info.url + "\n";
		}
		return CommandResult::Success(std::move(body), std::move(data));
	}
	else if (cmd.verb == "use")
	{
		if (cmd.args.empty())
			return CommandResult::Failure("usage: use <id>");
		uint32_t id = 0;
		if (!ParseId(cmd.args[0], id))
			return CommandResult::Failure("invalid connection id: " + cmd.args[0]);
		if (!manager.Exists(id))
			return CommandResult::Failure("no connection with id " + std::to_string(id));
		state.selectedId = id;
		return CommandResult::Success("selected connection " + std::to_string(id) + "\n", json{{"selected", id}});
	}
	else if (cmd.verb == "disconnect")
	{
		uint32_t id = state.selectedId;
		if (!cmd.args.empty() && !ParseId(cmd.args[0], id))
			return CommandResult::Failure("invalid connection id: " + cmd.args[0]);
		if (!id)
			return CommandResult::Failure("no connection selected (disconnect [id])");
		if (!manager.Destroy(id))
			return CommandResult::Failure("no connection with id " + std::to_string(id));
		if (state.selectedId == id)
			state.selectedId = 0;
		return CommandResult::Success("disconnected " + std::to_string(id) + "\n", json{{"disconnected", id}});
	}
	else if (cmd.verb == "status")
	{
		uint32_t id = state.selectedId;
		return OnSelected(manager, state, [id](HeadlessConnection &conn) {
			ConnectionStatus status = conn.GetStatusData();
			std::string		 body	= RenderStatus(status);
			body += "Mode: ";
			body += (conn.GetMode() == HeadlessMode::Minimal ? "minimal" : "simulated");
			body += "\n";
			return CommandResult::Success(std::move(body), StatusJson(id, status, conn.GetMode()));
		});
	}
	else if (cmd.verb == "move")
	{
		if (cmd.args.size() < 3)
			return CommandResult::Failure("usage: move <x> <y> <z>");
		float x = 0, y = 0, z = 0;
		try
		{
			x = std::stof(cmd.args[0]);
			y = std::stof(cmd.args[1]);
			z = std::stof(cmd.args[2]);
		}
		catch (const std::exception &e)
		{
			return CommandResult::Failure(std::string("parsing arguments: ") + e.what());
		}
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			conn.GetInputState().SetPose(x, y, z, 0.0f, 0.0f, 0.0f, 1.0f);
			return CommandResult::Success("position set to (" + cmd.args[0] + ", " + cmd.args[1] + ", " + cmd.args[2] + ")\n",
				json{{"position", {x, y, z}}});
		});
	}
	else if (cmd.verb == "turn")
	{
		if (cmd.args.size() < 4)
			return CommandResult::Failure("usage: turn <qx> <qy> <qz> <qw>");
		float qx = 0, qy = 0, qz = 0, qw = 1;
		try
		{
			qx = std::stof(cmd.args[0]);
			qy = std::stof(cmd.args[1]);
			qz = std::stof(cmd.args[2]);
			qw = std::stof(cmd.args[3]);
		}
		catch (const std::exception &e)
		{
			return CommandResult::Failure(std::string("parsing arguments: ") + e.what());
		}
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			conn.GetInputState().SetOrientation(qx, qy, qz, qw);
			return CommandResult::Success("orientation set\n", json{{"orientation", {qx, qy, qz, qw}}});
		});
	}
	else if (cmd.verb == "input")
	{
		if (cmd.args.empty())
			return CommandResult::Failure("usage: input <list|binary|analogue|motion> ...");
		const std::string &sub = cmd.args[0];
		if (sub == "list")
		{
			return OnSelected(manager, state, [](HeadlessConnection &conn) {
				const auto &inputs = conn.GetInputDefinitions();
				std::string body   = "available inputs: " + std::to_string(inputs.size()) + "\n";
				json		list   = json::array();
				for (const auto &input : inputs)
				{
					body += "  " + input.regexPath + "\n";
					list.push_back(json{
						{"id", static_cast<uint64_t>(input.inputId)},
						{"type", static_cast<int>(input.inputType)},
						{"regexPath", input.regexPath},
					});
				}
				return CommandResult::Success(std::move(body), json{{"inputs", std::move(list)}});
			});
		}
		else if (sub == "binary")
		{
			if (cmd.args.size() < 3)
				return CommandResult::Failure("usage: input binary <id> <0|1>");
			avs::uid id	   = 0;
			uint8_t	 value = 0;
			try
			{
				id	  = std::stoull(cmd.args[1]);
				value = (cmd.args[2] == "1") ? 1 : 0;
			}
			catch (const std::exception &e)
			{
				return CommandResult::Failure(std::string("parsing arguments: ") + e.what());
			}
			return OnSelected(manager, state, [&](HeadlessConnection &conn) {
				conn.SendBinaryInput(id, value);
				return CommandResult::Success("sent binary input " + cmd.args[1] + "=" + std::to_string(value) + "\n",
					json{{"sent", {{"kind", "binary"}, {"id", id}, {"value", value != 0}}}});
			});
		}
		else if (sub == "analogue")
		{
			if (cmd.args.size() < 3)
				return CommandResult::Failure("usage: input analogue <id> <value>");
			avs::uid id	   = 0;
			float	 value = 0;
			try
			{
				id	  = std::stoull(cmd.args[1]);
				value = std::stof(cmd.args[2]);
			}
			catch (const std::exception &e)
			{
				return CommandResult::Failure(std::string("parsing arguments: ") + e.what());
			}
			return OnSelected(manager, state, [&](HeadlessConnection &conn) {
				conn.SendAnalogueInput(id, value);
				return CommandResult::Success("sent analogue input " + cmd.args[1] + "=" + cmd.args[2] + "\n",
					json{{"sent", {{"kind", "analogue"}, {"id", id}, {"value", value}}}});
			});
		}
		else if (sub == "motion")
		{
			if (cmd.args.size() < 4)
				return CommandResult::Failure("usage: input motion <id> <x> <y>");
			avs::uid id = 0;
			float	 x = 0, y = 0;
			try
			{
				id = std::stoull(cmd.args[1]);
				x  = std::stof(cmd.args[2]);
				y  = std::stof(cmd.args[3]);
			}
			catch (const std::exception &e)
			{
				return CommandResult::Failure(std::string("parsing arguments: ") + e.what());
			}
			return OnSelected(manager, state, [&](HeadlessConnection &conn) {
				conn.SendMotionInput(id, x, y);
				return CommandResult::Success("sent motion input " + cmd.args[1] + "=(" + cmd.args[2] + "," + cmd.args[3] + ")\n",
					json{{"sent", {{"kind", "motion"}, {"id", id}, {"x", x}, {"y", y}}}});
			});
		}
		return CommandResult::Failure("unknown input subcommand: " + sub);
	}
	else if (cmd.verb == "mode")
	{
		if (cmd.args.empty())
			return CommandResult::Failure("usage: mode <minimal|simulated>");
		if (cmd.args[0] != "minimal" && cmd.args[0] != "simulated")
			return CommandResult::Failure("unknown mode: " + cmd.args[0]);
		HeadlessMode mode = (cmd.args[0] == "minimal") ? HeadlessMode::Minimal : HeadlessMode::Simulated;
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			conn.SetMode(mode);
			return CommandResult::Success("mode set to " + cmd.args[0] + "\n", json{{"mode", cmd.args[0]}});
		});
	}
	else if (cmd.verb == "geometry")
	{
		std::string what = cmd.args.empty() ? std::string() : cmd.args[0];
		if (!what.empty() && what != "nodes" && what != "resources")
			return CommandResult::Failure("usage: geometry [nodes|resources]");
		return OnSelected(manager, state, [&](HeadlessConnection &conn) {
			GeometryReport report = conn.GetGeometryData();
			if (what == "nodes")
				return CommandResult::Success(RenderGeometryNodes(report), GeometryNodesJson(report));
			if (what == "resources")
				return CommandResult::Success(RenderGeometryResources(report), GeometryResourcesJson(report));
			return CommandResult::Success(RenderGeometrySummary(report), GeometrySummaryJson(report));
		});
	}
	else if (cmd.verb == "identity")
	{
		return Identity();
	}
	else if (cmd.verb == "signin")
	{
		return SignIn(cmd.args.empty() ? std::string() : cmd.args[0]);
	}
	else if (cmd.verb == "signout")
	{
		teleport::client::identity.SignOut();
		return CommandResult::Success("signed out\n");
	}
	else if (cmd.verb == "shutdown")
	{
		state.shutdownRequested = true;
		return CommandResult::Success("service shutting down\n", json{{"shuttingDown", true}});
	}
	else if (cmd.verb == "quit" || cmd.verb == "exit")
	{
		// CLIs handle these locally; reaching here means a raw socket client
		// (e.g. nc). Answer politely and hang up this control connection only —
		// streaming connections are unaffected.
		state.closeAfterResponse = true;
		return CommandResult::Success("bye\n", json{{"bye", true}});
	}

	return CommandResult::Failure("unknown command: " + cmd.verb);
}

CommandResult CommandProcessor::Help() const
{
	std::string body = "Commands:\n";
	json		list = json::array();
	for (const auto &doc : COMMANDS)
	{
		std::string usage = doc.usage;
		// Pad the usage column so the summaries line up, but never run the two together:
		// a usage wider than the column gets a single space.
		size_t pad = (usage.size() < HELP_USAGE_WIDTH) ? HELP_USAGE_WIDTH - usage.size() : 1;
		body += "  " + usage + std::string(pad, ' ') + "- " + doc.summary + "\n";

		json entry{{"verb", doc.verb}, {"usage", doc.usage}, {"summary", doc.summary}};
		json aliases = json::array();
		if (doc.aliases[0])
			aliases.push_back(doc.aliases);
		entry["aliases"] = std::move(aliases);
		list.push_back(std::move(entry));
	}
	return CommandResult::Success(std::move(body), json{{"commands", std::move(list)}});
}

CommandResult CommandProcessor::Identity() const
{
	auto	   &identity = teleport::client::identity;
	std::string body	 = identity.GetDisplayText() + "\n";
	json		data;
	data["displayText"] = identity.GetDisplayText();
	data["signedIn"]	= identity.IsSignedIn();
	if (identity.IsSignedIn())
	{
		teleport::client::IdentityProfile profile = identity.GetProfile();
		body += "Provider: " + profile.provider + "\n";
		body += "Subject:  " + profile.subject + "\n";
		data["provider"] = profile.provider;
		data["subject"]	 = profile.subject;
		if (!profile.email.empty())
		{
			body += "Email:    " + profile.email + " (kept locally; not sent to servers)\n";
			data["email"] = profile.email;
		}
	}
	const std::string error = identity.GetLastError();
	if (!error.empty())
		body += "Last error: " + error + "\n";
	data["lastError"] = error;

	body += "Providers:";
	json providers = json::array();
	for (const auto &p : identity.GetProviders())
	{
		body += " ";
		body += p->GetName();
		providers.push_back(p->GetName());
	}
	body += "\n";
	data["providers"] = std::move(providers);

	// The device-code prompt, when a sign-in is waiting on the user. Without this an
	// agent driving the service over MCP can never complete a sign-in: the code is
	// otherwise only written to the service log, which it cannot read.
	teleport::client::SignInPrompt prompt = identity.GetSignInPrompt();
	if (prompt.IsPending())
	{
		body += "Sign-in pending: enter code " + prompt.userCode + " at " + prompt.verificationUrl + "\n";
		data["pendingSignIn"] = json{
			{"userCode", prompt.userCode},
			{"verificationUrl", prompt.verificationUrl},
			{"expiresInSeconds", prompt.expiresInSeconds},
		};
	}
	else
	{
		data["pendingSignIn"] = nullptr;
	}
	return CommandResult::Success(std::move(body), std::move(data));
}

CommandResult CommandProcessor::SignIn(const std::string &providerName) const
{
	auto	   &identity  = teleport::client::identity;
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
		return CommandResult::Failure("no identity providers are available in this build");
	if (identity.SignIn(name))
	{
		// The flow runs on a worker thread; poll `identity` for the device code, which
		// appears in its pendingSignIn field once the provider replies.
		return CommandResult::Success("signing in with " + name + "... (poll `identity` for the code)\n",
			json{{"provider", name}, {"started", true}});
	}
	return CommandResult::Failure("could not start sign-in (already in progress, or no such provider: " + name + ")");
}
