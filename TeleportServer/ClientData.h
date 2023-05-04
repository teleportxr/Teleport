#pragma once

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "TeleportServer/ClientManager.h"
#include "TeleportServer/ClientMessaging.h"
#include "TeleportServer/ServerSettings.h"
#include "TeleportServer/AudioEncodePipeline.h"
namespace teleport
{
	namespace server
	{
		class VideoEncodePipeline;
		class AudioEncodePipeline;
		typedef int64_t(* GetUnixTimestampFn)();	// was __stdcall*
		enum ConnectionState
		{
			UNCONNECTED,
			DISCOVERED,
			CONNECTED
		};
		enum class ReflectedStateStatus
		{
			UNSENT=0,SENT,CONFIRMED
		};
		/// <summary>
		/// A helper struct for a state that should be refleced client-side. Including a boolean flag for whether that state has been confirmed by the client.
		/// </summary>
		/// <typeparam name="State">The state's type</typeparam>
		template<typename State> struct ReflectedState
		{
			State state;
			ReflectedStateStatus status = ReflectedStateStatus::UNSENT;
		};
		template<typename State> struct ReflectedStateMap :public std::map<avs::uid, ReflectedState<State>>
		{
		};
		
		//! Data object for a connected client.
		class ClientData
		{
		public:
			ClientData(  std::shared_ptr<teleport::server::ClientMessaging> clientMessaging);
			void StartStreaming(const ServerSettings &casterSettings
				, uint32_t connectionTimeout
				, avs::uid serverID
				, GetUnixTimestampFn getUnixTimestamp
				, bool use_ssl);
			void setNodePosePath(avs::uid nodeID, const std::string &regexPosePath);
			void setInputDefinitions(const std::vector<teleport::core::InputDefinition> &inputDefs);
			// client settings from engine-side:
			ClientSettings clientSettings;
			avs::ClientDynamicLighting clientDynamicLighting;
			std::vector<teleport::core::InputDefinition> inputDefinitions;

			std::shared_ptr<VideoEncodePipeline> videoEncodePipeline;
			std::shared_ptr<AudioEncodePipeline> audioEncodePipeline;
			std::shared_ptr<teleport::server::ClientMessaging> clientMessaging;

			void SetConnectionState(ConnectionState c)
			{
				connectionState = c;
			}
			ConnectionState GetConnectionState() const
			{
				return connectionState;
			}

			bool validClientSettings = false;
			bool videoKeyframeRequired = false;

			bool setOrigin(uint64_t ctr,avs::uid uid);
			bool hasOrigin() const;
			avs::uid getOrigin() const;

			void setGlobalIlluminationTextures(size_t num, const avs::uid *uids);
			const std::vector<avs::uid> &getGlobalIlluminationTextures() const
			{
				return global_illumination_texture_uids;
			}
			void tick(float deltaTime);
		protected:
			ConnectionState connectionState = UNCONNECTED;
			mutable bool _hasOrigin=false;
			avs::uid originClientHas=0;
			std::vector<avs::uid> global_illumination_texture_uids;
			struct NodeSubtypeState
			{
				std::string regexPath;
			};
			ReflectedStateMap<NodeSubtypeState> nodeSubTypes;
		};
		struct ClientStatus
		{
			unsigned ipAddress;
		};
	}
}
