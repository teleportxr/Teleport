// libavstream
// (c) Copyright 2018-2023 Simul Software Ltd

#include "libavstream/network/webrtc_networksink.h"
#include "network/packetformat.hpp"

#include <iostream>
#include <cmath>
#include <functional>

#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

#include <ElasticFrameProtocol.h>

#pragma optimize("",off)
using namespace std;
using nlohmann::json;

#define EVEN_ID(id) (id-(id%2))
#define ODD_ID(id) (EVEN_ID(id)+1)

namespace avs
{
	struct DataChannel
	{
		DataChannel()
		{
		}
		DataChannel(const DataChannel& dc)
		{
			rtcDataChannel = dc.rtcDataChannel;
		}
		~DataChannel()
		{
			rtcDataChannel->close();
			rtcDataChannel = nullptr;
		}
		shared_ptr<rtc::DataChannel> rtcDataChannel;
		atomic<size_t> bytesReceived = 0;
		bool closed= false;
		bool readyToSend = false;
	};
	// Unused mostly.
	struct WebRtcNetworkSink::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSink, PipelineNode)
		std::unordered_map<uint64_t, DataChannel> dataChannels;
		std::shared_ptr<rtc::PeerConnection> rtcPeerConnection; 
			void onDataChannelReceived(shared_ptr<rtc::DataChannel> dc);
		void onDataChannel(shared_ptr<rtc::DataChannel> dc);
		std::unordered_map<int, uint32_t> idToStreamIndex;
		std::unordered_map<int, int> idToOutputIndex;
		std::unordered_map<uint32_t, std::unique_ptr<StreamParserInterface>> m_parsers;
		std::unique_ptr<ElasticFrameProtocolSender> m_EFPSender;
		bool recreateConnection = false;
		rtc::PeerConnection::State currentState = rtc::PeerConnection::State::New;
		shared_ptr<rtc::PeerConnection> createServerPeerConnection(const rtc::Configuration& config,
			std::function<void(const std::string&)> sendMessage,
			std::function<void(shared_ptr<rtc::DataChannel>)> onDataChannelReceived, std::string id)
		{
			auto pc = std::make_shared<rtc::PeerConnection>(config);

			pc->onStateChange(
				[this](rtc::PeerConnection::State state)
				{
					currentState = state;
					if (state == rtc::PeerConnection::State::Failed|| state == rtc::PeerConnection::State::Closed)
					{
						recreateConnection = true;
					}
					std::cout << "State: " << state << std::endl;
				});

			pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state)
				{
					std::cout << "Gathering State: " << state << std::endl;
				});

			pc->onLocalDescription([sendMessage, id](rtc::Description description)
				{
					// This is our offer.
					json message = { {"id", id},
								{"type", description.typeString()},
								{"sdp",  std::string(description)} };

			sendMessage(message.dump());
				});

			pc->onLocalCandidate([sendMessage, id](rtc::Candidate candidate)
				{
					json message = { {"id", id},
								{"type", "candidate"},
								{"candidate", std::string(candidate)},
								{"mid", candidate.mid()} ,
								{"mlineindex", 0} };

			sendMessage(message.dump());
				});

			pc->onDataChannel([onDataChannelReceived, id](shared_ptr<rtc::DataChannel> dc)
				{
					onDataChannelReceived(dc);
				});
			recreateConnection = false;
			//peerConnectionMap.emplace(id, pc);
			return pc;
		};

	};
}
using namespace avs;

WebRtcNetworkSink::WebRtcNetworkSink()
	: NetworkSink(new WebRtcNetworkSink::Private(this))
{
	rtc::InitLogger(rtc::LogLevel::Warning);
	m_data = static_cast<WebRtcNetworkSink::Private*>(m_d);
}

