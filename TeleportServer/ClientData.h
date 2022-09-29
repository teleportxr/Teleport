#pragma once

#include "enet/enet.h"
#include "libavstream/common.hpp"

#include "TeleportServer/ClientManager.h"
#include "TeleportServer/ClientMessaging.h"
#include "TeleportServer/GeometryStreamingService.h"
#include "TeleportServer/ServerSettings.h"

class PluginDiscoveryService;
class PluginVideoEncodePipeline;
class PluginAudioEncodePipeline;
namespace teleport
{
	namespace server
	{
		typedef int64_t(__stdcall* GetUnixTimestampFn)();
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
		
		class GeometryStreamingService;
		class ClientData
		{
		public:
			ClientData( std::shared_ptr<PluginVideoEncodePipeline> videoPipeline, std::shared_ptr<PluginAudioEncodePipeline> audioPipeline, std::shared_ptr<teleport::ClientMessaging> clientMessaging);
			void StartStreaming(const teleport::ServerSettings &casterSettings, const teleport::CasterEncoderSettings &encoderSettings
				, uint32_t connectionTimeout
				, avs::uid serverID
				, GetUnixTimestampFn getUnixTimestamp
				, bool use_ssl);
			void setNodePosePath(avs::uid nodeID, const std::string &regexPosePath);
			void setInputDefinitions(const std::vector<avs::InputDefinition> &inputDefs);
			// client settings from engine-side:
			teleport::ClientSettings clientSettings;
			avs::ClientDynamicLighting clientDynamicLighting;
			std::vector<avs::InputDefinition> inputDefinitions;
			teleport::CasterContext casterContext;

			std::shared_ptr<PluginVideoEncodePipeline> videoEncodePipeline;
			std::shared_ptr<PluginAudioEncodePipeline> audioEncodePipeline;
			std::shared_ptr<teleport::ClientMessaging> clientMessaging;

			bool isStreaming = false;
			bool validClientSettings = false;
			bool videoKeyframeRequired = false;

			bool setOrigin(uint64_t ctr,avs::uid uid,avs::vec3 pos,avs::vec4 orientation);
			bool isConnected() const;
			bool hasOrigin() const;
			avs::vec3 getOrigin() const;

			void setGlobalIlluminationTextures(size_t num, const avs::uid *uids);
			const std::vector<avs::uid> &getGlobalIlluminationTextures() const
			{
				return global_illumination_texture_uids;
			}
		protected:
			mutable bool _hasOrigin=false;
			avs::vec3 originClientHas;
			std::vector<avs::uid> global_illumination_texture_uids;
			struct NodeSubtypeState
			{
				std::string regexPath;
			};
			ReflectedStateMap<NodeSubtypeState> nodeSubTypes;
			ENetAddress address = {};
		};
		struct ClientStatus
		{
			unsigned ipAddress;
		};
	}
}
