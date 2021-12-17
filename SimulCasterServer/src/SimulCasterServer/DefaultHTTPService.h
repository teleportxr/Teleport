#pragma once

#include "SimulCasterServer/HTTPService.h"
#include <map>
#include <memory>
#include <thread>

namespace httplib
{
	class Server;
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

		bool isUsingSSL() const { return mUsingSSL; }

	private:
		std::unique_ptr<httplib::Server> mServer;
		std::thread mThread;
		bool mUsingSSL;
	};
}