#include "NetworkPipeline.h"

#include <algorithm>
#include <iostream>

using namespace SCServer;

void SCServer::NetworkPipeline::release()
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
}

avs::Pipeline* NetworkPipeline::getAvsPipeline() const
{
	return pipeline.get();
}

float NetworkPipeline::getBandWidthKPS() const
{
	return networkSink ? networkSink->getBandwidthKPerS() : 0.0f;
}

void NetworkPipeline::initialise(avs::Queue* ColorQueue, avs::Queue* DepthQueue, avs::Queue* GeometryQueue, avs::NetworkSinkParams sinkParams, uint16_t localPort, const char* remoteIP, uint16_t remotePort)
{
	pipeline.reset(new avs::Pipeline);
	networkSink.reset(new avs::NetworkSink);

	videoPipes.resize(DepthQueue ? 2 : 1);
	for(std::unique_ptr<VideoPipe>& Pipe : videoPipes)
	{
		Pipe = std::make_unique<VideoPipe>();
	}
	videoPipes[0]->sourceQueue = ColorQueue;
	if(DepthQueue) videoPipes[1]->sourceQueue = DepthQueue;

	geometryPipes.resize(1);
	for(std::unique_ptr<GeometryPipe>& Pipe : geometryPipes)
	{
		Pipe = std::make_unique<GeometryPipe>();
	}
	geometryPipes[0]->sourceQueue = GeometryQueue;

	size_t NumInputs = videoPipes.size() + geometryPipes.size();
	if(!networkSink->configure(NumInputs, nullptr, localPort, remoteIP, remotePort))
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
}

std::unique_ptr<avs::NetworkSink>& SCServer::NetworkPipeline::getNetworkSink()
{
	return networkSink;
}
