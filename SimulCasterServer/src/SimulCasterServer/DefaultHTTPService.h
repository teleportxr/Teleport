#pragma once

#include "SimulCasterServer/HTTPService.h"
#include <map>
#include <memory>
#include <thread>

namespace httplib
{
	class Server;
	class SSLServer;
}

namespace teleport
{
	class DefaultHTTPService : public HTTPService
	{
	public:
		DefaultHTTPService();
		~DefaultHTTPService();

		bool initialize(std::string mountDirectory, std::string certPath, std::string privateKeyPath, int32_t port) override;

		void shutdown() override;

		void tick() override;

	private:
		std::unique_ptr<httplib::SSLServer> mServer;
		std::thread mThread;
	};
}