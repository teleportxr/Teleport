#pragma once
#include <string>

namespace teleport
{
	namespace core
	{
		struct DomainPortPath
		{
			std::string protocol;
			std::string domain;
			int port = 0;
			std::string path;
		};
		extern DomainPortPath GetDomainPortPath(const std::string &url);
	}
}