#include "HeadlessClient.h"
#include "Repl.h"
#include "TeleportClient/Config.h"
#include "TeleportCore/ErrorHandling.h"
#include "TeleportCore/Logging.h"
#include "Platform/Core/FileLoader.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <filesystem>

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

int main(int argc, char *argv[])
{
	// Parse command line
	std::string cmdLine;
	for (int i = 1; i < argc; i++)
	{
		if (cmdLine.length() > 0)
			cmdLine += " ";
		cmdLine += argv[i];
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

	TELEPORT_LOG("Teleport Headless Client starting");

	// Setup signal handlers for graceful shutdown
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);

	try
	{
		HeadlessClient headlessClient;
		Repl repl(headlessClient);

		// Start REPL thread (blocking on stdin)
		std::thread replThread([&repl]() {
			repl.Run();
		});

		// Tick thread: update client at a fixed rate (20 Hz for M1)
		const double tickInterval = 1.0 / 20.0;
		double currentTime = 0.0;
		auto lastTickTime = std::chrono::high_resolution_clock::now();

		while (running && !repl.IsStopping())
		{
			auto now = std::chrono::high_resolution_clock::now();
			double elapsedMs = std::chrono::duration<double, std::milli>(now - lastTickTime).count();
			double elapsedSecs = elapsedMs / 1000.0;

			if (elapsedSecs >= tickInterval)
			{
				headlessClient.TickOnce(currentTime, tickInterval);
				currentTime += tickInterval;
				lastTickTime = now;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		// Shutdown
		repl.Stop();
		headlessClient.Disconnect();

		if (replThread.joinable())
		{
			replThread.join();
		}

		TELEPORT_LOG("Teleport Headless Client exiting");
		return 0;
	}
	catch (const std::exception &e)
	{
		TELEPORT_WARN("Fatal error: {}", e.what());
		return 2;
	}
}
