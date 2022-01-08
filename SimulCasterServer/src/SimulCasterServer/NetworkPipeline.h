#pragma once

#include <memory>
#include <vector>

#include <libavstream/platforms/platform_windows.hpp>
#include <libavstream/libavstream.hpp>

#define WITH_REMOTEPLAY_STATS 1

namespace teleport
{
	struct CasterNetworkSettings;
	struct ServerSettings;

	class NetworkPipeline
	{
	public:
		NetworkPipeline(const ServerSettings* settings);
		virtual ~NetworkPipeline();

		void initialise(const CasterNetworkSettings& inNetworkSettings, avs::Queue* videoQueue, avs::Queue* tagDataQueue, avs::Queue* geometryQueue, avs::Queue* audioQueue);

		virtual void release();
		virtual bool process();

		virtual avs::Pipeline* getAvsPipeline() const;

		avs::Result getCounters(avs::NetworkSinkCounters& counters) const;

		void setProcessingEnabled(bool enable);

		bool isProcessingEnabled() const;

	private:
		const ServerSettings* mSettings;

		std::unique_ptr<avs::Pipeline> mPipeline;
		std::unique_ptr<avs::NetworkSink> mNetworkSink;
		avs::Result mPrevProcResult;

#if WITH_REMOTEPLAY_STATS
		avs::Timestamp mLastTimestamp;
#endif // WITH_REMOTEPLAY_STATS
	};
}