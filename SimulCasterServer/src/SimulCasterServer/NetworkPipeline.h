#pragma once

#include <memory>
#include <vector>

#include "libavstream/libavstream.hpp"

namespace SCServer
{
	class NetworkPipeline
	{
	public:
		virtual ~NetworkPipeline() = default;

		virtual void release();
		virtual void process();

		virtual avs::Pipeline* getAvsPipeline() const;
		virtual float getBandWidthKPS() const;
	protected:
		void initialise(avs::Queue* colorQueue, avs::Queue* depthQueue, avs::Queue* geometryQueue, avs::NetworkSinkParams sinkParams, uint16_t localPort, const char* remoteIP, uint16_t remotePort);

		std::unique_ptr<avs::NetworkSink>& getNetworkSink();
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

		std::unique_ptr<avs::Pipeline> pipeline;
		std::vector<std::unique_ptr<VideoPipe>> videoPipes;
		std::vector<std::unique_ptr<GeometryPipe>> geometryPipes;
		std::unique_ptr<avs::NetworkSink> networkSink;
	};
}