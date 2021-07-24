// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <iostream>
#include <networksource_p.hpp>
#include <ElasticFrameProtocol.h>
#include <libavstream\queue.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#endif

#include <util/srtutil.h>


using namespace avs;
//static uint32_t gaps=0;

#define EFPFAILED(x) \
	((x) != ElasticFrameMessages::noError)

NetworkSource::NetworkSource()
	: Node(new NetworkSource::Private(this))
{
	m_data = static_cast<NetworkSource::Private*>(m_d);
}

Result NetworkSource::configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params)
{
	size_t numOutputs = streams.size();

	if (numOutputs == 0 || params.localPort == 0 || params.remotePort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!params.remoteIP || !params.remoteIP[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	try
	{
		m_data->bOneTimeWarnings=true;

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

		int32_t latency=60;
		CHECK_SRT_ERROR(srt_setsockopt(m_data->m_socket, 0, SRTO_RCVLATENCY, &latency, sizeof latency));

		m_data->remote_addr = CreateAddrInet(params.remoteIP, params.remotePort);
		m_data->remote_addr.sin_family = AF_INET;
		m_data->remote_addr.sin_port = htons(params.remotePort);
		if (inet_pton(AF_INET, params.remoteIP, &m_data->remote_addr.sin_addr) != 1)
		{
			return Result::Node_NotReady;
		}
		srt_setsockflag(m_data->m_socket, SRTO_SENDER, &yes, sizeof yes);
		m_data->bConnected=false;
        int events = SRT_EPOLL_IN | SRT_EPOLL_ERR|SRT_EPOLL_OUT;
		CHECK_SRT_ERROR(srt_epoll_add_usock(m_data->pollid,m_data->m_socket, &events));
		
		m_data->m_remote.address = params.remoteIP;
		m_data->m_remote.port = std::to_string(params.remotePort);
		m_data->bandwidthBytes=0;
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "NetworkSource: Failed to bind to UDP socket: " << e.what();
		return Result::Network_BindFailed;
	}

	setNumOutputSlots(numOutputs);

	m_data->m_streams = std::move(streams);

	for (size_t i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_data->m_streams[i];
		m_data->m_streamNodeMap[stream.id] = i;
	}

	m_data->m_tempBuffer.resize(200000);

	m_data->m_params = params;
	m_data->m_EFPReceiver.reset(new ElasticFrameProtocolReceiver(100, 0, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION));

	m_data->m_EFPReceiver->receiveCallback = [this](ElasticFrameProtocolReceiver::pFramePtr &rPacket, ElasticFrameProtocolContext* pCTX)->void
	{
		if (rPacket->mBroken)
		{
			AVSLOG(Warning) << "Received NAL-units of size: " << unsigned(rPacket->mFrameSize) <<
				" Stream ID: " << unsigned(rPacket->mStreamID) <<
				" PTS: " << unsigned(rPacket->mPts) <<
				" Corrupt: " << rPacket->mBroken <<
				" EFP connection: " << unsigned(rPacket->mSource) << "\n";
			std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
			m_data->m_counters.incompleteDecoderPacketsReceived++;
		}
		else
		{
			std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
			m_data->m_counters.decoderPacketsReceived++;
		}

		size_t bufferSize = sizeof(NetworkFrameInfo) + rPacket->mFrameSize;
		if (bufferSize > m_data->m_tempBuffer.size())
		{
			m_data->m_tempBuffer.resize(bufferSize);
		}
		
		NetworkFrameInfo frameInfo;
		frameInfo.pts = rPacket->mPts;
		frameInfo.dts = rPacket->mDts;
		frameInfo.dataSize = rPacket->mFrameSize;
		frameInfo.broken = rPacket->mBroken;

		memcpy(m_data->m_tempBuffer.data(), &frameInfo, sizeof(NetworkFrameInfo));
		memcpy(&m_data->m_tempBuffer[sizeof(NetworkFrameInfo)], rPacket->pFrameData, rPacket->mFrameSize);

		int nodeIndex = m_data->m_streamNodeMap[rPacket->mStreamID];

		auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
		if (!outputNode)
		{
			AVSLOG(Warning) << "NetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
			return;
		}

		size_t numBytesWrittenToOutput;
		auto result = outputNode->write(m_data->q_ptr(), m_data->m_tempBuffer.data(), bufferSize, numBytesWrittenToOutput);

		if (!result)
		{
			AVSLOG(Warning) << "NetworkSource EFP Callback: Failed to write to output node.";
			return;
		}

		if (numBytesWrittenToOutput < bufferSize)
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

	m_data->m_counters = {};
	m_data->m_remote = {};
	m_data->m_streamNodeMap.clear();
	m_data->m_streams.clear();

	m_data->m_tempBuffer.clear();

	// Socket must be killed BEFORE service, or asio throws a fit.
	// At a guess, this is because socket was created from service, and expects it to still exist
	// as socket is shut down.
	closeSocket();

	return Result::OK;
}

void NetworkSource::closeSocket()
{
	std::lock_guard<std::mutex> guard(m_data->m_networkMutex);

	m_data->runningThread = false;
	m_data->bConnected=false;
	if(m_data->m_socket)
		srt_close(m_data->m_socket);
	if(m_data->pollid)
		srt_epoll_release(m_data->pollid);
	srt_cleanup();
	m_data->pollid=0;
	m_data->m_socket=0;
}

void NetworkSource::pollData()
{
	asyncRecvPackets();
	while (m_data->runningThread)
	{
		std::lock_guard<std::mutex> guard(m_data->m_networkMutex);
		asyncRecvPackets();
	}
}

Result NetworkSource::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumOutputSlots() == 0 || !m_data->m_socket)
	{
		return Result::Node_NotConfigured;
	}
    int srtrfdslen = 2;
    int srtwfdslen = 2;
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

	{
		// Counters will be written to on packet processing thread
		std::lock_guard<std::mutex> guard(m_data->m_dataMutex);

		SRT_TRACEBSTATS perf;
		// returns 0 if there's no error during execution and -1 if there is
		if (srt_bstats(m_data->m_socket, &perf, false) == 0)
		{
			// KiloBytes
			m_data->m_counters.bandwidthKPS = (float)perf.mbpsRecvRate * 1000.0f * 0.125f;
			m_data->bandwidthBytes = m_data->m_counters.bandwidthKPS * 1000.0f;
			m_data->m_counters.networkPacketsReceived = perf.pktRecv;
			m_data->m_counters.networkPacketsDropped = perf.pktRcvLoss;
		}

		m_data->m_counters.connectionTime += deltaTime * 0.001f;

		if (m_data->m_counters.connectionTime)
		{
			m_data->m_counters.decoderPacketsReceivedPerSec = m_data->m_counters.decoderPacketsReceived / m_data->m_counters.connectionTime;
		}
	}

	m_data->m_pipelineTimestamp = timestamp;
	
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

void NetworkSource::sendAck(NetworkPacket &packet)
{
	NetworkPacket ackPacket;
	ByteBuffer buffer;
	ackPacket.serialize(buffer);
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif
void NetworkSource::asyncRecvPackets()
{
	static uint8_t packetData[PacketFormat::MaxPacketSize];

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
}

size_t NetworkSource::getSystemBufferSize() const
{
	return 100000;
}

