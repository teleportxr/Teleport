// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <iostream>
#include <networksource_p.hpp>
#include <ElasticFrameProtocol.h>
#include <libavstream\queue.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#endif
#if LIBAV_USE_SRT
#include <util/srtutil.h>
#endif

#ifndef LIBAV_USE_SRT
#define LIBAV_USE_SRT 1
#endif
using namespace avs;
//static uint32_t gaps=0;

#define EFPFAILED(x) \
	((x) != ElasticFrameMessages::noError)

NetworkSource::NetworkSource()
	: Node(new NetworkSource::Private(this))
{
	m_data = static_cast<NetworkSource::Private*>(m_d);
}

Result NetworkSource::configure(std::vector<NetworkSourceStream>&& streams, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSourceParams& params)
{
	size_t numOutputs = streams.size();

	if (numOutputs == 0 || localPort == 0 || remotePort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!remote || !remote[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	try
	{
		m_data->bOneTimeWarnings=true;
#if LIBAV_USE_SRT
		srt_startup();
		m_data->m_socket=srt_create_socket();
		if (m_data->m_socket == SRT_ERROR)
		{
			AVSLOG(Warning)<<"srt_socket: "<<srt_getlasterror_str();
			return Result::Node_NotReady;
		}
		m_data->pollid = srt_epoll_create();
		int yes = 1;
		int ten_thousand = 10000;	
		srt_setsockflag(m_data->m_socket, SRTO_SNDTIMEO, &ten_thousand, sizeof ten_thousand);
		srt_setsockflag(m_data->m_socket, SRTO_RCVTIMEO, &ten_thousand, sizeof ten_thousand);

		srt_setsockflag(m_data->m_socket, SRTO_PEERIDLETIMEO, &params.connectionTimeout, sizeof params.connectionTimeout);

		int32_t latency=12;
		CHECK_SRT_ERROR(srt_setsockopt(m_data->m_socket, 0, SRTO_RCVLATENCY, &latency, sizeof latency));
		m_data->remote_addr = CreateAddrInet(remote,remotePort);
		m_data->remote_addr.sin_family = AF_INET;
		m_data->remote_addr.sin_port = htons(remotePort);
		if (inet_pton(AF_INET,remote, &m_data->remote_addr.sin_addr) != 1)
		{
			return Result::Node_NotReady;
		}
		srt_setsockflag(m_data->m_socket, SRTO_SENDER, &yes, sizeof yes);
		m_data->bConnected=false;
        int events = SRT_EPOLL_IN | SRT_EPOLL_ERR|SRT_EPOLL_OUT;
		CHECK_SRT_ERROR(srt_epoll_add_usock(m_data->pollid,m_data->m_socket, &events));
	//CHECK_SRT_ERROR( srt_rendezvous(m_data->m_socket, (sockaddr*)&local_bind_addr, sizeof local_bind_addr,                                   (sockaddr*)&remote_addr, sizeof remote_addr));
		
		m_data->m_systemBufferSize = 1000000;
#else
		// deconfigure does this:
		{
			m_data->m_endpoint.reset();
		}
		// problem here:  socket must be reset before m_service, or crash happens.
		m_data->m_socket.reset();
		m_data->m_service.reset(new asio::io_service);
		udp::socket *udps = new udp::socket{ *m_data->m_service, udp::v4() };
		// problem here:  socket must be reset before m_service, or crash happens.
		m_data->m_socket.reset(udps);

		const asio::socket_base::reuse_address reuseAddr(true);
		m_data->m_socket->set_option(reuseAddr);
		const asio::socket_base::receive_buffer_size recvBufferSize(params.socketBufferSize);
		m_data->m_socket->set_option(recvBufferSize);
		asio::socket_base::receive_buffer_size real_option;
		m_data->m_socket->get_option(real_option);
		int real_size = real_option.value();
		if(real_size!=params.socketBufferSize)
		{
			AVSLOG(Warning)<<"Asio socket buffer size is not requested "<<params.socketBufferSize<<" but only "<<real_size<<"\n";
		}
		m_data->m_systemBufferSize = real_size;

		m_data->m_socket->bind(udp::endpoint{ udp::v4(), localPort });
#endif
		m_data->m_remote.address = remote;
		m_data->m_remote.port = std::to_string(remotePort);
		m_data->lastBandwidthTimestamp = 0;
		m_data->bandwidthBytes=0;
		m_data->lastBytesReceived = 0;
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "NetworkSource: Failed to bind to UDP socket: " << e.what();
		return Result::Network_BindFailed;
	}

	setNumOutputSlots(numOutputs);

	m_data->m_streams = std::move(streams);

	for (int i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_data->m_streams[i];
		m_data->m_streamNodeMap[stream.id] = i;
	}

	m_data->m_params = params;
	m_data->m_EFPReceiver.reset(new ElasticFrameProtocolReceiver(100, 0, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION));

	m_data->m_EFPReceiver->receiveCallback = [this](ElasticFrameProtocolReceiver::pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)->void
	{
		if (rPacket->mBroken)
		{
			AVSLOG(Warning) << "Received NAL-units of size " << unsigned(rPacket->mFrameSize) <<
				" stream " << unsigned(rPacket->mStreamID) <<
				" pts " << unsigned(rPacket->mPts) <<
				" is broken? " << rPacket->mBroken <<
				" from EFP connection " << unsigned(rPacket->mSource) << "\n";
			std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
			m_data->m_counters.incompleteDecoderPacketsReceived++;
		}
		else
		{
			std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
			m_data->m_counters.decoderPacketsReceived++;
		}

		std::vector<char> buffer(sizeof(NetworkFrameInfo) + rPacket->mFrameSize);

		NetworkFrameInfo frameInfo;
		frameInfo.pts = rPacket->mPts;
		frameInfo.dts = rPacket->mDts;
		frameInfo.dataSize = rPacket->mFrameSize;
		frameInfo.broken = rPacket->mBroken;

		memcpy(buffer.data(), &frameInfo, sizeof(NetworkFrameInfo));
		memcpy(&buffer[sizeof(NetworkFrameInfo)], rPacket->pFrameData, rPacket->mFrameSize);

		int nodeIndex = m_data->m_streamNodeMap[rPacket->mStreamID];

		auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
		if (!outputNode)
		{
			AVSLOG(Warning) << "NetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
			return;
		}

		size_t numBytesWrittenToOutput;
		auto result = outputNode->emplace(m_data->q_ptr(), std::move(buffer), numBytesWrittenToOutput);

		if (!result)
		{
			AVSLOG(Warning) << "NetworkSource EFP Callback: Failed to write to output node.";
			return;
		}

		if (numBytesWrittenToOutput < buffer.size())
		{
			AVSLOG(Warning) << "NetworkSource EFP Callback: Incomplete frame written to output node.";
		}
	};

	return Result::OK;
}

Result NetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}

	m_data->runningThread = false;
		
	if (m_data->thr.joinable())
	{
		std::lock_guard<std::mutex> guard(m_data->m_networkMutex);
		m_data->thr.join();
	}	

	// Will stop any extra EFP thread 
	m_data->m_EFPReceiver.reset();

	setNumOutputSlots(0);
	m_data->m_recvBuffer.reset();

	m_data->m_counters = {};
	m_data->m_remote = {};
	m_data->m_streamNodeMap.clear();
	m_data->m_streams.clear();

	// Socket must be killed BEFORE service, or asio throws a fit.
	// At a guess, this is because socket was created from service, and expects it to still exist
	// as socket is shut down.
#if LIBAV_USE_SRT
	closeSocket();
#else
	m_data->m_socket.reset();
	m_data->m_service.reset();
	m_data->m_endpoint.reset();
#endif

	return Result::OK;
}

