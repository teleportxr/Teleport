#pragma once

#include "libavstream/common.hpp"

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
		/// A helper struct for a state that should be reflected client-side. Including a boolean flag for whether that state has been confirmed by the client.
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
			ClientData(avs::uid clientID, std::shared_ptr<teleport::server::ClientMessaging> clientMessaging);
			void StartStreaming( uint32_t connectionTimeout
				, uint64_t sessionid
				, GetUnixTimestampFn getUnixTimestamp, int64_t startTimestamp_utc_unix_us
				, bool use_ssl);
			void tick(float deltaTime);
			void setNodePosePath(avs::uid nodeID, const std::string &regexPosePath);
			//! Called after reparenting to inform the client of the new parent.
			void reparentNode(avs::uid nodeID);
			void setInputDefinitions(const std::vector<teleport::core::InputDefinition> &inputDefs);
			teleport::core::ClientDynamicLighting clientDynamicLighting;
			std::vector<teleport::core::InputDefinition> inputDefinitions;

			std::shared_ptr<VideoEncodePipeline> videoEncodePipeline;
			std::shared_ptr<AudioEncodePipeline> audioEncodePipeline;
			std::shared_ptr<teleport::server::ClientMessaging> clientMessaging;

			void SetConnectionState(ConnectionState c);
			ConnectionState GetConnectionState() const
			{
				return connectionState;
			}

			bool videoKeyframeRequired = false;

			bool setOrigin(uint64_t ctr,avs::uid uid);
			bool hasOrigin() const;
			avs::uid getOrigin() const;

			void setGlobalIlluminationTextures(size_t num, const avs::uid *uids);
			const std::vector<avs::uid> &getGlobalIlluminationTextures() const
			{
				return global_illumination_texture_uids;
			}
			void resendUnconfirmedOrthogonalStates();
		protected:
			avs::uid clientID=0;
			teleport::core::SetupCommand lastSetupCommand;
			std::map<avs::uid, std::shared_ptr<OrthogonalNodeStateMap>> orthogonalNodeStates;
			uint64_t nextConfirmationNumber = 1;
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
