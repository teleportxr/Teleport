#pragma once

#include <memory>
#include <vector>

#include <libavstream/libavstream.hpp>


namespace teleport
{
	namespace audio
	{
		class AudioStreamTarget;
	}
	namespace server
	{
		struct ServerSettings;

		class SourceNetworkPipeline
		{
		public:
			SourceNetworkPipeline();
			virtual ~SourceNetworkPipeline();

			void initialize(const avs::NetworkSourceParams& sourceParams, avs::Queue* audioQueue, avs::AudioDecoder* audioDecoder, avs::AudioTarget* audioTarget);

			virtual void release();
			virtual bool process();

			avs::Pipeline* getAvsPipeline() const;

			avs::Result getCounterValues(avs::NetworkSourceCounters& counters) const;

		private:
			struct AudioPipe
			{
				avs::Queue* queue;
				avs::AudioDecoder* decoder;
				avs::AudioTarget* target;
			};

			std::unique_ptr<avs::Pipeline> pipeline;
			std::vector<std::unique_ptr<AudioPipe>> audioPipes;
			std::unique_ptr<avs::NetworkSource> networkSource;
			avs::Result prevProcResult= avs::Result::OK;
		};
	}
}