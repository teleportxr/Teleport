// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include <iostream>
#include "networksource_p.hpp"
#include <ElasticFrameProtocol.h>
#include <libavstream/queue.hpp>
#include <libavstream/timer.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#include <sys/prctl.h>
#endif

#include <util/srtutil.h>
#include "TeleportCore/ErrorHandling.h"


using namespace avs;

#define EFPFAILED(x) \
	((x) != ElasticFrameMessages::noError)

SrtEfpNetworkSource::SrtEfpNetworkSource()
	: NetworkSource(new SrtEfpNetworkSource::Private(this))
{
	m_data = static_cast<SrtEfpNetworkSource::Private*>(m_d);
}

Result SrtEfpNetworkSource::configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params)
{
	size_t numOutputs = streams.size();

	if (numOutputs == 0 || params.remotePort == 0 || params.remoteHTTPPort == 0)
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
#if NDEBUG
		//srt_logging::LogLevel::type loglevel = srt_logging::LogLevel::error;
#else
		//srt_logging::LogLevel::type loglevel = srt_logging::LogLevel::debug;
#endif

		m_data->m_socket=srt_create_socket();
		if (m_data->m_socket == SRT_ERROR)
		{
			AVSLOG(Warning)<<"srt_socket: "<<srt_getlasterror_str();
			return Result::Node_NotReady;
		}
		m_data->pollid = srt_epoll_create();
		int yes = 1;
		int hundred = 100;
		int ten_thousand = 10000;	
		srt_setsockflag(m_data->m_socket, SRTO_SNDTIMEO, &hundred, sizeof hundred);
		srt_setsockflag(m_data->m_socket, SRTO_RCVTIMEO, &hundred, sizeof hundred);

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

		TimerUtil::Start();
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "SrtEfpNetworkSource: Failed to bind to UDP socket: " << e.what();
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

		size_t bufferSize = sizeof(StreamPayloadInfo) + rPacket->mFrameSize;
		if (bufferSize > m_data->m_tempBuffer.size())
		{
			m_data->m_tempBuffer.resize(bufferSize);
		}
		
		StreamPayloadInfo frameInfo;
		frameInfo.frameID = rPacket->mPts;
		frameInfo.dataSize = rPacket->mFrameSize;
		frameInfo.connectionTime = TimerUtil::GetElapsedTimeS();
		frameInfo.broken = rPacket->mBroken;

		memcpy(m_data->m_tempBuffer.data(), &frameInfo, sizeof(StreamPayloadInfo));
		memcpy(&m_data->m_tempBuffer[sizeof(StreamPayloadInfo)], rPacket->pFrameData, rPacket->mFrameSize);

		int nodeIndex = m_data->m_streamNodeMap[rPacket->mStreamID];

		auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
		if (!outputNode)
		{
			AVSLOG(Warning) << "SrtEfpNetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
			return;
		}

		size_t numBytesWrittenToOutput;
		auto result = outputNode->write(m_data->q_ptr(), m_data->m_tempBuffer.data(), bufferSize, numBytesWrittenToOutput);

		if (!result)
		{
			AVSLOG(Warning) << "SrtEfpNetworkSource EFP Callback: Failed to write to output node.";
			return;
		}

		if (numBytesWrittenToOutput < bufferSize)
		{
			AVSLOG(Warning) << "SrtEfpNetworkSource EFP Callback: Incomplete frame written to output node.";
		}
	};

#if TELEPORT_CLIENT
	HTTPUtilConfig httpUtilConfig;
	httpUtilConfig.remoteIP = params.remoteIP;
	httpUtilConfig.remoteHTTPPort = params.remoteHTTPPort;
	httpUtilConfig.maxConnections = params.maxHTTPConnections;
	httpUtilConfig.useSSL = params.useSSL;
	auto f = std::bind(&SrtEfpNetworkSource::receiveHTTPFile, this, std::placeholders::_1, std::placeholders::_2);
	return m_data->m_httpUtil.initialize(httpUtilConfig, std::move(f));
#else
	return Result::OK;
#endif
}