void NetworkSource::closeSocket()
{
	std::lock_guard<std::mutex> guard(m_data->m_networkMutex);

	m_data->runningThread = false;
#if LIBAV_USE_SRT
	m_data->bConnected=false;
	if(m_data->m_socket)
		srt_close(m_data->m_socket);
	if(m_data->pollid)
		srt_epoll_release(m_data->pollid);
	srt_cleanup();
	m_data->pollid=0;
	m_data->m_socket=0;
#endif
}

void NetworkSource::pollData()
{
	asyncRecvPackets();
	while (m_data->runningThread)
	{
		std::lock_guard<std::mutex> guard(m_data->m_networkMutex);
#if LIBAV_USE_SRT
		asyncRecvPackets();
#else
		// Poll() accumulates NetworkPackets in recvBuffer.
		while (m_data->m_service->poll() > 0)
		{
			//std::this_thread::sleep_for(std::chrono::nanoseconds(10));
			std::this_thread::yield();
		}
		//std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		std::this_thread::yield();
#endif
	}
}

Result NetworkSource::process(uint32_t timestamp)
{
	if (getNumOutputSlots() == 0 || !m_data->m_socket)
	{
		return Result::Node_NotConfigured;
	}
    int srtrfdslen = 2;
    int srtwfdslen = 2;
#if LIBAV_USE_SRT
    SRTSOCKET srtrwfds[4] = { SRT_INVALID_SOCK, SRT_INVALID_SOCK , SRT_INVALID_SOCK  };
    int sysrfdslen = 2;
    SYSSOCKET sysrfds[2];
    if (srt_epoll_wait(m_data->pollid,
        &srtrwfds[0], &srtrfdslen, &srtrwfds[2], &srtwfdslen,
        100,
        &sysrfds[0], &sysrfdslen, 0, 0) >= 0)
	{
        for (size_t i = 0; i < sizeof(srtrwfds) / sizeof(SRTSOCKET); i++)
        {
            SRTSOCKET s = srtrwfds[i];
            if (s == SRT_INVALID_SOCK)
                continue;
            bool issource = (s==m_data->m_socket);
			if(!issource)
				continue;
			SRT_SOCKSTATUS status = srt_getsockstate(s);
			switch (status)
			{
				case SRTS_LISTENING:
					AVSLOG(Info) << "SRTS_LISTENING \n";
					break;
				case SRTS_BROKEN:
					AVSLOG(Error) << "SRTS_BROKEN \n";
					closeSocket();
					break;
				case SRTS_NONEXIST:
					AVSLOG(Error) << "SRTS_NONEXIST \n";
					closeSocket();
					break;
				case SRTS_OPENED:
					AVSLOG(Info) << "SRTS_OPENED \n";
					break;
				case SRTS_CLOSED:
					AVSLOG(Error) << "SRTS_CLOSED \n";
					closeSocket();
					break;
				case SRTS_CONNECTED:
					//AVSLOG(Info) << "SRTS_CONNECTED \n";
					m_data->bConnected=true;
					break;
				default:
					break;
			};
		}
	}
#endif
	{
		// Counters will be written to on packet processing thread
		std::lock_guard<std::mutex> guard(m_data->m_dataMutex);

		uint64_t bytes_this_frame = m_data->m_counters.bytesReceived - m_data->lastBytesReceived;
		m_data->bandwidthBytes += bytes_this_frame;

		int64_t time_step_ns = timestamp - m_data->lastBandwidthTimestamp;
		if (time_step_ns > 1000000 && m_data->bandwidthBytes > 0)
		{
			float bandwidth_bytes_per_ms = float(m_data->bandwidthBytes) / float(time_step_ns);
			m_data->m_counters.bandwidthKPS *= 0.9f;
			m_data->m_counters.bandwidthKPS += 0.1f * 1000.0f * bandwidth_bytes_per_ms / 1024.0f;
			m_data->lastBandwidthTimestamp = timestamp;

			m_data->bandwidthBytes = 0;
		}
		m_data->lastBytesReceived = m_data->m_counters.bytesReceived;
	}

	m_data->m_pipelineTimestamp = timestamp;
	
#if LIBAV_USE_SRT
	if(!m_data->bConnected)
	{
		int res=srt_connect(m_data->m_socket, (sockaddr*)&m_data->remote_addr, sizeof m_data->remote_addr);
		if(res)
		{
			CHECK_SRT_ERROR(res);
		}
	}
	if(!m_data->bConnected)
		return Result::Node_NotReady;
	if (!m_data->thr.joinable())
	{
		m_data->runningThread = true;
		m_data->thr = std::thread(&NetworkSource::pollData, this);
#ifdef __ANDROID__
		pthread_setname_np(m_data->thr .native_handle(), "buffer_thread");
		sched_param sch_params;
		// Roderick: Upping the priority to 99 really seems to help avoid dropped packets.
		sch_params.sched_priority = 99;
		pthread_setschedparam(m_data->thr .native_handle(), SCHED_RR, &sch_params);
#endif
	}
#else
	assert(m_data->m_service);
	if (!m_data->m_endpoint)
	{
		try
		{
			udp::resolver resolver(*m_data->m_service);
			auto results = resolver.resolve({ udp::v4(), m_data->m_remote.address, m_data->m_remote.port });
			if (results.empty())
			{
				throw std::runtime_error("Remote host not found");
			}
			m_data->m_endpoint.reset(new udp::endpoint{ *results });
			//asyncRecvPackets();
		}
		catch (const std::exception& e)
		{
			AVSLOG(Error) << "NetworkSource: Failed to resolve remote endpoint: " << e.what();
			return Result::Network_ResolveFailed;
		}
	}
	
#endif
	try
	{
#if !(LIBAV_USE_SRT)
		if (!m_data->thr.joinable())
		{
			m_data->runningThread = true;
			m_data->thr = std::thread(&NetworkSource::pollData, this);
#ifdef __ANDROID__
			pthread_setname_np(m_data->thr .native_handle(), "buffer_thread");
			sched_param sch_params;
			// Roderick: Upping the priority to 99 really seems to help avoid dropped packets.
			sch_params.sched_priority = 99;
			pthread_setschedparam(m_data->thr .native_handle(), SCHED_RR, &sch_params);
#endif
		}
#endif
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "NetworkSource: Network poll failed: " << e.what();
		return Result::Network_RecvFailed;
	}
	return Result::OK;
}

