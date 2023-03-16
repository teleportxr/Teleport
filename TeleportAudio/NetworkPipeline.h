#pragma once

#include <memory>
#include <vector>

#include <libavstream/libavstream.hpp>

namespace teleport
{
	namespace audio
	{
		struct NetworkSettings
		{
			int32_t localPort;
			const char* remoteIP;
			int32_t remotePort;
			int32_t clientBandwidthLimit;
			int32_t clientBufferSize;
			int32_t requiredLatencyMs;
			int32_t connectionTimeout;
		};

		class NetworkPipeline
		{
		public:
			NetworkPipeline();
			virtual ~NetworkPipeline();

			void initialise(const NetworkSettings& inNetworkSettings, avs::Queue* audioQueue);

			virtual void release();
			virtual bool process();

			avs::Pipeline* getAvsPipeline() const;

			avs::Result getCounters(avs::NetworkSinkCounters& counters) const;

		private:
			struct AudioPipe
			{
				avs::Queue* sourceQueue;
			};

			std::unique_ptr<avs::Pipeline> pipeline;
			std::vector<std::unique_ptr<AudioPipe>> audioPipes;
			std::unique_ptr<avs::NetworkSink> networkSink;
			avs::Result prevProcResult;
		};
	}
}