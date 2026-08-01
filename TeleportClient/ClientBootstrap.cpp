#include "ClientBootstrap.h"
#include "Config.h"
#include "Platform/Core/FileLoader.h"
#include "TeleportCore/Logging.h"
#include <filesystem>
#include <cstring>

#ifdef _WIN32
	#include <shlobj_core.h>
#else
	#include <unistd.h>
	#include <pwd.h>
#endif

namespace teleport
{
	namespace client
	{
		static std::string s_storage_folder;

		bool BootstrapClientEnvironment(const std::string &cmdLine)
		{
			(void)cmdLine; // Currently unused (EnsureSingleProcess is called separately in main())

			auto *fileLoader = platform::core::FileLoader::GetFileLoader();
			fileLoader->SetRecordFilesLoaded(true);

			// Find the pc_client directory by searching for client/client_default.ini
			std::filesystem::path current_path = std::filesystem::current_path();
			if (!std::filesystem::exists("client/client_default.ini"))
			{
#ifdef _WIN32
				// Windows: try to find it relative to executable
				wchar_t filename[700];
				DWORD res = GetModuleFileNameW(nullptr, filename, 700);
				if (res)
				{
					current_path = filename;
					current_path = current_path.remove_filename();
				}
#else
				// Linux: try to find it relative to executable
				char exe_path[1024];
				ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
				if (len != -1)
				{
					exe_path[len] = '\0';
					current_path = std::filesystem::path(exe_path).parent_path();
				}
#endif
				// Search up the directory tree for client/client_default.ini
				while (!current_path.empty() && !std::filesystem::exists("client/client_default.ini"))
				{
					std::filesystem::path prev_path = current_path;
					current_path = current_path.append("../").lexically_normal();
					if (prev_path == current_path)
						break;
					if (std::filesystem::exists(current_path))
						std::filesystem::current_path(current_path);
					else
						break;
				}
			}

			current_path = current_path.append("client").lexically_normal();
			if (!std::filesystem::exists(current_path))
			{
				TELEPORT_WARN("Cannot find client directory");
				return false;
			}
			std::filesystem::current_path(current_path);

			// Resolve platform-specific storage folder
#ifdef _WIN32
			// Windows: use CSIDL_LOCAL_APPDATA
			char szPath[MAX_PATH];
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
					home = pw->pw_dir;
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

			return true;
		}

		std::string GetStorageFolderPath()
		{
			return s_storage_folder;
		}
	}
}
