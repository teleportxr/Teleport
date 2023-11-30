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


namespace teleport
{
	namespace server
	{
#pragma pack(push)
#pragma pack(1)
		struct SessionState
		{
			uint64_t sessionId = 0;
		} AVS_PACKED;
#pragma pack(pop)
		class ClientMessaging;
		class ClientData;
		struct ClientSettings;
		//! Container for the client-specific data objects.
		class ClientManager
		{
		public:
			ClientManager();

			virtual ~ClientManager();

			static ClientManager &instance();
			bool initialize(std::set<uint16_t> signalPorts, int64_t start_unix_time_ns, std::string client_ip_match="", uint32_t maxClients = 100);
			bool shutdown();
			void tick(float deltaTime);
			bool startSession(avs::uid clientID, std::string clientIP);
			const ClientSettings *GetClientSettings(avs::uid clientID) 
			{
				auto f = clientSettings.find(clientID);
				if (f == clientSettings.end())
					return nullptr;
				return f->second.get();
			}

			void startAsyncNetworkDataProcessing();
			void stopAsyncNetworkDataProcessing(bool killThread = true);

			void removeClient(avs::uid u);
			void SetClientSettings(avs::uid clientID, const struct ClientSettings &clientSettings);

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

			const SessionState &getSessionState() const
			{
				return sessionState;
			}
		private:
			SessionState sessionState;
			void startStreaming(avs::uid clientID);
			void handleStoppedClients();
			void receiveMessages();
			void handleStreaming();
			void processNetworkDataAsync();
			std::set<avs::uid> clientIDs;
			std::atomic_bool mAsyncNetworkDataProcessingFailed = false;
			bool mInitialized = false;
			int64_t startTimestamp_utc_unix_ns = 0;
			std::atomic_bool mAsyncNetworkDataProcessingActive = false;
		public:
			std::map<avs::uid, std::shared_ptr<ClientData>> clients;
			std::map<avs::uid, std::shared_ptr<struct ClientSettings>> clientSettings;
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