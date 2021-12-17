// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#pragma once

#include <memory>
#include <string>
#include <queue>
#include <unordered_map>

#include <asio.hpp>

#include <common_p.hpp>
#include <node_p.hpp>
#include <network/packetformat.hpp>
#include <network/serial.hpp>
#include <util/bytebuffer.hpp>
#include <util/jitterbuffer.hpp>
#include <util/misc.hpp>
#include <util/ringbuffer.hpp>

#include <libavstream/networksource.hpp>

#if LIBAV_USE_SRT
#include <srt.h>
#endif
#include <thread>
#include "ElasticFrameProtocol.h"

#if IS_CLIENT
#include <libavstream/httputil.hpp>
#endif

namespace avs
{
	class NetworkPacketMap : public std::map<uint16_t, ValueWithTimestamp<NetworkPacket>>
	{
	public:
		uint16_t previousLastSeqPlusOne=0;
	};
	struct FragmentRange
	{
		void reset()
		{
			streamIndex=0;
			timestamp=0;
			localTimestamp=0;
			numFragments=0;
			frameFirstFlag=false;
			frameLastFlag=false;
		}
		bool has_value()
		{
			return (streamIndex!=0);
		}

		avs::NetworkPacketMap::iterator it;
		uint8_t  streamIndex=0;
		uint32_t timestamp=0;
		uint32_t localTimestamp=0;
		uint16_t numFragments=0;
		bool frameFirstFlag=false;
		bool frameLastFlag=false;
	};
	// A struct that defines a set of NetworkPackets that together form a single data packet.
	struct DecoderPacket
	{
		Serial<uint16_t> seqBegin;
		Serial<uint16_t> seqEnd;
		bool frameFirstFlag;
		bool frameLastFlag;
		BufferPool::Handle payload;
	};
	struct RawPacket
	{
		uint8_t data[PacketFormat::MaxPacketSize];
		uint32_t size:11;
		//uint32_t sent_ack:1;
	};
	struct NetworkSource::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(NetworkSource, PipelineNode)
			
#if LIBAV_USE_SRT
		SRTSOCKET m_socket;
		sockaddr_in remote_addr;
		bool bConnected=false;
		int pollid=0;
#else
		std::unique_ptr<asio::io_service> m_service;
		std::unique_ptr<udp::socket> m_socket;
		std::unique_ptr<udp::endpoint> m_endpoint;
#endif
		std::vector<NetworkSourceStream> m_streams;
		std::unordered_map<uint32_t, int> m_streamNodeMap;
		NetworkSourceParams m_params;
		NetworkSourceCounters m_counters;

		uint32_t m_pipelineTimestamp = 0;

		// Second level jitter queue with first level buffer queues for every stream.
		//std::unique_ptr<JitterBuffer<StreamBuffers>> m_jitterBuffer;

		struct
		{
			std::string address;
			std::string port;
		} m_remote;

		float bandwidthBytes;

		std::thread m_receiveThread;
		std::thread m_processThread;
		std::mutex m_networkMutex;
		std::mutex m_dataMutex;
		std::atomic_bool m_receivingPackets;

		int32_t debugStream;
		bool mDebugNetworkPackets=false;
		bool bDoChecksums=false;

		size_t m_systemBufferSize=0;

		bool bOneTimeWarnings=false;

		std::unique_ptr<ElasticFrameProtocolReceiver> m_EFPReceiver;

		std::vector<char> m_tempBuffer;
		RingBuffer<RawPacket, 12000> m_recvBuffer;
#if IS_CLIENT
		HTTPUtil m_httpUtil;
#endif
	};

} // avs