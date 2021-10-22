// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <map>
#include <unordered_map>

#include <asio.hpp>

#include <common_p.hpp>
#include <node_p.hpp>
#include <network/packetformat.hpp>
#include <util/bytebuffer.hpp>
#include <libavstream/networksink.hpp>
#if LIBAV_USE_SRT
#include <srt.h>
#endif
#include <ElasticFrameProtocol.h>

namespace avs
{
	using asio::ip::udp;
	struct NetworkSink::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(NetworkSink, PipelineNode)
#if LIBAV_USE_SRT
		SRTSOCKET m_socket=0;
		SRTSOCKET m_remote_socket=0;
		sockaddr_in remote_addr;
		int pollid=0;
		bool bConnected=false;
#else
		std::unique_ptr<asio::io_service> m_service;
		std::unique_ptr<udp::socket> m_socket;
		std::unique_ptr<udp::endpoint> m_endpoint;
#endif
		NetworkSinkCounters m_counters;
		uint32_t m_minBandwidthUsed;

		struct 
		{
			std::string address;
			std::string port;
		} m_remote;

		uint64_t lastTimestamp;
		uint32_t debugStream;
		bool doChecksums;
		bool mDebugNetworkPackets=false;
		std::atomic_bool m_processingEnabled = true;
		uint64_t throttleRate; //Unused
		uint64_t socketBufferSize; //Amount of bytes the client has in their socket's buffer to receive data.
		uint8_t estimatedDecodingFrequency; //Estimated times per second the client will decode the packets sent to them.
		std::unique_ptr<ElasticFrameProtocolSender> m_EFPSender;
		std::vector<NetworkSinkStream> m_streams;
		std::unordered_map<int, uint32_t> m_streamIndices;
		NetworkSinkParams m_params;
		std::queue<std::vector<uint8_t>> m_dataQueue;
		size_t m_maxPacketsAllowedPerSecond;
		size_t m_maxPacketsAllowed;
		/** Packets sent this frame */
		uint32_t m_packetsSent;
		std::unordered_map<uint32_t, std::unique_ptr<StreamParserInterface>> m_parsers;
		std::mutex m_countersMutex;
		uint32_t m_statsTimeElapsed;

		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex);
		void sendOrCacheData(const std::vector<uint8_t>& subPacket);
		void sendData(const std::vector<uint8_t>& subPacket);
		void closeConnection();
		void updateCounters(uint64_t timestamp, uint32_t deltaTime);
	};

} // avs