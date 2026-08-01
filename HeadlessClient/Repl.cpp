#include "Repl.h"
#include "TeleportClient/Identity.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/Input.h"
#include <iostream>
#include <iomanip>

Repl::Repl(HeadlessClient &client)
	: client(client)
{
}

Repl::~Repl()
{
	Stop();
}

void Repl::Run()
{
	std::cout << "Teleport Terminal Client\n";
	std::cout << "Type 'help' for commands\n\n";

	std::string line;
	while (!stopping && std::getline(std::cin, line))
	{
		if (line.empty())
			continue;

		auto cmd = parser.Parse(line);
		if (!cmd.IsValid())
			continue;

		ProcessCommand(cmd);

		if (cmd.verb == "quit" || cmd.verb == "exit")
			break;
	}
}

void Repl::Stop()
{
	stopping = true;
}

void Repl::ProcessCommand(const ReplCommand &cmd)
{
	if (cmd.verb == "help")
	{
		PrintHelp();
	}
	else if (cmd.verb == "connect")
	{
		if (cmd.args.empty())
		{
			std::cout << "Usage: connect <ip[:port]>\n";
			return;
		}
		std::string url = "teleport://" + cmd.args[0];
		if (client.Connect(url))
			std::cout << "Connecting to " << cmd.args[0] << "\n";
		else
			std::cout << "Connection failed\n";
	}
	else if (cmd.verb == "disconnect")
	{
		client.Disconnect();
		std::cout << "Disconnected\n";
	}
	else if (cmd.verb == "status")
	{
		PrintStatus();
	}
	else if (cmd.verb == "move")
	{
		if (cmd.args.size() < 3)
		{
			std::cout << "Usage: move <x> <y> <z>\n";
			return;
		}
		try
		{
			float x = std::stof(cmd.args[0]);
			float y = std::stof(cmd.args[1]);
			float z = std::stof(cmd.args[2]);
			client.GetInputState().SetPose(x, y, z, 0.0f, 0.0f, 0.0f, 1.0f);
			std::cout << "Position set to (" << x << ", " << y << ", " << z << ")\n";
		}
		catch (const std::exception &e)
		{
			std::cout << "Error parsing arguments: " << e.what() << "\n";
		}
	}
	else if (cmd.verb == "turn")
	{
		if (cmd.args.size() < 4)
		{
			std::cout << "Usage: turn <qx> <qy> <qz> <qw>\n";
			return;
		}
		try
		{
			float qx = std::stof(cmd.args[0]);
			float qy = std::stof(cmd.args[1]);
			float qz = std::stof(cmd.args[2]);
			float qw = std::stof(cmd.args[3]);
			client.GetInputState().SetOrientation(qx, qy, qz, qw);
			std::cout << "Orientation set\n";
		}
		catch (const std::exception &e)
		{
			std::cout << "Error parsing arguments: " << e.what() << "\n";
		}
	}
	else if (cmd.verb == "input")
	{
		if (cmd.args.empty())
		{
			std::cout << "Usage: input <list|binary|analogue|motion> ...\n";
			return;
		}
		if (cmd.args[0] == "list")
		{
			const auto &inputs = client.GetInputDefinitions();
			std::cout << "Available inputs: " << inputs.size() << "\n";
			for (const auto &input : inputs)
			{
				std::cout << "  " << input.regexPath << "\n";
			}
		}
		else if (cmd.args[0] == "binary")
		{
			if (cmd.args.size() < 3)
			{
				std::cout << "Usage: input binary <id> <0|1>\n";
				return;
			}
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				uint8_t value = (cmd.args[2] == "1") ? 1 : 0;
				client.SendBinaryInput(id, value);
				std::cout << "Sent binary input " << id << "=" << (int)value << "\n";
			}
			catch (const std::exception &e)
			{
				std::cout << "Error parsing arguments: " << e.what() << "\n";
			}
		}
		else if (cmd.args[0] == "analogue")
		{
			if (cmd.args.size() < 3)
			{
				std::cout << "Usage: input analogue <id> <value>\n";
				return;
			}
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				float value = std::stof(cmd.args[2]);
				client.SendAnalogueInput(id, value);
				std::cout << "Sent analogue input " << id << "=" << value << "\n";
			}
			catch (const std::exception &e)
			{
				std::cout << "Error parsing arguments: " << e.what() << "\n";
			}
		}
		else if (cmd.args[0] == "motion")
		{
			if (cmd.args.size() < 4)
			{
				std::cout << "Usage: input motion <id> <x> <y>\n";
				return;
			}
			try
			{
				avs::uid id = std::stoull(cmd.args[1]);
				float x = std::stof(cmd.args[2]);
				float y = std::stof(cmd.args[3]);
				client.SendMotionInput(id, x, y);
				std::cout << "Sent motion input " << id << "=(" << x << "," << y << ")\n";
			}
			catch (const std::exception &e)
			{
				std::cout << "Error parsing arguments: " << e.what() << "\n";
			}
		}
		else
		{
			std::cout << "Unknown input subcommand: " << cmd.args[0] << "\n";
		}
	}
	else if (cmd.verb == "mode")
	{
		if (cmd.args.empty())
		{
			std::cout << "Usage: mode <minimal|simulated>\n";
			return;
		}
		if (cmd.args[0] == "minimal")
		{
			client.SetMode(HeadlessMode::Minimal);
			std::cout << "Mode set to minimal\n";
		}
		else if (cmd.args[0] == "simulated")
		{
			client.SetMode(HeadlessMode::Simulated);
			std::cout << "Mode set to simulated\n";
		}
		else
		{
			std::cout << "Unknown mode: " << cmd.args[0] << "\n";
		}
	}
	else if (cmd.verb == "geometry")
	{
		std::cout << "\n=== Geometry ===\n";
		std::cout << client.GetGeometryReport(cmd.args.empty() ? std::string() : cmd.args[0]);
		std::cout << "\n";
	}
	else if (cmd.verb == "identity")
	{
		PrintIdentity();
	}
	else if (cmd.verb == "signin")
	{
		auto			  &identity	 = teleport::client::identity;
		const auto		  &providers = identity.GetProviders();
		// Default to the first provider that needs the user, which is the interesting one.
		std::string providerName = cmd.args.empty() ? std::string() : cmd.args[0];
		if (providerName.empty())
		{
			for (const auto &p : providers)
			{
				if (p->RequiresInteraction())
				{
					providerName = p->GetName();
					break;
				}
			}
		}
		if (providerName.empty())
		{
			std::cout << "No identity providers are available in this build.\n";
			return;
		}
		if (identity.SignIn(providerName))
		{
			// The flow runs on a worker thread and prints its instructions when Google replies.
			std::cout << "Signing in with " << providerName << "...\n";
		}
		else
		{
			std::cout << "Could not start sign-in (already in progress, or no such provider: " << providerName << ")\n";
		}
	}
	else if (cmd.verb == "signout")
	{
		teleport::client::identity.SignOut();
		std::cout << "Signed out\n";
	}
	else if (cmd.verb == "quit" || cmd.verb == "exit")
	{
		client.Disconnect();
		std::cout << "Exiting\n";
		Stop();
	}
	else
	{
		std::cout << "Unknown command: " << cmd.verb << "\n";
	}
}

