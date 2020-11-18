#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>

#include "ErrorHandling.h"
#include "CasterSettings.h"

namespace
{
	constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

namespace SCServer
{
	NetworkPipeline::NetworkPipeline(const CasterSettings* settings)
		:settings(settings)
	{
	}

	NetworkPipeline::~NetworkPipeline()
	{
		release();
	}

	void NetworkPipeline::initialise(const CasterNetworkSettings& inNetworkSettings, avs::Queue* colorQueue, avs::Queue* depthQueue, avs::Queue* geometryQueue, avs::Queue* audioQueue)
	{
		assert(colorQueue);

		avs::NetworkSinkParams SinkParams = {};
		SinkParams.socketBufferSize = networkPipelineSocketBufferSize;
		SinkParams.throttleToRateKpS = std::min(settings->throttleKpS, static_cast<int64_t>(inNetworkSettings.clientBandwidthLimit));// Assuming 60Hz on the other size. k per sec
		SinkParams.socketBufferSize = inNetworkSettings.clientBufferSize;
		SinkParams.requiredLatencyMs = inNetworkSettings.requiredLatencyMs;
		SinkParams.connectionTimeout = inNetworkSettings.connectionTimeout;

		pipeline.reset(new avs::Pipeline);
		networkSink.reset(new avs::NetworkSink);

		videoPipes.resize(depthQueue ? 2 : 1);
		for (std::unique_ptr<VideoPipe>& pipe : videoPipes)
		{
			pipe = std::make_unique<VideoPipe>();
		}
		videoPipes[0]->sourceQueue = colorQueue;
		if (depthQueue)
			videoPipes[1]->sourceQueue = depthQueue;

		if(audioQueue)
		{
			audioPipes.resize(1);
			for (std::unique_ptr<AudioPipe>& pipe : audioPipes)
			{
				pipe = std::make_unique<AudioPipe>();
			}
			audioPipes[0]->sourceQueue = audioQueue;
		}
		geometryPipes.resize(1);
		for (std::unique_ptr<GeometryPipe>& pipe : geometryPipes)
		{
			pipe = std::make_unique<GeometryPipe>();
		}
		geometryPipes[0]->sourceQueue = geometryQueue;

		char remoteIP[20];
		size_t stringLength = wcslen(inNetworkSettings.remoteIP);
		//Convert wide character string to multibyte string.
		wcstombs_s(&stringLength, remoteIP, inNetworkSettings.remoteIP, 20);

		std::vector<avs::NetworkSinkStream> streams;

		for (int32_t i = 0; i < videoPipes.size(); ++i)
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::AVC_AnnexB;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.id = 20 + i;
			stream.dataType = avs::NetworkDataType::HEVC; 
			streams.emplace_back(std::move(stream));
		}

		for (int32_t i = 0; i < audioPipes.size(); ++i)
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::Audio;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 2048;
			stream.id = 40 + i;
			stream.dataType = avs::NetworkDataType::Audio;
			streams.emplace_back(std::move(stream));
		}

		for (int32_t i = 0; i < geometryPipes.size(); ++i)
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::Geometry;
			stream.useParser = true;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.id = 60 + i;
			stream.dataType = avs::NetworkDataType::Geometry;
			streams.emplace_back(std::move(stream));
		}

		if (!networkSink->configure(std::move(streams), nullptr, inNetworkSettings.localPort, remoteIP, inNetworkSettings.remotePort, SinkParams))
		{
			TELEPORT_CERR << "Failed to configure network sink!" << std::endl;
			return;
		}

		for (int32_t i = 0; i < videoPipes.size(); ++i)
		{
			auto &pipe = videoPipes[i];
			if (!avs::Node::link(*pipe->sourceQueue, *networkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for video!" << std::endl;
				return;
			}
			pipeline->add(pipe->sourceQueue);
		}

		for (int32_t i = 0; i < audioPipes.size(); ++i)
		{
			auto& pipe = audioPipes[i];
			if (!avs::Node::link(*pipe->sourceQueue, *networkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for audio!" << std::endl;
				return;
			}
			pipeline->add(pipe->sourceQueue);
		}

		for (int32_t i = 0; i < geometryPipes.size(); ++i)
		{
			auto &pipe = geometryPipes[i];
			if (!avs::Node::link(*pipe->sourceQueue, *networkSink))
			{
				TELEPORT_CERR << "Failed to configure network pipeline for geometry!" << std::endl;
				return;
			}
			pipeline->add(pipe->sourceQueue);
		}

		pipeline->add(networkSink.get());

#if WITH_REMOTEPLAY_STATS
		lastTimestamp = avs::PlatformWindows::getTimestamp();
#endif // WITH_REMOTEPLAY_STATS
	}

	void NetworkPipeline::release()
	{
		pipeline.reset();
		if (networkSink) networkSink->deconfigure();
		networkSink.reset();
		videoPipes.clear();
		geometryPipes.clear();
		audioPipes.clear();
	}

	bool NetworkPipeline::process()
	{
		assert(pipeline);
		assert(networkSink);

		const avs::Result result = pipeline->process();
		if (!result && result != avs::Result::IO_Empty)
		{
			TELEPORT_CERR << "Network pipeline processing encountered an error!" << std::endl;
			return false;
		}

#if 0
		avs::Timestamp timestamp = avs::PlatformWindows::getTimestamp();
		if (avs::PlatformWindows::getTimeElapsed(lastTimestamp, timestamp) >= networkPipelineStatInterval)
		{
			const avs::NetworkSinkCounters counters = networkSink->getCounterValues();
			TELEPORT_COUT << "DP: " << counters.decoderPacketsQueued << " | NP: " << counters.networkPacketsSent << " | BYTES: " << counters.bytesSent << std::endl;
			lastTimestamp = timestamp;
		}
		networkSink->setDebugStream(settings->debugStream);
		networkSink->setDebugNetworkPackets(settings->enableDebugNetworkPackets);
		networkSink->setDoChecksums(settings->enableChecksums);
		networkSink->setEstimatedDecodingFrequency(settings->estimatedDecodingFrequency);
#endif // WITH_REMOTEPLAY_STATS
		return true;
	}

	avs::Pipeline* NetworkPipeline::getAvsPipeline() const
	{
		return pipeline.get();
	}

	float NetworkPipeline::getBandWidthKPS() const
	{
		return networkSink ? networkSink->getBandwidthKPerS() : 0.0f;
	}
}