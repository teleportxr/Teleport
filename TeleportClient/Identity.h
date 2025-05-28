#pragma once
#include <string>
namespace teleport
{
	namespace client
	{
		//! Abstract discovery service for clients to connect to the server.
		class Identity
		{
		public:
			void Init();
			std::string identity;
		};
		extern Identity identity;
	}
}