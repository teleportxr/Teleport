// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include <libavstream/common.hpp>
#include <libavstream/node.hpp>
#include <optional>

namespace avs
{
	struct NetworkPacket;
	class NetworkPacketMap;

	/*! Network source stream data. */
	struct NetworkSourceStream
	{
		/*! id */
		uint32_t id = UINT32_MAX;
	};

	/*! Network source parameters. */
	struct NetworkSourceParams
	{
		const char* remoteIP = "";
		int32_t remotePort = 0;
		int32_t remoteHTTPPort = 0;
		uint32_t connectionTimeout = 5000;
		uint32_t maxHTTPConnections = 10;
		uint32_t httpStreamID = UINT32_MAX;
		bool asyncProcessPackets = false;
		bool useSSL = false;
	};

	/*! Network source counters. */
	struct NetworkSourceCounters
	{
		/*! Total bytes received. */
		uint64_t bytesReceived = 0;
		/*! Number of received network packets. */
		uint64_t networkPacketsReceived = 0;
		/*! Number of successfully assembled decoder packets. */
		uint64_t decoderPacketsReceived = 0;
		/*! Number of HTTP files received. */
		uint64_t httpFilesReceived = 0;
		/*! Number of network packets dropped due to GC timeout. */
		uint64_t networkPacketsDropped = 0;
		/*! Number of decoder packets dropped due to GC timeout. */
		uint64_t decoderPacketsDropped = 0;
		/*! Number of incomplete decoder packets received. */
		uint64_t incompleteDecoderPacketsReceived = 0;
		/*! Fraction of decoder packets dropped. */
		float decoderDropped = 0.0f;
		/*! Fraction of network packets dropped. */
		float networkDropped = 0.0f;
		/*! Bandwidth in kilobytes. */
		float bandwidthKPS = 0.0f;
		/*! Number of successfully assembled decoder packets received per second. */
		double decoderPacketsReceivedPerSec = 0.0;
	};

	/*!
	 * Network source node `[passive, 0/1]`
	 *
	 * Receives video stream from a remote UDP endpoint.
	 */
	class AVSTREAM_API NetworkSource : public PipelineNode
	{
	public:
		NetworkSource(PipelineNode::Private* d_ptr);
		virtual Result configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params)=0;
		virtual NetworkSourceCounters getCounterValues() const=0;
	};

} // avs