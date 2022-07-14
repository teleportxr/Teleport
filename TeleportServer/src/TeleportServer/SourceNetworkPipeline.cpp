#include "SourceNetworkPipeline.h"

#include <algorithm>
#include <iostream>

#include "TeleportCore/ErrorHandling.h"
#include "ServerSettings.h"

#include "AudioStreamTarget.h"

namespace teleport
{
	SourceNetworkPipeline::SourceNetworkPipeline(const ServerSettings* inSettings)
		: settings(inSettings), prevProcResult(avs::Result::OK)
	{
	}

	SourceNetworkPipeline::~SourceNetworkPipeline()
	{
		release();
	}

	void SourceNetworkPipeline::initialize(const avs::NetworkSourceParams& sourceParams, avs::Queue* audioQueue, avs::AudioDecoder* audioDecoder, avs::AudioTarget* audioTarget)
	{
		pipeline.reset(new avs::Pipeline);
		networkSource.reset(new avs::NetworkSource);

		if(audioQueue)
		{
			audioPipes.resize(1);
			for (std::unique_ptr<AudioPipe>& pipe : audioPipes)
			{
				pipe = std::make_unique<AudioPipe>();
			}
			audioPipes[0]->queue = audioQueue;
			audioPipes[0]->decoder = audioDecoder;
			audioPipes[0]->target = audioTarget;
		}

		std::vector<avs::NetworkSourceStream> streams;

		for (int32_t i = 0; i < audioPipes.size(); ++i)
		{
			avs::NetworkSourceStream stream;
			stream.id = 100 + i;
			streams.emplace_back(std::move(stream));
		}

		// Configure for num video streams + 1 audio stream + 1 geometry stream
		if (!networkSource->configure(std::move(streams), sourceParams))
		{
			TELEPORT_CERR << "Failed to configure network source!" << std::endl;
			return;
		}

		pipeline->add(networkSource.get());

		for (int32_t i = 0; i < audioPipes.size(); ++i)
		{
			auto& pipe = audioPipes[i];
			if (!avs::PipelineNode::link(*networkSource, *pipe->queue))
			{
				TELEPORT_CERR << "Failed to link network source and audio queue!" << std::endl;
				return;
			}

			if (!avs::PipelineNode::link(*pipe->queue, *pipe->decoder))
			{
				TELEPORT_CERR << "Failed to link audio queue and audio decoder!" << std::endl;
				return;
			}

			if (pipeline->link({ pipe->decoder, pipe->target }))
			{
				TELEPORT_CERR << "Failed to link audio queue and audio decoder!" << std::endl;
				return;
			}
		}
		prevProcResult = avs::Result::OK;
	}

	void SourceNetworkPipeline::release()
	{
		pipeline.reset();
		if (networkSource) networkSource->deconfigure();
		networkSource.reset();
		audioPipes.clear();
	}

	bool SourceNetworkPipeline::process()
	{
		assert(pipeline);
		assert(networkSource);

		const avs::Result result = pipeline->process();
		// Prevent spamming of errors from NetworkSource. This can happen when there is a connection issue.
		if (!result && result != avs::Result::IO_Empty)
		{
			if (result != prevProcResult)
			{
				TELEPORT_CERR << "Network pipeline processing encountered an error!" << std::endl;
				prevProcResult = result;
			}
			return false;
		}
		prevProcResult = result;

		return true;
	}

	avs::Pipeline* SourceNetworkPipeline::getAvsPipeline() const
	{
		return pipeline.get();
	}

	avs::Result SourceNetworkPipeline::getCounterValues(avs::NetworkSourceCounters& counters) const
	{
		if (networkSource)
		{
			counters = networkSource->getCounterValues();
		}
		else
		{
			TELEPORT_CERR << "Can't return counters because network source is null." << std::endl;
			return avs::Result::Node_Null;
		}
		return avs::Result::OK;
	}
}