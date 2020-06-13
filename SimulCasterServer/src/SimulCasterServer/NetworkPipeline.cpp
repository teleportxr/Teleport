#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>

#include "CasterSettings.h"

namespace
{
	constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

namespace SCServer
{
	NetworkPipeline::NetworkPipeline(const SCServer::CasterSettings* settings)
		:settings(settings)
	{}

	NetworkPipeline::~NetworkPipeline()
	{
		release();
	}

	void NetworkPipeline::initialise(const CasterNetworkSettings& inNetworkSettings, avs::Queue* colorQueue, avs::Queue* depthQueue, avs::Queue* geometryQueue)
	{
		assert(colorQueue);

		avs::NetworkSinkParams SinkParams = {};
		SinkParams.socketBufferSize = networkPipelineSocketBufferSize;
		SinkParams.throttleToRateKpS = std::min(settings->throttleKpS, static_cast<int64_t>(inNetworkSettings.clientBandwidthLimit));// Assuming 60Hz on the other size. k per sec
		SinkParams.socketBufferSize = inNetworkSettings.clientBufferSize;
		SinkParams.requiredLatencyMs = inNetworkSettings.requiredLatencyMs;

		pipeline.reset(new avs::Pipeline);
		networkSink.reset(new avs::NetworkSink);

		videoPipes.resize(depthQueue ? 2 : 1);
		for (std::unique_ptr<VideoPipe>& pipe : videoPipes)
		{
			pipe = std::make_unique<VideoPipe>();
		}
		videoPipes[0]->sourceQueue = colorQueue;
		if (depthQueue) videoPipes[1]->sourceQueue = depthQueue;

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

		size_t NumInputs = videoPipes.size() + geometryPipes.size();

		std::vector<avs::NetworkSinkStream> streams;

		for (int32_t i = 0; i < videoPipes.size(); ++i)
		{
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::AVC_AnnexB;
			stream.useParser = false;
			stream.isDataLimitPerFrame = false;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.streamIndex = 50 + i;
			stream.dataType = avs::NetworkDataType::HEVC; // this shouldn't be hardcoded when H264 support added
			streams.emplace_back(std::move(stream));
		}

		for (int32_t i = 0; i < geometryPipes.size(); ++i)
		{
			static const uint8_t expectedFPS = 60;
			avs::NetworkSinkStream stream;
			stream.parserType = avs::StreamParserType::Geometry;
			stream.useParser = true;
			stream.isDataLimitPerFrame = true;
			stream.counter = 0;
			stream.chunkSize = 64 * 1024;
			stream.streamIndex = 100 + i;
			stream.dataType = avs::NetworkDataType::Geometry;
			streams.emplace_back(std::move(stream));
		}

		if (!networkSink->configure(std::move(streams), nullptr, inNetworkSettings.localPort, remoteIP, inNetworkSettings.remotePort, SinkParams))
		{
			std::cout << "Failed to configure network sink! \n";
			return;
		}

		for (int32_t i = 0; i < videoPipes.size(); ++i)
		{
			auto &pipe = videoPipes[i];
			if (!avs::Node::link(*pipe->sourceQueue, *networkSink))
			{
				std::cout << "Failed to configure network pipeline for video! \n";
				return;
			}
			pipeline->add(pipe->sourceQueue);
		}

		for (int32_t i = 0; i < geometryPipes.size(); ++i)
		{
			auto &pipe = geometryPipes[i];
			if (!avs::Node::link(*pipe->sourceQueue, *networkSink))
			{
				std::cout << "Failed to configure network pipeline for geometry! \n";
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
	}

	void NetworkPipeline::process()
	{
		assert(pipeline);
		assert(networkSink);

		const avs::Result result = pipeline->process();
		if (!result && result != avs::Result::IO_Empty)
		{
			std::cout << "Network pipeline processing encountered an error!\n";
		}

#if WITH_REMOTEPLAY_STATS
		avs::Timestamp timestamp = avs::PlatformWindows::getTimestamp();
		if (avs::PlatformWindows::getTimeElapsed(lastTimestamp, timestamp) >= networkPipelineStatInterval)
		{
			const avs::NetworkSinkCounters counters = networkSink->getCounterValues();
			std::cout << "DP: " << counters.decoderPacketsQueued << " | NP: " << counters.networkPacketsSent << " | BYTES: " << counters.bytesSent << std::endl;
			lastTimestamp = timestamp;
		}
		networkSink->setDebugStream(settings->debugStream);
		networkSink->setDebugNetworkPackets(settings->enableDebugNetworkPackets);
		networkSink->setDoChecksums(settings->enableChecksums);
		networkSink->setEstimatedDecodingFrequency(settings->estimatedDecodingFrequency);
#endif // WITH_REMOTEPLAY_STATS
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