void SrtEfpNetworkSource::receiveHTTPFile(const char* buffer, size_t bufferSize)
{
	int nodeIndex = m_data->m_streamNodeMap[m_data->m_params.httpStreamID];

	auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
	if (!outputNode)
	{
		AVSLOG(Warning) << "SrtEfpNetworkSource HTTP Callback: Invalid output node. Should be an avs::Queue.";
		return;
	}

	size_t numBytesWrittenToOutput;
	auto result = outputNode->write(m_data->q_ptr(), buffer, bufferSize, numBytesWrittenToOutput);

	if (!result)
	{
		AVSLOG(Warning) << "SrtEfpNetworkSource HTTP Callback: Failed to write to output node.";
		return;
	}

	if (numBytesWrittenToOutput < bufferSize)
	{
		AVSLOG(Warning) << "SrtEfpNetworkSource HTTP Callback: Incomplete payload written to output node.";
		return;
	}

	{
		std::lock_guard<std::mutex> guard(m_data->m_dataMutex);
		m_data->m_counters.httpFilesReceived++;
	}
}

Result SrtEfpNetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}

	m_data->m_receivingPackets = false;
		
	if (m_data->m_receiveThread.joinable())
	{
		m_data->m_receiveThread.join();
	}	

	if (m_data->m_processThread.joinable())
	{
		std::lock_guard<std::mutex> guard(m_data->m_networkMutex);
		m_data->m_processThread.join();
	}

	// Will stop any extra EFP thread 
	m_data->m_EFPReceiver.reset();

	setNumOutputSlots(0);

	m_data->m_counters = {};
	m_data->m_remote = {};
	m_data->m_streamNodeMap.clear();
	m_data->m_streams.clear();

	m_data->m_tempBuffer.clear();

	m_data->m_recvBuffer.reset();

	// Socket must be killed BEFORE service, or asio throws a fit.
	// At a guess, this is because socket was created from service, and expects it to still exist
	// as socket is shut down.
	closeSocket();

#if TELEPORT_CLIENT
	return m_data->m_httpUtil.shutdown();
#else
	return Result::OK;
#endif
}

void SrtEfpNetworkSource::closeSocket()
{
	std::lock_guard<std::mutex> guard(m_data->m_networkMutex);

	m_data->m_receivingPackets = false;
	m_data->bConnected=false;
	if(m_data->m_socket)
		srt_close(m_data->m_socket);
	if(m_data->pollid)
		srt_epoll_release(m_data->pollid);
	srt_cleanup();
	m_data->pollid=0;
	m_data->m_socket=0;
}

Result SrtEfpNetworkSource::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumOutputSlots() == 0 || !m_data->m_socket)
	{
		return Result::Node_NotConfigured;
	}

	if (!m_data->bConnected)
	{
		int res = srt_connect(m_data->m_socket, (sockaddr*)&m_data->remote_addr, sizeof m_data->remote_addr);
		if (res)
		{
			CHECK_SRT_ERROR(res);
			// Try to reset the socket.
			closeSocket();
			return avs::Result::Failed;
		}
	}

    int srtrfdslen = 2;
    int srtwfdslen = 2;
    SRTSOCKET srtrwfds[4] = { SRT_INVALID_SOCK, SRT_INVALID_SOCK , SRT_INVALID_SOCK, SRT_INVALID_SOCK };
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
					return Result::Network_Disconnection;
				case SRTS_NONEXIST:
					AVSLOG(Error) << "SRTS_NONEXIST \n";
					closeSocket();
					return Result::Network_Disconnection;
				case SRTS_OPENED:
					AVSLOG(Info) << "SRTS_OPENED \n";
					break;
				case SRTS_CLOSED:
					AVSLOG(Error) << "SRTS_CLOSED \n";
					closeSocket();
					return Result::Network_Disconnection;
				case SRTS_CONNECTED:
					//AVSLOG(Info) << "SRTS_CONNECTED \n";
					m_data->bConnected=true;
					break;
				default:
					break;
			};
		}
	}
	else
	{
		closeSocket();
		return Result::Network_Disconnection;
	}

	if (!m_data->bConnected)
		return Result::Node_NotReady;

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

		double connectionTime = TimerUtil::GetElapsedTimeS();
		if (connectionTime)
		{
			m_data->m_counters.decoderPacketsReceivedPerSec = m_data->m_counters.decoderPacketsReceived / connectionTime;
		}
	}

	m_data->m_pipelineTimestamp = timestamp;
	

	if (!m_data->m_receiveThread.joinable() && !m_data->m_receivingPackets)
	{
		m_data->m_receivingPackets = true;
		m_data->m_receiveThread = std::thread(&SrtEfpNetworkSource::asyncReceivePackets, this);
#ifdef __ANDROID__
		pthread_setname_np(m_data->m_receiveThread .native_handle(), "receive_packets_thread");
		sched_param sch_params;
		// Roderick: Upping the priority to 99 really seems to help avoid dropped packets.
		sch_params.sched_priority = 99;
		pthread_setschedparam(m_data->m_receiveThread .native_handle(), SCHED_RR, &sch_params);
#endif
	}

	if (!m_data->m_params.asyncProcessPackets)
	{
		processPackets();
	}
	else if (!m_data->m_processThread.joinable())
	{
		m_data->m_processThread = std::thread(&SrtEfpNetworkSource::asyncProcessPackets, this);
#ifdef __ANDROID__
		pthread_setname_np(m_data->m_processThread.native_handle(), "process_packets_thread");
		sched_param sch_params;
		sch_params.sched_priority = 99;
		pthread_setschedparam(m_data->m_processThread.native_handle(), SCHED_RR, &sch_params);
#endif
	}