// configure() is called when we have agreed to connect with a specific client.
Result WebRtcNetworkSink::configure(std::vector<NetworkSinkStream>&& streams, const char* local_bind, uint16_t localPort, const char* remote, uint16_t remotePort, const NetworkSinkParams& params)
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

	m_data->m_EFPSender.reset(new ElasticFrameProtocolSender(65535));
	m_data->m_EFPSender->sendCallback = std::bind(&WebRtcNetworkSink::sendEfpData, this, std::placeholders::_1);
	m_params = params;
	m_streams = std::move(streams);
	size_t numOutputs = 0;
	for (size_t i = 0; i < m_streams.size(); ++i)
	{
		auto& stream = m_streams[i];
		if (stream.canReceive)
		{
			numOutputs++;
		}
	}
	setNumInputSlots(m_streams.size());
	setNumOutputSlots(numOutputs);
	CreatePeerConnection();

	// Having completed the above, we are not yet ready to actually send data.
	// We await the "offer" from webrtc locally and the "candidates" from the STUN server.
	// Then we must send these using the messaging channel (Websockets or Enet) to the client.
	return Result::OK;
}

void WebRtcNetworkSink::CreatePeerConnection()
{
	rtc::Configuration config;
	config.iceServers.emplace_back("stun:stun.stunprotocol.org:3478");
	config.iceServers.emplace_back("stun:stun.l.google.com:19302");
	m_data->rtcPeerConnection = m_data->createServerPeerConnection(config, std::bind(&WebRtcNetworkSink::SendConfigMessage, this, std::placeholders::_1), std::bind(&WebRtcNetworkSink::Private::onDataChannelReceived, m_data, std::placeholders::_1), "1");

	// Now ensure data channels are initialized...

	// Called by the parser interface if the stream uses one
	auto onPacketParsed = [](PipelineNode* node, uint32_t inputNodeIndex, const char* buffer, size_t dataSize, size_t dataOffset, bool isLastPayload)->Result
	{
		WebRtcNetworkSink* ns = static_cast<WebRtcNetworkSink*>(node);
		return ns->packData((const uint8_t*)buffer + dataOffset, dataSize, inputNodeIndex);
	};
	int outputIndex = 0;
	for (size_t i = 0; i < m_streams.size(); ++i)
	{
		auto& stream = m_streams[i];
		stream.buffer.resize(stream.chunkSize);
		m_data->idToStreamIndex.emplace(stream.id, i);

		if (stream.useParser)
		{
			auto parser = std::unique_ptr<StreamParserInterface>(avs::StreamParserInterface::Create(stream.parserType));
			parser->configure(this, onPacketParsed, i);
			m_data->m_parsers[i] = std::move(parser);
		}
		if (stream.canReceive)
		{
		//	m_data->idToOutputIndex[stream.id] = outputIndex++; no, match-up by name.
		}
		m_data->dataChannels.try_emplace(stream.id);
		DataChannel& dataChannel = m_data->dataChannels[stream.id];
		rtc::DataChannelInit dataChannelInit;
		dataChannelInit.reliability.type =  rtc::Reliability::Type::Rexmit;
		dataChannelInit.reliability.unordered = true;
		dataChannelInit.reliability.rexmit = int(0);
		// ensure oddness for the channel creator:
		dataChannelInit.id = ODD_ID(stream.id);
		std::string dcLabel = stream.label;
		dataChannel.rtcDataChannel = m_data->rtcPeerConnection->createDataChannel(dcLabel, dataChannelInit);
		dataChannel.readyToSend = false;
		// We DO NOT get a callback from creating a dc locally.
		m_data->onDataChannel(dataChannel.rtcDataChannel);
	}
}

WebRtcNetworkSink::~WebRtcNetworkSink()
{
	deconfigure();
}

NetworkSinkCounters WebRtcNetworkSink::getCounters() const
{
	std::lock_guard<std::mutex> lock(m_countersMutex);
	return m_counters;
}

void WebRtcNetworkSink::setProcessingEnabled(bool enable)
{
	enabled = enable;
}

bool WebRtcNetworkSink::isProcessingEnabled() const
{
	return enabled ;
}

Result WebRtcNetworkSink::deconfigure()
{
	m_data->m_EFPSender.reset();
	// This should clear out the rtcDataChannel shared_ptrs, so that rtcPeerConnection can destroy them.
	m_data->dataChannels.clear();
	if (m_data->rtcPeerConnection)
	{
		m_data->rtcPeerConnection->close();
		while (m_data->rtcPeerConnection->state() != rtc::PeerConnection::State::Closed)
		{
		}
	}
	m_data->rtcPeerConnection = nullptr;
	m_data->m_parsers.clear();
	m_streams.clear();

	if (getNumInputSlots() <= 0)
		return Result::OK;
	setNumInputSlots(0);
	setNumOutputSlots(0);

	return Result::OK;
}

