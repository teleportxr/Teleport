// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once
#include <string>
#include <vector>

constexpr unsigned int TELEPORT_TIMEOUT = 1000;

namespace teleport
{
	namespace client
	{
		struct Bookmark
		{
			std::string url;
			std::string title;
		};
		enum class LobbyView:uint8_t
		{
			WHITE,
			NEON
		};
		enum class StartupConnectOption:uint8_t
		{
			NONE=0,
			HOME,
			LAST
		};
		struct Options
		{
			LobbyView lobbyView;
			bool showGeometryOffline = false;
			StartupConnectOption startupConnectOption = StartupConnectOption::LAST;
			unsigned int connectionTimeout=TELEPORT_TIMEOUT;
			bool simulateVR = false;
			bool passThrough=false;
			bool mode2D=false;
		};
		struct DebugOptions
		{
			bool showAxes = false;
			bool showOverlays = false;
			bool showStageSpace = false;
			bool useDebugShader = false;
			bool enableTextureTranscodingThread = true;
			bool enableGeometryTranscodingThread = true;
			std::string debugShader;
		};
		class Config
		{
			std::vector<Bookmark> bookmarks;
			std::string storageFolder="storage";

		public:
			static Config &GetInstance();
			void LoadConfigFromIniFile();
			std::vector<std::string> recent_server_urls;
			int pause_for_debugger=0;
			bool enable_vr = true;
#if TELEPORT_INTERNAL_CHECKS
			bool dev_mode=true;
#else
			bool dev_mode=false;
#endif
			std::string log_filename="TeleportClient.log";
			
			Options options;
			DebugOptions debugOptions;
			void LoadOptions();
			void SaveOptions();
			const std::vector<std::string> &GetRecent() const;
			const std::vector<Bookmark> &GetBookmarks() const;
			void AddBookmark(const Bookmark &b);
			void LoadBookmarks();
			void SaveBookmarks();
			//! When we connect, store the URL
			void StoreRecentURL(const char *r);
			//! Where do we store temp files?
			void SetStorageFolder(const char *f);
			const std::string &GetStorageFolder() const;
		};
	}
}
