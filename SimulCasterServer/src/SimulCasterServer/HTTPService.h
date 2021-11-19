#pragma once

#include <cstdint>
#include <string>

namespace teleport
{
	class HTTPService
	{
	public:
		virtual bool initialize(std::string mountDirectory, std::string certPath, std::string privateKeyPath, int32_t port) = 0;
		virtual void shutdown() = 0;
		virtual void tick() = 0;
	};
}