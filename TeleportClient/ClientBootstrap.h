#pragma once

#include <string>

namespace teleport
{
	namespace client
	{
		bool FindClientDirectory();
		//! Initialize the client environment: find asset directory, resolve storage folder,
		//! load configuration, and set up logging. Called identically by graphical and headless clients.
		//! @param cmdLine Command-line arguments (platform-specific, used for single-instance check)
		//! @return True on success, false if a critical error occurred (exit the program)
		bool BootstrapClientEnvironment(const std::string &cmdLine);

		//! Get the resolved storage folder path (e.g. ~/.local/share/TeleportXR on Linux,
		//! %LOCALAPPDATA%\\TeleportXR on Windows). Valid after BootstrapClientEnvironment succeeds.
		std::string GetStorageFolderPath();
	}
}
