#pragma once

#include "TeleportClient/DiscoveryService.h"
namespace teleport
{
	namespace android
	{
		class AndroidDiscoveryService: public teleport::client::DiscoveryService
		{
		public:
			AndroidDiscoveryService();
			virtual ~AndroidDiscoveryService();

		};
	}
}
