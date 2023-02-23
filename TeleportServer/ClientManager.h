#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "libavstream/common.hpp"
#include "../src/platform.hpp"

typedef struct _ENetHost ENetHost;

namespace teleport
{
	namespace server
	{
		class ClientMessaging;
		//! Container for the client-specific data objects.
		class ClientManager
		{
		public:
			ClientManager();

			virtual ~ClientManager();

			bool initialize(int32_t listenPort, uint32_t maxClients = 100);
			bool shutdown();
			void tick(float deltaTime);

			void startAsyncNetworkDataProcessing();
			void stopAsyncNetworkDataProcessing(bool killThread = true);

			void addClient(ClientMessaging* client);
			void removeClient(ClientMessaging* client);

			bool hasHost() const;
			bool hasClient(avs::uid clientID);
			uint16_t getServerPort() const;

			avs::Timestamp getLastTickTimestamp() const;

			bool asyncProcessingFailed() const
			{
				return mAsyncNetworkDataProcessingFailed;
			}

		private:
			void handleMessages();
			void handleStreaming();
			void processNetworkDataAsync();

			std::atomic_bool mAsyncNetworkDataProcessingFailed = false;
			bool mInitialized = false;
			int32_t mListenPort = 0;

			ENetHost* mHost = nullptr;

			std::atomic_bool mAsyncNetworkDataProcessingActive = false;

			std::vector<ClientMessaging*> mClients;
			std::thread mNetworkThread;
			std::mutex mNetworkMutex;
			mutable std::mutex mDataMutex;
			avs::Timestamp mLastTickTimestamp;
			uint32_t mMaxClients = 0;
			std::vector<bool> mPorts;

			// Seconds
			static constexpr float mStartSessionTimeout = 3;
		};
	}
}