#pragma once

#include <memory>
#include <vector>

#include <libavstream/platforms/this_platform.h>

#include <libavstream/libavstream.hpp>
#include <libavstream/genericdecoder.h>

#define WITH_TELEPORT_STATS 1

namespace teleport
{
	namespace server
	{
		struct ServerNetworkSettings;
		struct ServerSettings;

		//! Network pipeline.
		class NetworkPipeline
		{
		public:
			NetworkPipeline();
			virtual ~NetworkPipeline();

			void initialise(const ServerNetworkSettings& inNetworkSettings);

			virtual void release();
			virtual bool process();
			bool isInitialized() const
			{
				return initialized;
			}
			virtual avs::Pipeline* getAvsPipeline() const;

			avs::Result getCounters(avs::NetworkSinkCounters& counters) const;

			void setProcessingEnabled(bool enable);
			bool isProcessingEnabled() const;

			bool getNextStreamingControlMessage(std::string &str);
			void receiveStreamingControlMessage(const std::string& str);

			avs::Queue ColorQueue;
			avs::Queue TagDataQueue;
			avs::Queue GeometryQueue;
			avs::Queue AudioQueue;
			avs::Queue reliableSendQueue;
			avs::Queue unreliableReceiveQueue;
			avs::Queue unreliableSendQueue;
			avs::Queue reliableReceiveQueue;
			std::unique_ptr<avs::NetworkSink> mNetworkSink;
		private:
			bool initialized = false;

			std::unique_ptr<avs::Pipeline> mPipeline;
			avs::Result mPrevProcResult = avs::Result::OK;

#if WITH_TELEPORT_STATS
			avs::Timestamp mLastTimestamp;
#endif // WITH_TELEPORT_STATS
		};
	}
}
