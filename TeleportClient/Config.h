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
		class Config
		{
		public:
			void LoadConfigFromIniFile();
			std::vector<std::string> server_ips;
			bool enable_vr = true;
			bool dev_mode = false;
			bool render_local_offline = false;
			std::string log_filename="TeleportClient.log";
		};
	}
}