NetworkSourceCounters NetworkSource::getCounterValues() const
{
	return m_data->m_counters;
}

void NetworkSource::setDebugStream(uint32_t s)
{
	m_data->debugStream = s;
}

void NetworkSource::setDoChecksums(bool s)
{
	m_data->bDoChecksums = s;
}

void NetworkSource::setDebugNetworkPackets(bool s)
{
	m_data->mDebugNetworkPackets = s;
}

size_t NetworkSource::getSystemBufferSize() const
{
	return m_data->m_systemBufferSize;
}

void NetworkSource::sendAck(NetworkPacket &packet)
{
	NetworkPacket ackPacket;
	ByteBuffer buffer;
	ackPacket.serialize(buffer);
}

void NetworkSource::accumulatePackets()
{
	while (!m_data->m_recvBuffer.empty())
	{
		RawPacket rawPacket = m_data->m_recvBuffer.get();
		size_t bytesRecv = (size_t)rawPacket.size;
		if (bytesRecv == 0)
		{
			AVSLOG(Warning) << "No bytes in packet  " << "\n";
			continue;
		}
		if (m_data->mDebugNetworkPackets)
		{
			NetworkPacket packet;
			ByteBuffer buffer(bytesRecv);
			memcpy(buffer.data(), rawPacket.data, bytesRecv);
			if (packet.unserialize(buffer, 0, bytesRecv))
			{
				sendAck(packet);
				if (packet.streamIndex == m_data->debugStream)
				{
					unsigned long long checksum = 0;
					if (m_data->bDoChecksums)
						for (size_t i = 0; i < (bytesRecv); i++)
						{
							checksum += (unsigned long long)rawPacket.data[i];
						}
					if (packet.flags.fragmentFirst&&packet.flags.fragmentLast)
					{
						AVSLOG(Warning) << m_data->debugStream << " - Packet Whole Fragment " << packet.sequence << " " << bytesRecv << " with checksum " << checksum << " at time=" << m_data->m_pipelineTimestamp << "\n";
					}
					else if (packet.flags.fragmentFirst)
					{
						AVSLOG(Warning) << m_data->debugStream << " - Packet fragmentFirst " << packet.sequence << " " << bytesRecv << " with checksum " << checksum << " at time=" << m_data->m_pipelineTimestamp << "\n";
					}
					else if (packet.flags.fragmentLast)
					{
						AVSLOG(Warning) << m_data->debugStream << " - Packet fragmentLast " << packet.sequence << " " << bytesRecv << " with checksum " << checksum << " at time=" << m_data->m_pipelineTimestamp << "\n";
					}
					else
					{
						AVSLOG(Warning) << m_data->debugStream << " - Packet " << packet.sequence << " " << bytesRecv << " with checksum " << checksum << " at time=" << m_data->m_pipelineTimestamp << "\n";
					}
				}
			}
		}
		m_data->m_counters.bytesReceived += bytesRecv;
		m_data->m_counters.networkPacketsReceived++;

		auto val = m_data->m_EFPReceiver->receiveFragmentFromPtr(rawPacket.data, bytesRecv, 0);
		if (EFPFAILED(val))
		{
			AVSLOG(Warning) << "EFP Error: Invalid data fragment received" << "\n";
		}
		rawPacket.size = 0;
	}
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif
void NetworkSource::asyncRecvPackets()
{
	static uint8_t packetData[PacketFormat::MaxPacketSize];
#if LIBAV_USE_SRT
	int st = srt_recvmsg(m_data->m_socket, (char*)packetData, PacketFormat::MaxPacketSize);
    if (st <= 0)
    {
		std::this_thread::sleep_for(std::chrono::nanoseconds(5)); // 0.000000005s (cross platform solution) same as Sleep(100)
		std::this_thread::yield();
        return;
    }

	{
		std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
		m_data->m_counters.bytesReceived += PacketFormat::MaxPacketSize;
		m_data->m_counters.networkPacketsReceived++;
	}

	auto val = m_data->m_EFPReceiver->receiveFragmentFromPtr(packetData, PacketFormat::MaxPacketSize, 0);
	if (EFPFAILED(val))
	{
		AVSLOG(Warning) << "EFP Error: Invalid data fragment received" << "\n";
	}
#else
	assert(m_data->m_socket);
	assert(m_data->m_endpoint);
	RawPacket *rawPacket= m_data->m_recvBuffer.reserve_next();
	m_data->m_socket->async_receive_from(asio::buffer(rawPacket->data,PacketFormat::MaxPacketSize), *(m_data->m_endpoint.get()),
		[this, rawPacket](asio::error_code ec, size_t bytesRecv)
			{
				if (!ec && bytesRecv > 0)
				{
					rawPacket->size= bytesRecv;
//					rawPacket->sent_ack=false;
					if (!m_data->m_recvBuffer.full())
					{
						m_data->m_recvBuffer.commit_next();
						std::this_thread::yield();
						asyncRecvPackets();
					}
					else
					{
						AVSLOG(Warning) << "Ring buffer exhausted " << "\n";
						return;
					}
				}
				else if (ec)
				{
					AVSLOG(Warning) << " asio: " << ec.value()<<"\n" ;
				}
				else
				{
					AVSLOG(Warning) << " no m_data in async_receive_from\n" ;
				}
			});
#endif
}

