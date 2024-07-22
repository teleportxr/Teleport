// libavstream
// (c) Copyright 2018-2024 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <libavstream/stream/parser_interface.hpp>

namespace avs
{
	/*! Network sink parameters. */
	struct NetworkSinkParams
	{
		/*!
		 * Operating system socket send buffer size.
		 */
		int socketBufferSize = 1024 * 1024;
		uint64_t throttleToRateKpS;
		uint32_t requiredLatencyMs;
		uint32_t connectionTimeout = 5000;
		uint32_t bandwidthInterval = 10000;
	};

	/*! Network sink counters. */
	struct NetworkSinkCounters
	{
		/*! Total bytes sent. */
		uint64_t bytesSent = 0;
		/*! Number of sent network packets. */
		uint64_t networkPacketsSent = 0;
		/*! Available bandwidth  */
		double bandwidth = 0;
		/*! Average bandwidth used */
		double avgBandwidthUsed = 0;
		/*! Minimum bandwidth used */
		double minBandwidthUsed = 0;
		/*! Maximum bandwidth used */
		double maxBandwidthUsed = 0;
	};

	/*! Network sink stream data. */
	struct NetworkSinkStream
	{
		/*! Stream index */
		uint64_t counter = 0;
		/*! Max size of the buffer */
		size_t chunkSize = 0;
		/*! id */
		uint32_t id = 0;
		/*! Stream parser type */
		StreamParserType parserType;
		/*! Data type */
		NetworkDataType dataType = NetworkDataType::HEVC;
		/* Whether to use a parser */
		bool useParser = false;
		/*! Whether there is a data limit per frame on this stream */
		bool isDataLimitPerFrame = false;
		//! The unique text label, used to identify streams across the network.
		std::string label;
		//! The name of the queue that the stream's inputs will come from.
		std::string inputName;
		//! The name of the queue that the stream's outputs will go to, if this stream has any.
		std::string outputName;
		//! Whether the stream should be assembled into frames with EFP.
		bool framed = false;
		bool canReceive = false;
		bool reliable=true;
		/*! Buffer of data to be sent */
		std::vector<uint8_t> buffer;
	};
	//! A message sent by the reliable (e.g. Websockets) channel.
	struct SetupMessage
	{
		std::string text;
	};
	class AVSTREAM_API NetworkSink : public PipelineNode
	{
	public:
		NetworkSink(PipelineNode::Private* d_ptr):PipelineNode(d_ptr) {}
		virtual ~NetworkSink() {}
		virtual StreamingConnectionState getConnectionState() const {
			return StreamingConnectionState::UNINITIALIZED;
		}
		virtual Result configure(std::vector<NetworkSinkStream>&& streams,  const NetworkSinkParams& params = {}) = 0;
		virtual Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex) = 0;
		virtual NetworkSinkCounters getCounters() const=0;
		virtual void setProcessingEnabled(bool enable)=0;
		virtual bool isProcessingEnabled() const=0;
		virtual bool getNextStreamingControlMessage(std::string&)
		{
			return false;
		}
		virtual void receiveStreamingControlMessage(const std::string&) {}
	};
 	class AVSTREAM_API NullNetworkSink final: public NetworkSink
	{
	public:
		NullNetworkSink() :NetworkSink(nullptr) {}
		virtual ~NullNetworkSink() {}
		Result configure(std::vector<NetworkSinkStream>&& streams,  const NetworkSinkParams& params = {}) override { return avs::Result::OK; }
		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex) override  { return avs::Result::OK; }
		NetworkSinkCounters getCounters() const override {
			return NetworkSinkCounters();
		}
		void setProcessingEnabled (bool enable) override
		{
		}
		bool isProcessingEnabled() const override
		{
			return false;
		}
		const char* getDisplayName() const override { return "NullNetworkSink"; }
	};
}