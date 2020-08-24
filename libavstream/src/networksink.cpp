// libavstream
// (c) Copyright 2018-2019 Simul Software Ltd

#include <networksink_p.hpp>
#include <network/packetformat.hpp>
#if LIBAV_USE_SRT
#include <util/srtutil.h>
#endif
#include <iostream>
#include <cmath>

using namespace avs;

NetworkSink::NetworkSink()
	: Node(new NetworkSink::Private(this)), m_data((NetworkSink::Private*)(m_d))
{}

Result NetworkSink::configure(std::vector<NetworkSinkStream>&& streams, const char* local_bind, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSinkParams& params)
{
	size_t numInputs = streams.size();
	if (numInputs == 0 || localPort == 0 || remotePort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!remote || !remote[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	if (numInputs > (size_t)PacketFormat::MaxNumStreams)
	{
		return Result::Node_InvalidConfiguration;
	}

	try
	{
#if LIBAV_USE_SRT
		srt_startup();
		srt_logging::LogLevel::type loglevel = srt_logging::LogLevel::debug;
		srt_setloglevel(loglevel);
		m_data->pollid = srt_epoll_create();
		m_data->m_socket = srt_create_socket();
		int no = 0;
		//int hundred = 100;
		int thousand = 1000;
		//int thirty_thousand = 30000;
		//int million = 1000000;
		srt_setsockflag(m_data->m_socket, SRTO_RCVSYN, &no, sizeof no);
		//srt_setsockflag(m_data->m_socket, SRTO_SNDSYN, &no, sizeof no);
		srt_setsockflag(m_data->m_socket, SRTO_SNDTIMEO, &thousand, sizeof thousand);
		srt_setsockflag(m_data->m_socket, SRTO_RCVTIMEO, &thousand, sizeof thousand);

		//SRT_TRANSTYPE tt = SRTT_LIVE;
		//CHECK_SRT_ERROR(srt_setsockopt(m_data->m_socket, 0, SRTO_TRANSTYPE, &tt, sizeof tt));
		//int32_t latency=params.requiredLatencyMs;
		//CHECK_SRT_ERROR(srt_setsockopt(m_data->m_socket, 0, SRTO_PEERLATENCY, &latency, sizeof latency));
		int payloadSize = avs::PacketFormat::MaxPacketSize;
		CHECK_SRT_ERROR(srt_setsockopt(m_data->m_socket, 0, SRTO_PAYLOADSIZE, &payloadSize, sizeof(payloadSize)));
		int events = SRT_EPOLL_IN | SRT_EPOLL_ERR | SRT_EPOLL_OUT;
		CHECK_SRT_ERROR(srt_epoll_add_usock(m_data->pollid, m_data->m_socket, &events));
		sockaddr_in local_bind_addr;//= CreateAddrInet(local_bind,localPort);

		// bind the socket to the local address and port.
		local_bind_addr.sin_family = AF_INET;
		local_bind_addr.sin_port = htons(localPort);
		if (local_bind)
		{
			if (inet_pton(AF_INET, local_bind, &local_bind_addr.sin_addr) != 1)
				return Result::Node_InvalidConfiguration;
		}
		else
		{
			local_bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		CHECK_SRT_ERROR(srt_bind(m_data->m_socket, (struct sockaddr*)&local_bind_addr, sizeof(sockaddr_in)));

		srt_listen(m_data->m_socket, 2);

#else
		m_data->m_service.reset(new asio::io_service);

		m_data->m_socket.reset(new udp::socket{ *m_data->m_service, udp::v4() });

		const asio::socket_base::reuse_address reuseAddr(true);
		m_data->m_socket->set_option(reuseAddr);
		const asio::socket_base::send_buffer_size sendBufferSize(params.socketBufferSize);
		m_data->m_socket->set_option(sendBufferSize);

		m_data->m_socket->bind(udp::endpoint{ udp::v4(), localPort });
		m_data->m_remote.address = remote;
		m_data->m_remote.port = std::to_string(remotePort);
#endif

		m_data->lastBandwidthTimestamp = 0;
		m_data->bandwidthBytes = 0;
		m_data->bandwidthKPerS = 0.0f;

		m_data->throttleRate = params.throttleToRateKpS;
		m_data->socketBufferSize = params.socketBufferSize;
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "NetworkSink: Failed to bind to UDP socket: " << e.what();
		return Result::Network_BindFailed;
	}

	setNumInputSlots(numInputs);

	m_data->m_streams = std::move(streams);

	// Called by the parser interface if the stream uses one
	auto onPacketParsed = [](Node* node, uint32_t inputNodeIndex, const char* buffer, size_t dataSize, size_t dataOffset, bool isLastPayload)->Result
	{
		NetworkSink* ns = static_cast<NetworkSink*>(node);
		ns->m_data->packData((const uint8_t*)buffer + dataOffset, dataSize, inputNodeIndex);
		return Result::OK;
	};

	for (size_t i = 0; i < m_data->m_streams.size(); ++i)
	{
		auto& stream = m_data->m_streams[i];
		stream.buffer.resize(stream.chunkSize);
		if (stream.useParser)
		{
			auto parser = std::unique_ptr<StreamParserInterface>(avs::StreamParserInterface::Create(stream.parserType));
			parser->configure(this, onPacketParsed, i);
			m_data->m_parsers[i] = std::move(parser);
		}
	}

	m_data->m_EFPSender.reset(new ElasticFrameProtocolSender(PacketFormat::MaxPacketSize));
	// The callback will be called on the same thread calling 'packAndSendFromPtr'
	m_data->m_EFPSender->sendCallback = std::bind(&Private::sendOrCacheData, m_data, std::placeholders::_1);

	m_data->m_maxPacketCountPerFrame = 2500000 / 60 / PacketFormat::MaxPacketSize;
	m_data->m_packetsSent = 0;

	return Result::OK;
}

NetworkSink::~NetworkSink()
{
	deconfigure();
}

Result NetworkSink::deconfigure()
{
	while (!m_data->m_dataQueue.empty())
	{
		m_data->m_dataQueue.pop();
	}

	m_data->m_EFPSender.reset();
	m_data->m_parsers.clear();
	m_data->m_streams.clear();

	if (getNumInputSlots() <= 0)
		return Result::OK;
	setNumInputSlots(0);

	m_data->m_remote = {};

	m_data->m_counters = {};

#if LIBAV_USE_SRT
	srt_close(m_data->m_remote_socket);
	srt_close(m_data->m_socket);
	srt_epoll_release(m_data->pollid);
	m_data->bConnected = false;
	m_data->pollid = 0;
	m_data->m_socket = 0;
	m_data->m_remote_socket = 0;
	srt_cleanup();
#else
	m_data->m_socket->shutdown(asio::socket_base::shutdown_both);
	m_data->m_socket->close();

	m_data->m_socket.reset();
	m_data->m_endpoint.reset();
	m_data->m_service.reset();
#endif
	m_data->debugStream = 0;
	m_data->doChecksums = false;
	return Result::OK;
}

Result NetworkSink::process(uint32_t timestamp)
{
	if (getNumInputSlots() == 0 || !m_data->m_socket)
	{
		return Result::Node_NotConfigured;
	}
#if LIBAV_USE_SRT
	/*	if(!m_data->m_remote_socket||m_data->m_remote_socket<0)
		{
		   socklen_t sa_len = sizeof(sockaddr_in);
	//SRT_API SRTSOCKET srt_accept       (SRTSOCKET u, struct sockaddr* addr, int* addrlen);
			int res=srt_accept(m_data->m_socket, (struct sockaddr*)(&m_data->remote_addr),(int*) &sa_len);
			if(res>0)
			{
			AVSLOG(Info) << "srt_accept"<<" \n";
			}
		}*/
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
			SRT_SOCKSTATUS status = srt_getsockstate(s);
			if (!m_data->m_remote_socket)
			{
				switch (status)
				{
				case SRTS_LISTENING:
				{
					AVSLOG(Info) << "SRTS_LISTENING \n";
					socklen_t sa_len = sizeof(sockaddr_in);
					SRTSOCKET rem = srt_accept(m_data->m_socket, (struct sockaddr*)(&m_data->remote_addr), (int*)&sa_len);
					if (rem > 0)
					{
						m_data->m_remote_socket = rem;
						m_data->bConnected = true;
					}
				}
				break;
				case SRTS_CONNECTED:
					if (s > 0)
					{
						int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
						CHECK_SRT_ERROR(srt_epoll_add_usock(m_data->pollid, m_data->m_remote_socket, &events));
						m_data->bConnected = true;
						m_data->m_remote_socket = s;
						//AVSLOG(Info) << "SRTS_CONNECTED"<<"\n";
					}
					break;
				default:
					AVSLOG(Info) << "SRTS_? \n";
					break;
				};
			}
			bool issource = (s == m_data->m_remote_socket);
			if (!issource)
			{
				continue;
			}
			switch (status)
			{
			case SRTS_LISTENING:
				AVSLOG(Info) << "SRTS_LISTENING \n";
				break;
			case SRTS_BROKEN:
				AVSLOG(Error) << "SRTS_BROKEN \n";
				break;
			case SRTS_NONEXIST:
				AVSLOG(Error) << "SRTS_NONEXIST \n";
				break;
			case SRTS_CLOSED:
				AVSLOG(Error) << "SRTS_CLOSED \n";
				{
					m_data->bConnected = false;
					srt_close(m_data->m_remote_socket);
					m_data->m_remote_socket = 0;
				}
				break;
			case SRTS_CONNECTED:
				AVSLOG(Info) << "SRTS_CONNECTED \n";
				m_data->bConnected = true;
			default:
				break;
			};
		}
	}
	if (!m_data->m_remote_socket)
		return Result::Node_NotReady;
	//CHECK_SRT_ERROR(srt_connect(m_data->m_socket, (sockaddr*)&m_data->remote_addr, sizeof m_data->remote_addr));

	//CHECK_SRT_ERROR( srt_rendezvous(m_data->m_socket, (sockaddr*)&local_bind_addr, sizeof local_bind_addr,                                   (sockaddr*)&remote_addr, sizeof remote_addr));

	if (!m_data->bConnected)
		return Result::Node_NotReady;

	if (!m_data->m_remote_socket)
		return Result::Node_NotReady;
	SRT_SOCKSTATUS stat = srt_getsockstate(m_data->m_remote_socket);
	if (stat != SRT_SOCKSTATUS::SRTS_CONNECTED)
	{
		if (stat == SRT_SOCKSTATUS::SRTS_CLOSED)
		{
			m_data->bConnected = false;
			srt_close(m_data->m_remote_socket);
			m_data->m_remote_socket = 0;
		}
		return Result::Node_NotReady;
	}
	if (!m_data->bConnected)
		return Result::Node_NotReady;

	SRT_TRACEBSTATS perf;
	// returns 0 if there's no error during execution and -1 if there is
	if (srt_bstats(m_data->m_remote_socket, &perf, true) == 0)
	{
		m_data->bandwidthKPerS = (float)perf.mbpsBandwidth * 1000.0f;
		m_data->bandwidthBytes = (m_data->bandwidthKPerS * 1000.0f) / 8.0f;
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
		}
		catch (const std::exception& e)
		{
			AVSLOG(Error) << "NetworkSink: Failed to resolve remote endpoint: " << e.what();
			return Result::Network_ResolveFailed;
		}
	}
#endif

	const size_t numInputs = getNumInputSlots();
	if (numInputs == 0 || !m_data->m_EFPSender)
	{
		return Result::Node_NotConfigured;
	}

	// Called to get the data from the input nodes
	auto readInput = [this](uint32_t inputNodeIndex, size_t& numBytesRead) -> Result
	{
		Node* node = getInput(inputNodeIndex);
		auto& stream = m_data->m_streams[inputNodeIndex];
		
		assert(node);
		assert(stream.buffer.size() >= stream.chunkSize);

		if (IOInterface* nodeIO = dynamic_cast<IOInterface*>(node))
		{
			size_t bufferSize = stream.buffer.size(); 
			Result result = nodeIO->read(this, stream.buffer.data(), bufferSize, numBytesRead);
			if (result == Result::IO_Retry)
			{
				stream.buffer.resize(bufferSize);
				result = nodeIO->read(this, stream.buffer.data(), bufferSize, numBytesRead);
			}
			numBytesRead = std::min(bufferSize, numBytesRead);

			return result;
		}
		else
		{
			assert(false);
			return Result::Node_Incompatible;
		}
	};

	m_data->m_packetsSent = 0;

	for (int i = 0; i < (int)getNumInputSlots(); ++i)
	{
		const NetworkSinkStream& stream = m_data->m_streams[i];

		if (stream.isDataLimitPerFrame)
		{
			while (!m_data->m_dataQueue.empty() && m_data->m_packetsSent < m_data->m_maxPacketCountPerFrame)
			{
				m_data->sendData(m_data->m_dataQueue.front());
				m_data->m_dataQueue.pop();
			}
		}

		size_t numBytesRead = 0;
		try
		{
			if (Result result = readInput(i, numBytesRead); result != Result::OK)
			{
				if (result != Result::IO_Empty)
				{
					AVSLOG(Error) << "NetworkSink: Failed to read from input node: " << i << "\n";
					continue;
				}
			}
		}
		catch (const std::bad_alloc&)
		{
			return Result::IO_OutOfMemory;
		}

		if (numBytesRead == 0)
		{
			continue;
		}

		if (stream.useParser && m_data->m_parsers.find(i) != m_data->m_parsers.end())
		{
			m_data->m_parsers[i]->parse((const char*)stream.buffer.data(), numBytesRead);
		}
		else
		{
			m_data->packData(stream.buffer.data(), numBytesRead, i);
		}
	}

	return Result::OK;
}

