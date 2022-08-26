#pragma once
#include "TeleportClient/Config.h"
namespace teleport
{
	namespace client
	{
		//! The main class for the client application. Lifetime for the whole application, and shared
		//! across all connections.
		class ClientApp
		{
		public:
			ClientApp();
			~ClientApp();
			void Initialize();
			teleport::client::Config config;
		};
	}
}