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
#include "SignalingService.h"

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

			bool initialize(std::set<uint16_t> signalPorts, std::string client_ip_match="", uint32_t maxClients = 100);
			bool shutdown();
			void tick(float deltaTime);
			bool startSession(avs::uid clientID, std::string clientIP);

			void startAsyncNetworkDataProcessing();
			void stopAsyncNetworkDataProcessing(bool killThread = true);

			void removeClient(avs::uid u);

			bool hasHost() const;
			bool hasClient(avs::uid clientID);
			std::shared_ptr<ClientData> GetClient(avs::uid clientID);
			const std::set<avs::uid> &GetClientUids() const;

			avs::Timestamp getLastTickTimestamp() const;

			bool asyncProcessingFailed() const
			{
				return mAsyncNetworkDataProcessingFailed;
			}

			SignalingService signalingService;
		private:
			void startStreaming(avs::uid clientID);
			void handleStoppedClients();
			void receiveMessages();
			void handleStreaming();
			void processNetworkDataAsync();
			std::set<avs::uid> clientIDs;
			std::atomic_bool mAsyncNetworkDataProcessingFailed = false;
			bool mInitialized = false;
			uint64_t sessionID=0;

			std::atomic_bool mAsyncNetworkDataProcessingActive = false;
		public:
			std::map<avs::uid, std::shared_ptr<ClientData>> clients;
		protected:
			std::thread mNetworkThread;
			std::mutex mNetworkMutex;
			mutable std::mutex mDataMutex;
			std::atomic<avs::Timestamp> mLastTickTimestamp;
			uint32_t mMaxClients = 0;

			// Seconds
			static constexpr float mStartSessionTimeout = 3;
		};
	}
}