void NetworkSink::Private::packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex)
{
	uint32_t code = 0;
	ElasticFrameContent dataContent;

	auto& stream = m_streams[inputNodeIndex];

	switch (stream.dataType)
	{
	case NetworkDataType::Geometry:
	case NetworkDataType::Audio:
		dataContent = ElasticFrameContent::privatedata;
		break;
	case NetworkDataType::H264:
		code = EFP_CODE('A', 'N', 'X', 'B');
		dataContent = ElasticFrameContent::h264;
		break;
	case NetworkDataType::HEVC:
		code = EFP_CODE('A', 'N', 'X', 'B');
		dataContent = ElasticFrameContent::h265;
		break;
	default:
		AVSLOG(Error) << "NetworkSink: Invalid stream datatype. Cannot send data. \n";
		return;
	}

	// total number of EFP superframes created since the start for this stream
	stream.counter++;

	m_EFPSender->packAndSendFromPtr(buffer,
		bufferSize,
		dataContent,
		stream.counter, // pts
		stream.counter, // dts
		code,
		stream.streamIndex,
		NO_FLAGS);
}

void NetworkSink::Private::sendOrCacheData(const std::vector<uint8_t>& subPacket)
{
	// streamID is second byte for all EFP packet types
	const auto& stream = m_streams[subPacket[1]];

	if (stream.isDataLimitPerFrame && m_packetsSent >= m_maxPacketCountPerFrame)
	{
		m_dataQueue.push(subPacket);
		return;
	}

	sendData(subPacket);
}