#if TELEPORT_CLIENT
	return m_data->m_httpUtil.process();
#else
	return Result::OK;
#endif
}

NetworkSourceCounters SrtEfpNetworkSource::getCounterValues() const
{
	return m_data->m_counters;
}

void SrtEfpNetworkSource::setDebugStream(uint32_t s)
{
	m_data->debugStream = s;
}

void SrtEfpNetworkSource::setDoChecksums(bool s)
{
	m_data->bDoChecksums = s;
}

void SrtEfpNetworkSource::setDebugNetworkPackets(bool s)
{
	m_data->mDebugNetworkPackets = s;
}

void SrtEfpNetworkSource::sendAck(NetworkPacket &packet)
{
	NetworkPacket ackPacket;
	ByteBuffer buffer;
	ackPacket.serialize(buffer);
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif

void SrtEfpNetworkSource::asyncReceivePackets()
{
#ifdef __ANDROID__
	const char *newName="asyncReceivePackets";
	if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(const_cast<char *>(newName)), NULL, NULL, NULL))
	{
		TELEPORT_CERR<<"prctl(PR_SET_NAME, \"%s\") error - %s"<< newName<<" "<< strerror(errno);
	}
#endif
	while (m_data->m_receivingPackets)
	{
		RawPacket* rawPacket = m_data->m_recvBuffer.reserve_next();
		int st = srt_recvmsg(m_data->m_socket, (char*)rawPacket, PacketFormat::MaxPacketSize);
		// 0 means connection has been close and -1 means an error occurred.
		if (st <= 0)
		{
			//std::this_thread::sleep_for(std::chrono::nanoseconds(5)); // 0.000000005s 
			std::this_thread::yield();
			break;
		}

		if (!m_data->m_recvBuffer.full())
		{
			m_data->m_recvBuffer.commit_next();
			//std::this_thread::yield();
		}
		else
		{
			AVSLOG(Warning) << "Ring buffer exhausted " << "\n";
		}
	}
}

void SrtEfpNetworkSource::asyncProcessPackets()
{
	while (m_data->m_receivingPackets)
	{
		processPackets();
		std::this_thread::yield();
	}
}

void SrtEfpNetworkSource::processPackets()
{
	RawPacket rawPacket;
	while (!m_data->m_recvBuffer.empty())
	{
		//m_data->m_recvBuffer.copyTail(&rawPacket);
		rawPacket = m_data->m_recvBuffer.get();
		{
			std::lock_guard<std::mutex> lock(m_data->m_dataMutex);
			m_data->m_counters.bytesReceived += PacketFormat::MaxPacketSize;
			m_data->m_counters.networkPacketsReceived++;
		}

		auto val = m_data->m_EFPReceiver->receiveFragmentFromPtr(rawPacket.data, PacketFormat::MaxPacketSize, 0);
		if (EFPFAILED(val))
		{
			AVSLOG(Warning) << "EFP Error: Invalid data fragment received" << "\n";
		}
	}
}

size_t SrtEfpNetworkSource::getSystemBufferSize() const
{
	return 100000;
}

#if TELEPORT_CLIENT
std::queue<HTTPPayloadRequest>& SrtEfpNetworkSource::GetHTTPRequestQueue()
{
	return m_data->m_httpUtil.GetRequestQueue();
}
#endif

