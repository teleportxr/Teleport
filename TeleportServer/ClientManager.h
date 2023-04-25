#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <map>
#include <set>

#include "libavstream/common.hpp"
#include "../src/platform.hpp"
#include "DiscoveryService.h"

typedef struct _ENetHost ENetHost;

namespace teleport
{
	namespace server
	{
		class ClientMessaging;
		class ClientData;
		//! Container for the client-specific data objects.
		class ClientManager
		{
		public:
			ClientManager();

			virtual ~ClientManager();

			bool initialize(int32_t signalPort, int32_t servPort, std::string client_ip_match="", uint32_t maxClients = 100);
			bool shutdown();
			void tick(float deltaTime);
			bool startSession(avs::uid clientID, std::string clientIP, int servicePort);

			void startAsyncNetworkDataProcessing();
			void stopAsyncNetworkDataProcessing(bool killThread = true);

			void removeClient(avs::uid u);

			bool hasHost() const;
			bool hasClient(avs::uid clientID);
			std::shared_ptr<ClientData> GetClient(avs::uid clientID);
			const std::set<avs::uid> &GetClientUids() const;

			uint16_t getServerPort() const;

			avs::Timestamp getLastTickTimestamp() const;

			bool asyncProcessingFailed() const
			{
				return mAsyncNetworkDataProcessingFailed;
			}

			SignalingService signalingService;
		private:
			void handleStoppedClients();
			void receiveMessages();
			void handleStreaming();
			void processNetworkDataAsync();
			std::set<avs::uid> clientIDs;
			std::atomic_bool mAsyncNetworkDataProcessingFailed = false;
			bool mInitialized = false;
			int32_t mListenPort = 0;

			ENetHost* mHost = nullptr;

			std::atomic_bool mAsyncNetworkDataProcessingActive = false;

			std::map<avs::uid, std::shared_ptr<ClientData>> clients;
			std::thread mNetworkThread;
			std::mutex mNetworkMutex;
			mutable std::mutex mDataMutex;
			std::atomic<avs::Timestamp> mLastTickTimestamp;
			uint32_t mMaxClients = 0;
			std::map<uint16_t,bool> mPorts;

			// Seconds
			static constexpr float mStartSessionTimeout = 3;
			uint16_t servicePort = 0;
		};
	}
}