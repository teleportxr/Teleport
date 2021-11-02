#pragma once

#include "SimulCasterServer/HTTPService.h"
#include <map>
#include <memory>

namespace httplib
{
	class Server;
}

namespace teleport
{
	class DefaultHTTPService : public HTTPService
	{
	public:
		~DefaultHTTPService()
		{
			shutdown();
		}

		bool initialize(std::string directoryPath) override;

		void shutdown() override;

		void tick() override;

	private:
		std::unique_ptr<httplib::Server> mServer;
	};
}