Result WebRtcNetworkSink::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumInputSlots() == 0 )
	{
		return Result::Node_NotConfigured;
	}
	if (m_data->recreateConnection)
	{
		CreatePeerConnection();
	}
	auto state = m_data->rtcPeerConnection->state();
	if (state != rtc::PeerConnection::State::Connected)
	{
		return Result::Node_NotReady;
	}
	// Called to get the data from the input nodes
	auto readInput = [this](uint32_t inputNodeIndex, size_t& numBytesRead) -> Result
	{
		PipelineNode* node = getInput(inputNodeIndex);
		if (inputNodeIndex >= m_streams.size())
			return Result::Failed;
		auto& stream = m_streams[inputNodeIndex];

		assert(node);
		assert(stream.buffer.size() >= stream.chunkSize);
		static uint64_t frameID = 0;
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
			if (result == Result::OK)
			{
				//std::cout << ".";
				//memcpy(stream.buffer.data(), &streamPayloadInfo, infoSize);
			}

			return result;
		}
		else
		{
			assert(false);
			return Result::Node_Incompatible;
		}
	};
	for (int i = 0; i < (int)getNumInputSlots(); ++i)
	{
		const NetworkSinkStream& stream = m_streams[i];
		// If channel is backed-up in WebRTC, don't grab data off the queue.
		const DataChannel& dataChannel= m_data->dataChannels[stream.id];
		if (!dataChannel.readyToSend)
			continue;
		size_t numBytesRead = 0;
		try
		{
			Result result = readInput(i, numBytesRead);
			if (result != Result::OK)
			{
				if (result != Result::IO_Empty)
				{
					AVSLOG(Error) << "WebRtcNetworkSink: Failed to read from input node: " << i << "\n";
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
		Result res = Result::OK;
		if (stream.useParser && m_data->m_parsers.find(i) != m_data->m_parsers.end())
		{
			res = m_data->m_parsers[i]->parse(((const char*)stream.buffer.data()), numBytesRead);
		}
		else
		{
			res = packData(stream.buffer.data(), numBytesRead, i);
		}
		if (!res)
		{
			return res;
		}
	}

	return Result::OK;
}

Result WebRtcNetworkSink::packData(const uint8_t* buffer, size_t bufferSize, uint32_t inputNodeIndex)
{
	uint32_t code = 0;
	ElasticFrameContent dataContent;

	auto& stream = m_streams[inputNodeIndex];

	switch (stream.dataType)
	{
	case NetworkDataType::Generic:
		dataContent = ElasticFrameContent::unknown;
		break;
	case NetworkDataType::Framed:
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
		AVSLOG(Error) << "WebRtcNetworkSink: Invalid stream data type. Cannot send data. \n";
		return Result::NetworkSink_InvalidStreamDataType;
	}

	// Total number of EFP superframes created since the start for this stream.
	stream.counter++;
	if (dataContent == ElasticFrameContent::unknown)
	{
		// This data will NOT be framed
		sendData(stream.id, buffer, bufferSize);
	}
	else
	{
		auto efpResult = m_data->m_EFPSender->packAndSendFromPtr(
			buffer,
			bufferSize,
			dataContent,
			stream.counter, // pts
			stream.counter, // dts
			code,
			stream.id,
			NO_FLAGS);

		if (efpResult != ElasticFrameMessages::noError)
		{
			AVSLOG(Error) << "WebRtcNetworkSink: An error occured in EFP trying to pack data. \n";
			return Result::NetworkSink_PackingDataFailed;
		}
	}
	return Result::OK;
}

Result WebRtcNetworkSink::sendEfpData(const std::vector<uint8_t>& subPacket)
{
	// streamID is second byte for all EFP packet types
	uint8_t id = subPacket[1];
	return sendData(id,subPacket.data(), subPacket.size());
}

Result WebRtcNetworkSink::sendData(uint8_t id,const uint8_t *packet,size_t sz)
{
	auto index = m_data->idToStreamIndex[id];
	auto &dataChannel = m_data->dataChannels[id];
	auto c = dataChannel.rtcDataChannel;
	if (c)
	{
		try
		{
			if (c->isOpen())
			{
				// From Google's comments - assume the same applies to libdatachannel.
				// Send() sends |data| to the remote peer. If the data can't be sent at the SCTP
				// level (due to congestion control), it's buffered at the data channel level,
				// up to a maximum of 16MB. If Send is called while this buffer is full, the
				// data channel will be closed abruptly.
				//
				// So, it's important to use buffered_amount() and OnBufferedAmountChange to
				// ensure the data channel is used efficiently but without filling this
				// buffer.
				if (c->bufferedAmount() + sz >= 1024 * 1024 * 16)
				{
					dataChannel.readyToSend = false;
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size "<< sz <<" as it would overflow the webrtc buffer.\n";
					return Result::OK;
				}
				// Can't send a buffer greater than 262144. even 64k is dodgy:
				if (sz >= c->maxMessageSize())
				{
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << " as it is too large for a webrtc data channel.\n";
					return Result::OK;
				}
				if (!c->send((std::byte*)packet, sz))
				{
					dataChannel.readyToSend = false;
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << ", buffered amount is " << c->bufferedAmount() << ", available is " << c->availableAmount() << ".\n";
				}
				else if (id == 100)
				{
				//..	std::cout << "WebRTC: channel " << (int)id << ", sent packet of size " << sz << "\n";
				}
			}
			else
			{
				std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << ", channel is closed.\n";
			}
		}
		catch (std::runtime_error err)
		{
			if (err.what())
				std::cerr << err.what() << std::endl;
			else
				std::cerr << "std::runtime_error." << std::endl;
		}
		catch (std::logic_error err)
		{
			if (err.what())
				std::cerr << err.what() << std::endl;
			else
				std::cerr << "std::logic_error." << std::endl;
		}

	}
	return Result::OK;
}


void WebRtcNetworkSink::receiveAnswer(const std::string& sdp)
{
	std::cerr << "WebRtcNetworkSink::receiveAnswer "<<sdp.c_str()<<std::endl;
	try
	{
		rtc::Description rtcDescription(sdp, "offer");
		m_data->rtcPeerConnection->setRemoteDescription(rtcDescription);
	}
	catch (std::logic_error err)
	{
		if (err.what())
			std::cerr << err.what() << std::endl;
		else
			std::cerr << "std::logic_error." << std::endl;
	}
	catch (...)
	{
		std::cerr << "rtcPeerConnection->addRemoteCandidate exception." << std::endl;
	}
}

void WebRtcNetworkSink::receiveCandidate(const std::string& candidate, const std::string& mid,int mlineindex)
{
	std::cerr << "addRemoteCandidate " << candidate.c_str() << std::endl;
	try
	{
		m_data->rtcPeerConnection->addRemoteCandidate(rtc::Candidate(candidate));
	}
	catch (std::logic_error err)
	{
		if (err.what())
			std::cerr << err.what() << std::endl;
		else
			std::cerr << "std::logic_error." << std::endl;
	}
	catch (...)
	{
		std::cerr << "rtcPeerConnection->addRemoteCandidate exception." << std::endl;
	}
}


void WebRtcNetworkSink::SendConfigMessage(const std::string& str)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	messagesToSend.push_back(str);
}


Result WebRtcNetworkSink::onInputLink(int slot, PipelineNode* node)
{
	return Result::OK;
}

Result WebRtcNetworkSink::onOutputLink(int slot, PipelineNode* node)
{
	std::string name = node->getDisplayName();
	for (int i = 0; i < m_streams.size(); i++)
	{
		auto& stream = m_streams[i];
		if (stream.label == name)
			m_data->idToOutputIndex[stream.id]=slot;
	}
	return Result::OK;
}

bool WebRtcNetworkSink::getNextStreamingControlMessage(std::string& msg)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if (!messagesToSend.size())
		return false;
	msg = messagesToSend[0];
	messagesToSend.erase(messagesToSend.begin());
	return true;
}

