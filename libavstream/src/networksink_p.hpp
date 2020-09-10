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
	struct NetworkSink::Private final : public Node::Private
	{
		AVSTREAM_PRIVATEINTERFACE(NetworkSink, Node)
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

#if !defined(LIBAV_USE_EFP)
		ByteBuffer m_buffer;

		std::map<uint8_t, uint16_t> m_streamSequences; //<ID of stream; sequence id of next packet sent on that stream>
		std::map<uint8_t, std::queue<std::queue<NetworkPacket>>> m_streamQueues; //<ID of stream; queue of decoder packets (which are queues of network packets)>
		std::map<uint8_t, std::queue<std::queue<NetworkPacket>>>::iterator m_currentStream; //The stream we are currently sending a decoder packet on.
#endif

		struct {
			std::string address;
			std::string port;
		} m_remote;

		uint64_t lastBandwidthTimestamp;
		uint64_t lastTimestamp;
		uint64_t bandwidthBytes;
		uint32_t debugStream;
		bool doChecksums;
		bool mDebugNetworkPackets=false;
		float bandwidthKPerS;
		uint64_t throttleRate; //Unused
		uint64_t socketBufferSize; //Amount of bytes the client has in their socket's buffer to receive data.
		uint8_t estimatedDecodingFrequency; //Estimated times per second the client will decode the packets sent to them.
		std::unique_ptr<ElasticFrameProtocolSender> m_EFPSender;
		std::vector<NetworkSinkStream> m_streams;
		std::unordered_map<int, uint32_t> m_streamIndices;
		std::queue<std::vector<uint8_t>> m_dataQueue;
		size_t m_maxPacketCountPerFrame;
		size_t m_packetsSent;
		std::unordered_map<uint32_t, std::unique_ptr<StreamParserInterface>> m_parsers;

		Result packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex);
		void sendOrCacheData(const std::vector<uint8_t>& subPacket);
		void sendData(const std::vector<uint8_t>& subPacket);
	};

} // avs