void Repl::PrintHelp() const
{
	std::cout << "Commands:\n"
		<< "  connect <ip[:port]>  - Connect to a server\n"
		<< "  disconnect           - Disconnect from server\n"
		<< "  status               - Show connection status\n"
		<< "  move <x> <y> <z>     - Set avatar position\n"
		<< "  turn <qx> <qy> <qz> <qw> - Set avatar orientation (quaternion)\n"
		<< "  input list           - List available inputs\n"
		<< "  input binary <id> <0|1> - Send binary input event\n"
		<< "  input analogue <id> <f> - Send analogue input event\n"
		<< "  input motion <id> <x> <y> - Send motion input event\n"
		<< "  mode <minimal|simulated> - Set client mode\n"
		<< "  geometry             - Summarise streamed geometry\n"
		<< "  geometry nodes       - List tracked nodes\n"
		<< "  geometry resources   - List pointer resource URLs\n"
		<< "  identity             - Show who this client is signed in as\n"
		<< "  signin [provider]    - Sign in; prints a URL and a code to enter elsewhere\n"
		<< "  signout              - Forget the current identity\n"
		<< "  help                 - Show this help\n"
		<< "  quit / exit          - Exit the client\n";
}

void Repl::PrintIdentity() const
{
	auto	   &identity = teleport::client::identity;
	std::cout << "\n=== Identity ===\n";
	std::cout << identity.GetDisplayText() << "\n";
	if (identity.IsSignedIn())
	{
		teleport::client::IdentityProfile profile = identity.GetProfile();
		std::cout << "Provider: " << profile.provider << "\n";
		std::cout << "Subject:  " << profile.subject << "\n";
		if (!profile.email.empty())
			std::cout << "Email:    " << profile.email << " (kept locally; not sent to servers)\n";
	}
	const std::string error = identity.GetLastError();
	if (!error.empty())
		std::cout << "Last error: " << error << "\n";
	std::cout << "Providers:";
	for (const auto &p : identity.GetProviders())
	{
		std::cout << " " << p->GetName();
	}
	std::cout << "\n\n";
}

void Repl::PrintStatus() const
{
	std::cout << "\n=== Client Status ===\n";
	std::cout << client.GetStatus();
	std::cout << "Mode: " << (client.GetMode() == HeadlessMode::Minimal ? "minimal" : "simulated") << "\n";
	std::cout << "\n";
}
