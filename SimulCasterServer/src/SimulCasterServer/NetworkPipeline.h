#pragma once

#include <memory>
#include <vector>

#include "libavstream/libavstream.hpp"
#include "libavstream/platforms/platform_windows.hpp"

#define WITH_REMOTEPLAY_STATS 1

namespace SCServer
{
	struct CasterNetworkSettings;
	struct CasterSettings;

	class NetworkPipeline
	{
	public:
		NetworkPipeline(const CasterSettings& settings);
		virtual ~NetworkPipeline() = default;

		void initialise(const CasterNetworkSettings& inNetworkSettings, avs::Queue* colorQueue, avs::Queue* depthQueue, avs::Queue* geometryQueue);

		virtual void release();
		virtual void process();

		virtual avs::Pipeline* getAvsPipeline() const;
		virtual float getBandWidthKPS() const;
	private:
		struct VideoPipe
		{
			avs::Queue* sourceQueue;
			avs::Forwarder forwarder;
			avs::Packetizer packetizer;
		};
		struct GeometryPipe
		{
			avs::Queue* sourceQueue;
			avs::Forwarder forwarder;
			avs::Packetizer packetizer;
		};

		const CasterSettings& settings;

		std::unique_ptr<avs::Pipeline> pipeline;
		std::vector<std::unique_ptr<VideoPipe>> videoPipes;
		std::vector<std::unique_ptr<GeometryPipe>> geometryPipes;
		std::unique_ptr<avs::NetworkSink> networkSink;

#if WITH_REMOTEPLAY_STATS
		avs::Timestamp lastTimestamp;
#endif // WITH_REMOTEPLAY_STATS
	};
}