void NetworkSink::Private::sendData(const std::vector<uint8_t> &subPacket)
{
	const char* buffer = (const char*)subPacket.data();
	const size_t bufferSize = subPacket.size();

	try
	{
#if LIBAV_USE_SRT
		SRT_MSGCTRL mctrl;
		srt_msgctrl_init(&mctrl);
		srt_sendmsg2(m_remote_socket, buffer, bufferSize, &mctrl);
#else
		m_socket->send_to(asio::buffer(subPacket), *m_endpoint);
#endif
		m_packetsSent++;
	}
	catch (const std::exception& e)
	{
		AVSLOG(Error) << "NetworkSink: Send failed: " << e.what() << "\n";
	}
}

NetworkSinkCounters NetworkSink::getCounterValues() const
{
	return m_data->m_counters;
}


float NetworkSink::getBandwidthKPerS() const
{
	return m_data->bandwidthKPerS;
}

void NetworkSink::setDebugStream(uint32_t s)
{
	m_data->debugStream = s;
}

void NetworkSink::setDoChecksums(bool s)
{
	m_data->doChecksums = s;
}

void NetworkSink::setDebugNetworkPackets(bool s)
{
	m_data->mDebugNetworkPackets = s;
}

void NetworkSink::setEstimatedDecodingFrequency(uint8_t estimatedDecodingFrequency)
{
	m_data->estimatedDecodingFrequency = estimatedDecodingFrequency;
}