void WebRtcNetworkSink::receiveStreamingControlMessage(const std::string& msg)
{
	json message = json::parse(msg);
	auto it = message.find("type");
	if (it == message.end())
		return;
	try
	{
		auto type = it->get<std::string>();
		if (type == "answer")
		{
			auto o = message.find("sdp");
			string sdp = o->get<std::string>();
			receiveAnswer(sdp);
		}
		else if (type == "candidate")
		{
			auto c = message.find("candidate");
			string candidate = c->get<std::string>();
			auto m = message.find("mid");
			std::string mid;
			if (m != message.end())
				mid = m->get<std::string>();
			auto l = message.find("mlineindex");
			int mlineindex;
			if (l != message.end())
				mlineindex = l->get<int>();
			receiveCandidate(candidate, mid, mlineindex);
		}
	}
	catch (std::invalid_argument inv)
	{
		if (inv.what())
			std::cerr << inv.what() << std::endl;
		else
			std::cerr << "std::invalid_argument." << std::endl;
	}
	catch (...)
	{
		std::cerr << "receiveStreamingControlMessage exception." << std::endl;
	}
}


void WebRtcNetworkSink::Private::onDataChannelReceived(shared_ptr<rtc::DataChannel> dc)
{
	if (!dc->id().has_value())
		return;
	uint16_t id = (dc->id().value());
	id = EVEN_ID(id);
	std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\"" << std::endl;
}

