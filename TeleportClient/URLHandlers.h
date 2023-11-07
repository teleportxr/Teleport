#pragma once

#include <string>

namespace teleport
{
	namespace client
	{
		extern std::string GetLauncherForProtocol(std::string protocol);
		extern void LaunchProtocolHandler(std::string url);
		extern void LaunchProtocolHandler(std::string protocol, std::string url);
	}
}