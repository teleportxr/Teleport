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
			//! Load config, bookmarks and options, and start the client clock. Must be called once,
			//! after the storage folder has been resolved. Repeat calls are ignored.
			void Initialize();
			bool IsInitialized() const
			{
				return initialized;
			}

		private:
			bool initialized = false;
		};
	}
}