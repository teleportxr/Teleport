#include "ConnectionManager.h"
#include "ControlProtocol.h"
#include "ControlServer.h"
#include "TeleportClient/Config.h"
#include "TeleportClient/GoogleDeviceIdentityProvider.h"
#include "TeleportClient/GuestIdentityProvider.h"
#include "TeleportClient/Identity.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "Platform/Core/FileLoader.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <shlobj_core.h>
#else
	#include <unistd.h>
	#include <pwd.h>
#endif

std::atomic<bool> running{true};

void SignalHandler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
	{
		running = false;
	}
}

static void PrintUsage(const char *argv0)
{
	std::cout
		<< "Usage: " << argv0 << " [-p port] [-?]\n"
		<< "Teleport streaming service. Holds live server connections and takes\n"
		<< "one-line commands from teleport_cli over 127.0.0.1 (default port "
		<< teleport_control::DEFAULT_PORT << ").\n"
		<< "  -p <port>  Control port (also TELEPORT_SERVICE_PORT)\n"
		<< "  -?         This help\n";
}

int main(int argc, char *argv[])
{
	// Control port: -p <port> (or -p<port>), else TELEPORT_SERVICE_PORT, else default.
	uint16_t port = teleport_control::DEFAULT_PORT;
	if (const char *envPort = std::getenv("TELEPORT_SERVICE_PORT"))
	{
		if (int p = std::atoi(envPort); p > 0 && p < 65536)
			port = static_cast<uint16_t>(p);
	}
	for (int i = 1; i < argc; i++)
	{
		const char *arg = argv[i];
		if (!std::strcmp(arg, "-?"))
		{
			PrintUsage(argv[0]);
			return 0;
		}
		if (!std::strcmp(arg, "-p"))
		{
			if (i + 1 >= argc)
			{
				std::cerr << argv[0] << ": option requires an argument -- p\n";
				PrintUsage(argv[0]);
				return 2;
			}
			port = static_cast<uint16_t>(std::atoi(argv[++i]));
		}
		else if (!std::strncmp(arg, "-p", 2) && arg[2] != '\0')
		{
			port = static_cast<uint16_t>(std::atoi(arg + 2));
		}
	}

	// Initialize config
	auto &config = teleport::client::Config::GetInstance();
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	fileLoader->SetRecordFilesLoaded(true);

#ifdef _WIN32
	char szPath[MAX_PATH];
	HRESULT hResult = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, szPath);
	std::string storage_folder;
	if (hResult == S_OK)
	{
		storage_folder = std::string(szPath) + "/TeleportXR";
	}
#else
	const char *home = getenv("HOME");
	if (!home)
	{
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			home = pw->pw_dir;
	}
	std::string storage_folder;
	if (home)
	{
		storage_folder = std::string(home) + "/.local/share/TeleportXR";
		std::filesystem::create_directories(storage_folder);
	}
#endif

	if (storage_folder.empty())
	{
		storage_folder = std::filesystem::current_path().string();
	}

	if (storage_folder.length() > 200)
	{
		TELEPORT_WARN("Storage path is too long: {}", storage_folder);
		return 1;
	}

	config.SetStorageFolder(storage_folder.c_str());
	config.LoadConfigFromIniFile();

	// Identity: this client has no browser, so Google sign-in uses the device authorization
	// grant — the user is given a code to enter on a phone or another computer. Registering the
	// providers here means Identity::Init() does not fall back to the GUI client's loopback flow,
	// which would need a browser on this machine. Init() itself only restores a remembered
	// sign-in; nothing prompts the user until a control client sends "signin".
	auto &identity = teleport::client::identity;
	identity.RegisterProvider(std::make_shared<teleport::client::GoogleDeviceIdentityProvider>());
	identity.RegisterProvider(std::make_shared<teleport::client::GuestIdentityProvider>());
	identity.Init();

	TELEPORT_LOG("Teleport service starting");

	// Setup signal handlers for graceful shutdown
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);

	try
	{
		ConnectionManager connectionManager;
		ControlServer server(connectionManager);
		if (!server.Start(port))
		{
			TELEPORT_WARN("Could not listen on 127.0.0.1:{} — is another instance running?", port);
			return 1;
		}
		TELEPORT_LOG("Control interface listening on 127.0.0.1:{}", port);

		// Tick loop: update all connections at a fixed rate (20 Hz).
		const double tickInterval = 1.0 / 20.0;
		double currentTime = 0.0;
		auto lastTickTime = std::chrono::high_resolution_clock::now();

		while (running && !server.ShutdownRequested())
		{
			auto now = std::chrono::high_resolution_clock::now();
			double elapsedMs = std::chrono::duration<double, std::milli>(now - lastTickTime).count();
			double elapsedSecs = elapsedMs / 1000.0;

			if (elapsedSecs >= tickInterval)
			{
				// Applies the result of a sign-in running on the identity worker thread.
				// Process-global, so done once per tick rather than per connection.
				identity.Update();
				connectionManager.TickAll(currentTime, tickInterval);
				currentTime += tickInterval;
				lastTickTime = now;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		// Shutdown: prompt, because Stop() wakes accept() with a self-connect and
		// shuts client sockets down (the old stdin REPL used to hang on getline).
		server.Stop();
		identity.Shutdown();
		connectionManager.DestroyAll();

		TELEPORT_LOG("Teleport service exiting");
		return 0;
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("Fatal error: {}", e.what());
		return 2;
	}
}
