#pragma once

#include <cstdint>
#include <string>

namespace teleport
{
	class HTTPService
	{
	public:
		virtual bool initialize(std::string directoryPath) = 0;
		virtual void shutdown() = 0;
		virtual void tick() = 0;
	};
}