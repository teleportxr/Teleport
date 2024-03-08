#pragma once

#include <atomic>
#include <functional>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <map>
#include <set>
#include <safe/safe.h>

#include "libavstream/common.hpp"
#include "../src/platform.hpp"
#include "DefaultHTTPService.h"
#include "SignalingService.h"
#include "TeleportServer/ServerSettings.h"
#include "TeleportServer/ClientMessaging.h"
#include "TeleportServer/ClientData.h"


typedef void(TELEPORT_STDCALL *ProcessAudioInputFn)(avs::uid uid, const uint8_t *data, size_t dataSize);
namespace teleport
{
	namespace server
	{
		extern avs::Context avsContext;
		extern ServerSettings serverSettings;

		extern std::unique_ptr<DefaultHTTPService> httpService;
		extern SetHeadPoseFn setHeadPose;
		extern SetControllerPoseFn setControllerPose;
		extern ProcessNewInputStateFn processNewInputState;
		extern ProcessNewInputEventsFn processNewInputEvents;
		extern DisconnectFn onDisconnect;
		extern ProcessAudioInputFn processAudioInput;
		extern GetUnixTimestampFn getUnixTimestampNs;
		extern ReportHandshakeFn reportHandshake;
		extern uint32_t connectionTimeout;
		/// The collected values required to initialize a server session; see Server_Teleport_Initialize().
		struct InitializationSettings
		{
			char *clientIP;			   ///< IP address to match to connecting clients. May be blank.
			char *httpMountDirectory;  ///< Local (server-side) directory for HTTP requests: usually the Teleport cache directory.
			char *certDirectory;	   ///< Local directory for HTTP certificates.
			char *privateKeyDirectory; ///< Local directory for private keys.
			char *signalingPorts;	   ///< Optional list of ports to listen for signaling connections and queries.

			ClientStoppedRenderingNodeFn clientStoppedRenderingNode; ///< Delegate to be called when client is no longer rendering a specified node.
			ClientStartedRenderingNodeFn clientStartedRenderingNode;
			SetHeadPoseFn headPoseSetter;
			SetControllerPoseFn controllerPoseSetter;
			ProcessNewInputStateFn newInputStateProcessing;
			ProcessNewInputEventsFn newInputEventsProcessing;
			DisconnectFn disconnect;
			avs::MessageHandlerFunc messageHandler;
			ReportHandshakeFn reportHandshake;
			ProcessAudioInputFn processAudioInput;
			GetUnixTimestampFn getUnixTimestampNs;
			int64_t start_unix_time_us;
		};
		extern bool ApplyInitializationSettings(const InitializationSettings *initializationSettings);
	}
}

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
			avs::uid popFirstUnlinkedClientUid();
			bool initialize(std::set<uint16_t> signalPorts, int64_t start_unix_time_us, std::string client_ip_match = "", uint32_t maxClients = 100);
			bool shutdown();
			void tick(float deltaTime);
			bool startSession(avs::uid clientID, std::string clientIP);
			void stopClient(avs::uid clientID);
			const ClientSettings *GetClientSettings(avs::uid clientID) 
			{
				auto f = clientSettings.find(clientID);
				if (f == clientSettings.end())
					return nullptr;
				return f->second.get();
			}

			void startAsyncNetworkDataProcessing();
			void stopAsyncNetworkDataProcessing(bool killThread = true);
			void InitializeVideoEncoder(avs::uid clientID, VideoEncodeParams &videoEncodeParams);
			void ReconfigureVideoEncoder(avs::uid clientID, VideoEncodeParams &videoEncodeParams);
			void EncodeVideoFrame(avs::uid clientID, const uint8_t *tagData, size_t tagDataSize);
			void SendAudio(const uint8_t *data, size_t dataSize);
			void removeLostClient(avs::uid u);
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
			int64_t startTimestamp_utc_unix_us = 0;
			std::atomic_bool mAsyncNetworkDataProcessingActive = false;
		public:
			std::map<avs::uid, std::shared_ptr<ClientData>> clients;
			std::map<avs::uid, std::shared_ptr<struct ClientSettings>> clientSettings;
			std::mutex audioMutex;
			std::mutex videoMutex;
			AudioSettings audioSettings;

			std::set<avs::uid> unlinkedClientIDs; // Client IDs that haven't been linked to a session component.
		
		protected:
			std::thread mNetworkThread;
			std::shared_mutex clientsMutex;
			mutable std::mutex mDataMutex;
			std::atomic<avs::Timestamp> mLastTickTimestamp;
			uint32_t mMaxClients = 0;

			// Seconds
			static constexpr float mStartSessionTimeout = 3;
			std::set<avs::uid> lostClients; // Clients who have been lost, and are awaiting deletion.
		};
	}
}