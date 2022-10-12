// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once
#include <string>
#include <vector>

constexpr char const* TELEPORT_SERVER_IP = "127.0.0.1";
constexpr unsigned int TELEPORT_SERVER_PORT = 10500;
constexpr unsigned int TELEPORT_CLIENT_SERVICE_PORT = 10501;
constexpr unsigned int TELEPORT_CLIENT_STREAMING_PORT = 10502;
constexpr unsigned int TELEPORT_CLIENT_DISCOVERY_PORT = 10599;
constexpr unsigned int TELEPORT_SERVER_DISCOVERY_PORT = 10600;

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
		struct Options
		{
			LobbyView lobbyView;
			bool showGeometryOffline = false;
		};
		class Config
		{
			std::vector<Bookmark> bookmarks;
			std::string storageFolder="";
			const std::string &GetStoragePath() const;
		public:
			void LoadConfigFromIniFile();
			std::vector<std::string> recent_server_urls;
			bool enable_vr = true;
#if TELEPORT_INTERNAL_CHECKS
			bool dev_mode=true;
#else
			bool dev_mode=false;
#endif
			std::string log_filename="TeleportClient.log";
			
			Options options;
			void LoadOptions();
			void SaveOptions();
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