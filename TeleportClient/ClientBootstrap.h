#pragma once

#include <filesystem>
#include <string>

namespace teleport
{
	namespace client
	{
		//! Locate the directory holding the client's runtime data: client_default.ini, assets, shaders
		//! and fonts. Returns an empty path if it cannot be found. Has no side effects - in particular
		//! it does not change the working directory - so it is safe to call before the client is set up.
		std::filesystem::path ResolveClientDataDirectory();

		//! Resolve the client data directory (see ResolveClientDataDirectory), remember it for
		//! GetClientDataDirectory(), and make it the working directory.
		bool FindClientDirectory();

		//! The data directory resolved by FindClientDirectory(); empty until that has succeeded.
		//! Prefer this to the working directory: the two coincide today, but only the former is
		//! guaranteed to keep meaning the data directory.
		const std::filesystem::path &GetClientDataDirectory();

		//! Absolute path to a file or directory within the client data directory. The data directory
		//! is read-only once installed, so this is for reading only; write to GetStorageFolderPath().
		std::string GetClientDataPath(const std::string &relativePath);

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