void WebRtcNetworkSink::Private::onDataChannel(shared_ptr<rtc::DataChannel> dc)
{
	if (!dc->id().has_value())
		return;
	uint16_t id = (dc->id().value());
	id = EVEN_ID(id);
	DataChannel& dataChannel = dataChannels[id];
	dataChannel.rtcDataChannel = dc;
	// Wait for the onOpen callback before sending.
	dataChannel.readyToSend = false;
	std::cout << "DataChannel from " << id << " created with label \"" << dc->label() << "\"" << std::endl;

	dc->onOpen([this, id]()
		{
			DataChannel& dataChannel = dataChannels[id];
			dataChannel.readyToSend = true;
			std::cout << "DataChannel "<<id<<"opened" << std::endl;
		});

	dc->onClosed([this,id]()
		{
			DataChannel& dataChannel = dataChannels[id];
			std::cout << "DataChannel from " << id << " closed" << std::endl;
		//	dataChannel.closed = true;
		});
	dc->onBufferedAmountLow([this, id]()
		{
			DataChannel& dataChannel = dataChannels[id];
			dataChannel.readyToSend = true;
			std::cerr << "WebRTC: channel " << id << ", buffered amount is low.\n";

		});
	auto& stream = q_ptr()->m_streams[idToStreamIndex[id]];
	if(stream.canReceive)
	dc->onMessage([this,  id](rtc::binary b)
		{
			DataChannel& dataChannel = dataChannels[id];
	//	// data holds either std::string or rtc::binary
			//std::cout << "Binary message from " << id	<< " received, size=" << b.size() << std::endl;
			auto o = idToOutputIndex.find(id);
			if (o == idToOutputIndex.end())
				return;
			int outputIndex = o->second;
			dataChannel.bytesReceived += b.size();
			auto& stream = q_ptr()->m_streams[idToStreamIndex[id]];
			if (!stream.framed)
			{
				size_t numBytesWrittenToOutput;
				auto outputNode = dynamic_cast<IOInterface*>(q_ptr()->getOutput(outputIndex));
				if (!outputNode)
				{
					AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
					return;
				}
				auto result = outputNode->write(q_ptr(), (const void*)b.data(), b.size(), numBytesWrittenToOutput);
				if (numBytesWrittenToOutput != b.size())
				{
					AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: failed to write all to output Node.";
					return;
				}
			}
		},[this, &dataChannel,id](rtc::string s) {});

	//dataChannelMap.emplace(id, dc);
}

ConnectionState WebRtcNetworkSink::getConnectionState() const
{
	if (m_data->rtcPeerConnection)
	{
		switch (m_data->currentState)
		{
		case rtc::PeerConnection::State::New:
			return ConnectionState::NEW_UNCONNECTED;
		case rtc::PeerConnection::State::Connecting:
			return ConnectionState::CONNECTING;
		case rtc::PeerConnection::State::Connected:
			return ConnectionState::CONNECTED;
		case rtc::PeerConnection::State::Disconnected:
			return ConnectionState::DISCONNECTED;
		case rtc::PeerConnection::State::Failed:
			return ConnectionState::FAILED;
		case rtc::PeerConnection::State::Closed:
			return ConnectionState::CLOSED;
		default:
			return ConnectionState::ERROR_STATE;
		}
	}
	return ConnectionState::UNINITIALIZED;
}