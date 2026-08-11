#include "ClientBootstrap.h"
#include "Config.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/Logging.h"
#include "TeleportCore/StringFunctions.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <shlobj_core.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace
{
	//! The file that identifies a directory as the client data directory.
	const char *const kClientDataMarker		= "client_default.ini";

	//! Names to try for the data directory, relative to a candidate root, in priority order:
	//!  - "share/teleportxr" is the installed layout, /opt/teleportxr/share/teleportxr, which is
	//!    also what /usr/share/teleportxr would look like if the client is ever packaged for a
	//!    distribution archive - only the prefix differs, so this name need not change.
	//!  - "client" is the source tree, where the data sits beside the build directory.
	const char *const kClientDataDirNames[] = {"share/teleportxr", "client"};

	//! The directory containing the running executable, or an empty path if it cannot be determined.
	std::filesystem::path ExecutableDirectory()
	{
#ifdef _WIN32
		wchar_t filename[700];
		DWORD	res = GetModuleFileNameW(nullptr, filename, 700);
		if (!res)
		{
			return {};
		}
		return std::filesystem::path(filename).parent_path();
#elif defined(__APPLE__)
		char	 exe_path[1024];
		uint32_t size = sizeof(exe_path);
		if (_NSGetExecutablePath(exe_path, &size) != 0)
		{
			return {};
		}
		return std::filesystem::path(exe_path).parent_path();
#else
		char	exe_path[1024];
		ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (len == -1)
		{
			return {};
		}
		exe_path[len] = '\0';
		return std::filesystem::path(exe_path).parent_path();
#endif
	}

	//! Whichever of kClientDataDirNames exists directly below root, or an empty path if none does.
	std::filesystem::path DataDirectoryBelow(const std::filesystem::path &root)
	{
		if (root.empty())
		{
			return {};
		}
		std::error_code ec;
		for (const char *name : kClientDataDirNames)
		{
			std::filesystem::path candidate = root / name;
			if (std::filesystem::exists(candidate / kClientDataMarker, ec))
			{
				return candidate;
			}
		}
		return {};
	}
}

namespace teleport
{
	namespace client
	{
		static std::string			 s_storage_folder;
		static std::filesystem::path s_client_data_dir;

		std::filesystem::path		 ResolveClientDataDirectory()
		{
			std::error_code ec;
			// An explicit override wins, so that an uninstalled build can be pointed at any data directory.
			std::string env = teleport::core::GetEnvVar("TELEPORT_CLIENT_DATA_DIR");
			if (!env.empty())
			{
				std::filesystem::path dir(env);
				if (std::filesystem::exists(dir / kClientDataMarker, ec))
				{
					return dir;
				}
				TELEPORT_WARN("TELEPORT_CLIENT_DATA_DIR is set to {}, which contains no {}. Ignoring it.", env, kClientDataMarker);
			}
			// The working directory, for a client started from the directory above its data.
			std::filesystem::path dir	= std::filesystem::current_path(ec);
			std::filesystem::path found = DataDirectoryBelow(dir);
			if (!found.empty())
			{
				return found;
			}
			while (!dir.empty())
			{
				found = DataDirectoryBelow(dir);
				if (!found.empty())
				{
					return found;
				}
				std::filesystem::path parent = dir.parent_path();
				if (parent == dir)
				{
					break;
				}
				dir = parent;
			}
			// Then the executable's directory and each directory above it: bin/ -> the install prefix
			// for a packaged client, and the build output directory -> the source root for a dev build.
			//
			// Unconditional, not gated on the CWD walk above having left `dir` empty: on POSIX,
			// parent_path() of the root ("/") is "/" itself, so the while loop above always exits
			// via the "parent == dir" break with `dir == "/"`, never empty - a gate here on
			// dir.empty() was therefore always false and this fallback never ran on any platform.
			// Exactly the case that matters for TeleportPCClient.app launched from /Applications:
			// the working directory a GUI app is launched with has nothing to do with where it's
			// installed, so the CWD walk above never finds anything and this is the only path left.
			{
				std::filesystem::path dir = ExecutableDirectory();
				while (!dir.empty())
				{
					found = DataDirectoryBelow(dir);
					if (!found.empty())
					{
						return found;
					}
					std::filesystem::path parent = dir.parent_path();
					if (parent == dir)
					{
						break;
					}
					dir = parent;
				}
			}
			return {};
		}

		bool FindClientDirectory()
		{
			std::filesystem::path dir = ResolveClientDataDirectory();
			if (dir.empty())
			{
				TELEPORT_WARN("Cannot find the client data directory: no {} below the working directory or the executable.", kClientDataMarker);
				return false;
			}
			std::error_code ec;
			s_client_data_dir = std::filesystem::absolute(dir, ec).lexically_normal();
			// The renderer, GUI and shader loader all read through paths relative to the data
			// directory ("assets/localGeometryCache/...", "textures", "assets/shaders"), so it also
			// becomes the working directory. Nothing may *write* through those relative paths: once
			// installed the data directory belongs to root. Write to GetStorageFolderPath() instead.
			std::filesystem::current_path(s_client_data_dir, ec);
			if (ec)
			{
				TELEPORT_WARN("Cannot enter the client data directory {}: {}", s_client_data_dir.string(), ec.message());
				return false;
			}
			return true;
		}

		const std::filesystem::path &GetClientDataDirectory()
		{
			return s_client_data_dir;
		}

		std::string GetClientDataPath(const std::string &relativePath)
		{
			if (s_client_data_dir.empty())
			{
				return relativePath;
			}
			return (s_client_data_dir / relativePath).lexically_normal().string();
		}
		bool BootstrapClientEnvironment(const std::string &cmdLine)
		{
			(void)cmdLine; // Currently unused (EnsureSingleProcess is called separately in main())

			auto *fileLoader = platform::core::FileLoader::GetFileLoader();
			fileLoader->SetRecordFilesLoaded(true);

			// Resolve platform-specific storage folder
#ifdef _WIN32
			// Windows: use CSIDL_LOCAL_APPDATA
			char	szPath[MAX_PATH];
			HRESULT hResult = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, szPath);
			if (hResult == S_OK)
			{
				s_storage_folder = std::string(szPath) + "/TeleportXR";
			}
#else
			// Linux: use ~/.local/share/TeleportXR
			const char *home = getenv("HOME");
			if (!home)
			{
				struct passwd *pw = getpwuid(getuid());
				if (pw)
				{
					home = pw->pw_dir;
				}
			}
			if (home)
			{
				s_storage_folder = std::string(home) + "/.local/share/TeleportXR";
				std::filesystem::create_directories(s_storage_folder);
			}
#endif

			// Fallback if resolution failed
			if (s_storage_folder.empty())
			{
				s_storage_folder = std::filesystem::current_path().string();
			}

			// Validate path length
			if (s_storage_folder.length() > 200)
			{
				TELEPORT_WARN("Storage path is too long: {}", s_storage_folder);
				s_storage_folder = std::filesystem::current_path().string();
				if (s_storage_folder.length() > 200)
				{
					TELEPORT_WARN("Storage path still too long: {}", s_storage_folder);
					return false;
				}
			}

			// Initialize config
			auto &config = Config::GetInstance();
			config.SetStorageFolder(s_storage_folder.c_str());
			config.LoadConfigFromIniFile();

			return true;
		}

		std::string GetStorageFolderPath()
		{
			return s_storage_folder;
		}
	}
}
