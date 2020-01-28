#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>

#include "CasterSettings.h"

using namespace SCServer;

namespace
{
	constexpr double networkPipelineStatInterval = 60000000.0; // 1s
	constexpr int networkPipelineSocketBufferSize = 16 * 1024 * 1024; // 16MiB
}

NetworkPipeline::NetworkPipeline(const SCServer::CasterSettings* settings)
	:settings(settings)
{}

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
	for(std::unique_ptr<VideoPipe>& Pipe : videoPipes)
	{
		Pipe = std::make_unique<VideoPipe>();
	}
	videoPipes[0]->sourceQueue = colorQueue;
	if(depthQueue) videoPipes[1]->sourceQueue = depthQueue;

	geometryPipes.resize(1);
	for(std::unique_ptr<GeometryPipe>& Pipe : geometryPipes)
	{
		Pipe = std::make_unique<GeometryPipe>();
	}
	geometryPipes[0]->sourceQueue = geometryQueue;

	char remoteIP[20];
	size_t stringLength = wcslen(inNetworkSettings.remoteIP);
	//Convert wide character string to multibyte string.
	wcstombs_s(&stringLength, remoteIP, inNetworkSettings.remoteIP, 20);

	size_t NumInputs = videoPipes.size() + geometryPipes.size();
	if(!networkSink->configure(NumInputs, nullptr, inNetworkSettings.localPort, remoteIP, inNetworkSettings.remotePort))
	{
		std::cout << "Failed to configure network sink!\n";
		return;
	}

	// Each video stream has an input Queue, a forwarder, and a packetizer.
	// The Geometry queue consists of an input Queue, a forwarder, and a Geometry packetizer.
	for(int32_t i = 0; i < videoPipes.size(); ++i)
	{
		auto& Pipe = videoPipes[i];
		Pipe->forwarder.configure(1, 1, 64 * 1024);
		Pipe->packetizer.configure(avs::StreamParserInterface::Create(avs::StreamParserType::AVC_AnnexB), 1, 50 + i);
		if(!pipeline->link({Pipe->sourceQueue, &Pipe->forwarder, &Pipe->packetizer}) || !avs::Node::link(Pipe->packetizer, *networkSink))
		{
			std::cout << "Failed to configure network video pipeline!\n";
			return;
		}
	}
	for(int32_t i = 0; i < geometryPipes.size(); ++i)
	{
		auto& Pipe = geometryPipes[i];
		Pipe->forwarder.configure(1, 1, 64 * 1024);
		Pipe->packetizer.configure(avs::StreamParserInterface::Create(avs::StreamParserType::Geometry), 1, 100 + i);
		if(!pipeline->link({Pipe->sourceQueue, &Pipe->forwarder, &Pipe->packetizer}) || !avs::Node::link(Pipe->packetizer, *networkSink))
		{
			std::cout << "Failed to configure network video pipeline!\n";
			return;
		}
	}
	pipeline->add(networkSink.get());

#if WITH_REMOTEPLAY_STATS
	lastTimestamp = avs::PlatformWindows::getTimestamp();
#endif // WITH_REMOTEPLAY_STATS
}

void NetworkPipeline::release()
{
	pipeline.reset();
	if(networkSink) networkSink->deconfigure();
	networkSink.reset();
	videoPipes.clear();
	geometryPipes.clear();
}

void NetworkPipeline::process()
{
	assert(pipeline);
	assert(networkSink);

	const avs::Result result = pipeline->process();
	if(!result && result != avs::Result::IO_Empty)
	{
		std::cout << "Network pipeline processing encountered an error!\n";
	}

#if WITH_REMOTEPLAY_STATS
	avs::Timestamp timestamp = avs::PlatformWindows::getTimestamp();
	if(avs::PlatformWindows::getTimeElapsed(lastTimestamp, timestamp) >= networkPipelineStatInterval)
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