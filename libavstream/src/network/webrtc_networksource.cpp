// libavstream
// (c) Copyright 2018-2022 Simul Software Ltd

#include <iostream>
#include "libavstream/network/webrtc_networksource.h"
#include <ElasticFrameProtocol.h>
#include <libavstream/queue.hpp>
#include <libavstream/timer.hpp>

#ifdef __ANDROID__
#include <pthread.h>
#include <sys/prctl.h>
#endif

#include <functional>

//rtc
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include "TeleportCore/ErrorHandling.h"
#if TELEPORT_CLIENT
#include <libavstream/httputil.hpp>
#endif
#include <ios>
#ifdef _MSC_VER
FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}
#endif
#pragma optimize("",off)
using namespace std;
using nlohmann::json;

#define EVEN_ID(id) (id-(id%2))
#define ODD_ID(id) (EVEN_ID(id)+1)

static shared_ptr<rtc::PeerConnection> createClientPeerConnection(const rtc::Configuration& config,
	std::function<void(const std::string&)> sendMessage,
	std::function<void(shared_ptr<rtc::DataChannel>)> onDataChannel,std::string id)
{
	auto pc = std::make_shared<rtc::PeerConnection>(config);
	
	pc->onStateChange(
		[](rtc::PeerConnection::State state)
		{
			std::cout << "PeerConnection onStateChange to: " << state << std::endl;
		});

	pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state)
		{
			std::cout << "Gathering State: " << state << std::endl;
		});

	pc->onLocalDescription([sendMessage,id](rtc::Description description)
		{
			// This is our answer.
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

	pc->onDataChannel([onDataChannel, id](shared_ptr<rtc::DataChannel> dc)
	{
		onDataChannel(dc);
	});

	//peerConnectionMap.emplace(id, pc);
	return pc;
};
namespace avs
{
	struct DataChannel
	{
		DataChannel() {}
		DataChannel(const DataChannel& dc)
		{
			rtcDataChannel = dc.rtcDataChannel;
		}
		shared_ptr<rtc::DataChannel> rtcDataChannel;
		atomic<size_t> bytesReceived = 0;
		atomic<size_t> bytesSent = 0;
		bool closed = false;
		bool readyToSend = false;
		std::vector<uint8_t> sendBuffer;
	};
	struct WebRtcNetworkSource::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSource, PipelineNode)
		std::vector<DataChannel> dataChannels;
		std::shared_ptr<rtc::PeerConnection> rtcPeerConnection;
		std::unique_ptr<ElasticFrameProtocolReceiver> m_EFPReceiver;
		NetworkSourceCounters m_counters;
		void onDataChannel(shared_ptr<rtc::DataChannel> dc);
		std::unordered_map<uint32_t, uint8_t> idToStreamIndex;
		//! Map input nodes to streams outgoing. Many to one relation.
		std::unordered_map<uint8_t, uint8_t> inputToStreamIndex;
		//! Map input nodes to streams outgoing. One to one relation.
		std::unordered_map<uint8_t, uint8_t> streamIndexToOutput;
		Result sendData(uint8_t id, const uint8_t* packet, size_t sz);
#if TELEPORT_CLIENT
		HTTPUtil m_httpUtil;
#endif
	};
}

using namespace avs;


WebRtcNetworkSource::WebRtcNetworkSource()
	: NetworkSource(new WebRtcNetworkSource::Private(this))
{
	rtc::InitLogger(rtc::LogLevel::Warning);
	m_data = static_cast<WebRtcNetworkSource::Private*>(m_d);
}

Result WebRtcNetworkSource::configure(std::vector<NetworkSourceStream>&& in_streams,int numInputs, const NetworkSourceParams& params)
{
	size_t numOutputs = in_streams.size();
	if (numOutputs == 0 || params.remotePort == 0 || params.remoteHTTPPort == 0)
	{
		return Result::Node_InvalidConfiguration;
	}
	if (!params.remoteIP || !params.remoteIP[0])
	{
		return Result::Node_InvalidConfiguration;
	}
	m_params = params;
	m_data->m_EFPReceiver.reset(new ElasticFrameProtocolReceiver(100, 0, nullptr, ElasticFrameProtocolReceiver::EFPReceiverMode::RUN_TO_COMPLETION));

	m_data->m_EFPReceiver->receiveCallback = [this](ElasticFrameProtocolReceiver::pFramePtr& rPacket, ElasticFrameProtocolContext* pCTX)->void
	{
		if (rPacket->mBroken)
		{
			AVSLOG(Warning) << "Received NAL-units of size: " << unsigned(rPacket->mFrameSize) <<
				" Stream ID: " << unsigned(rPacket->mStreamID) <<
				" PTS: " << unsigned(rPacket->mPts) <<
				" Corrupt: " << rPacket->mBroken <<
				" EFP connection: " << unsigned(rPacket->mSource) << "\n";
			std::lock_guard<std::mutex> guard(m_dataMutex);
			m_data->m_counters.incompleteDecoderPacketsReceived++;
		}
		else
		{
			std::lock_guard<std::mutex> guard(m_dataMutex);
			m_data->m_counters.decoderPacketsReceived++;
		}

		size_t bufferSize = sizeof(StreamPayloadInfo) + rPacket->mFrameSize;
		if (bufferSize > m_tempBuffer.size())
		{
			m_tempBuffer.resize(bufferSize);
		}

		StreamPayloadInfo frameInfo;
		frameInfo.frameID = rPacket->mPts;
		frameInfo.dataSize = rPacket->mFrameSize;
		frameInfo.connectionTime = TimerUtil::GetElapsedTimeS();
		frameInfo.broken = rPacket->mBroken;

		memcpy(m_tempBuffer.data(), &frameInfo, sizeof(StreamPayloadInfo));
		memcpy(&m_tempBuffer[sizeof(StreamPayloadInfo)], rPacket->pFrameData, rPacket->mFrameSize);
		// The mStreamID is encoded sender-side within the EFP wrapper.
		
		uint8_t streamIndex = m_data->idToStreamIndex[rPacket->mStreamID];// streamID is 20,40,60, etc
		uint8_t outputNodeIndex = m_data->streamIndexToOutput[streamIndex];
		auto outputNode = dynamic_cast<Queue*>(getOutput(outputNodeIndex));
		if (!outputNode)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
			return;
		}

		size_t numBytesWrittenToOutput;
		auto result = outputNode->write(m_data->q_ptr(), m_tempBuffer.data(), bufferSize, numBytesWrittenToOutput);

		if (!result)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Failed to write to output node.";
			return;
		}

		if (numBytesWrittenToOutput < bufferSize)
		{
			AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Incomplete frame written to output node.";
		}
	};

	setNumInputSlots(numInputs);
	setNumOutputSlots(numOutputs);

	m_streams = std::move(in_streams);
	streamStatus.resize(m_streams.size());

	// This will map from the stream id's - 20,40,60,80 etc 
	// to the index of outputs, 0,1,2,3,4 etc.
	m_data->dataChannels.resize(m_streams.size());
	m_data->idToStreamIndex.clear();

#if TELEPORT_CLIENT
	HTTPUtilConfig httpUtilConfig;
	httpUtilConfig.remoteIP = params.remoteIP;
	httpUtilConfig.remoteHTTPPort = params.remoteHTTPPort;
	httpUtilConfig.maxConnections = params.maxHTTPConnections;
	httpUtilConfig.useSSL = params.useSSL;
	auto f = std::bind(&WebRtcNetworkSource::receiveHTTPFile, this, std::placeholders::_1, std::placeholders::_2);
	return m_data->m_httpUtil.initialize(httpUtilConfig, std::move(f));
#else
	return Result::OK;
#endif
}

Result WebRtcNetworkSource::onInputLink(int slot, PipelineNode* node)
{
	std::string name=node->getDisplayName();
	for (int i=0;i<m_streams.size();i++)
	{
		auto& stream = m_streams[i];
		if(stream.inputName==name)
			m_data->inputToStreamIndex[slot] = i;
	}
	return Result::OK;
}

Result WebRtcNetworkSource::onOutputLink(int slot, PipelineNode* node)
{
	std::string name = node->getDisplayName();
	for (int i = 0; i < m_streams.size(); i++)
	{
		auto& stream = m_streams[i];
		if (stream.outputName == name)
			m_data->streamIndexToOutput[i] = slot;
	}
	return Result::OK;
}

void WebRtcNetworkSource::receiveOffer(const std::string& offer)
{
	rtc::Description rtcDescription(offer,"offer");
	rtc::Configuration config;
	config.iceServers.emplace_back("stun:stun.stunprotocol.org:3478");
	config.iceServers.emplace_back("stun:stun.l.google.com:19302");
	m_data->rtcPeerConnection = createClientPeerConnection(config, std::bind(&WebRtcNetworkSource::SendConfigMessage, this, std::placeholders::_1), std::bind(&WebRtcNetworkSource::Private::onDataChannel, m_data, std::placeholders::_1), "1");
	m_data->rtcPeerConnection->setRemoteDescription(rtcDescription);
}

void WebRtcNetworkSource::receiveCandidate(const std::string& candidate, const std::string& mid, int mlineindex)
{
	try
	{
		m_data->rtcPeerConnection->addRemoteCandidate(rtc::Candidate(candidate, mid));
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

void WebRtcNetworkSource::receiveHTTPFile(const char* buffer, size_t bufferSize)
{
	uint8_t streamIndex = m_data->idToStreamIndex[m_params.httpStreamID];
	uint8_t nodeIndex = m_data->streamIndexToOutput[streamIndex];

	auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
	if (!outputNode)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Invalid output node. Should be an avs::Queue.";
		return;
	}

	size_t numBytesWrittenToOutput;
	auto result = outputNode->write(this, buffer, bufferSize, numBytesWrittenToOutput);

	if (!result)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Failed to write to output node.";
		return;
	}

	if (numBytesWrittenToOutput < bufferSize)
	{
		AVSLOG(Warning) << "WebRtcNetworkSource HTTP Callback: Incomplete payload written to output node.";
		return;
	}

	{
		std::lock_guard<std::mutex> guard(m_dataMutex);
		m_data->m_counters.httpFilesReceived++;
	}
}

Result WebRtcNetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}
	setNumOutputSlots(0);
	// This should clear out the rtcDataChannel shared_ptrs, so that rtcPeerConnection can destroy them.
	m_data->dataChannels.clear();
	m_data->rtcPeerConnection = nullptr;
	m_streams.clear();
	m_data->m_counters = {};
	m_data->idToStreamIndex.clear();
	m_streams.clear();

	m_data->m_EFPReceiver.reset();
#if TELEPORT_CLIENT
	return m_data->m_httpUtil.shutdown();
#else
	return Result::OK;
#endif
}

Result WebRtcNetworkSource::process(uint64_t timestamp, uint64_t deltaTime)
{
	if (getNumOutputSlots() == 0 )
	{
		return Result::Node_NotConfigured;
	}
	// receiving data from the network is handled elsewhere.
	// Here we only handle receiving data from inputs to be sent onward.

	// Called to get the data from the input nodes
	auto readInput = [this](uint8_t inputNodeIndex, uint8_t streamIndex,
		std::vector<uint8_t> &buffer,size_t& numBytesRead) -> Result
	{
		PipelineNode* node = getInput(inputNodeIndex);
		//if (inputNodeIndex >= m_streams.size())
		//	return Result::Failed;
		auto& stream = m_streams[streamIndex];
		const DataChannel& dataChannel = m_data->dataChannels[stream.id];

		assert(node);
		//assert(buffer.size() >= stream.chunkSize);
		static uint64_t frameID = 0;
		if (IOInterface* nodeIO = dynamic_cast<IOInterface*>(node))
		{
			size_t bufferSize = buffer.size();
			Result result = nodeIO->read(this, buffer.data(), bufferSize, numBytesRead);
			if (result == Result::IO_Retry)
			{
				buffer.resize(bufferSize);
				result = nodeIO->read(this, buffer.data(), bufferSize, numBytesRead);
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
	for (uint32_t i = 0; i < (uint32_t)getNumInputSlots(); ++i)
	{
		uint32_t streamIndex = m_data->inputToStreamIndex[i];
		if (streamIndex >= m_streams.size())
			continue;
		PipelineNode* node = getInput(i);
		if (!node)
			continue;
		const auto& stream = m_streams[streamIndex];
		DataChannel& dataChannel = m_data->dataChannels[streamIndex];
		// If channel is backed-up in WebRTC, don't grab data off the queue.
		if (!dataChannel.readyToSend)
			continue;
		size_t numBytesRead = 0;
		Result result = Result::OK;
		while (result == Result::OK)
		{
			try
			{
				result = readInput(i, streamIndex, dataChannel.sendBuffer, numBytesRead);
				if (result != Result::OK)
				{
					if (result != Result::IO_Empty)
					{
						AVSLOG(Error) << "WebRtcNetworkSink: Failed to read from input node: " << i << "\n";
						break;
					}
				}
			}
			catch (const std::bad_alloc&)
			{
				return Result::IO_OutOfMemory;
			}
			if (numBytesRead == 0)
			{
				break;
			}
			Result res = Result::OK;
			//if (stream.useParser && m_data->m_parsers.find(i) != m_data->m_parsers.end())
			//{
			//res = m_data->m_parsers[i]->parse(((const char*)stream.buffer.data()), numBytesRead);
			//}
			//else
			{
				res = m_data->sendData(stream.id, dataChannel.sendBuffer.data(), numBytesRead);
			}
			if (!res)
			{
				break;
			}
		}
	}
	static float intro = 0.01f;
	// update the stream stats.
	if(deltaTime>0)
	for (int i=0;i<m_data->dataChannels.size();i++)
	{
		DataChannel& dc = m_data->dataChannels[i];
		size_t b = dc.bytesReceived;
		dc.bytesReceived= 0;
		size_t o = dc.bytesSent;
		dc.bytesSent = 0;
		streamStatus[i].inwardBandwidthKps *=1.0f-intro;
		streamStatus[i].inwardBandwidthKps +=intro* (float)b / float(deltaTime)*(1000.0f/1024.0f);
		if (m_streams[i].outgoing)
		{
			streamStatus[i].outwardBandwidthKps *= 1.0f - intro;
			streamStatus[i].outwardBandwidthKps += intro * (float)o / float(deltaTime) * (1000.0f / 1024.0f);
		}
	}
#if TELEPORT_CLIENT
	return m_data->m_httpUtil.process();
#else
	return Result::OK;
#endif
}

NetworkSourceCounters WebRtcNetworkSource::getCounterValues() const
{
	return m_data->m_counters;
}

#ifndef _MSC_VER
__attribute__((optnone))
#endif

#if TELEPORT_CLIENT
std::queue<HTTPPayloadRequest>& WebRtcNetworkSource::GetHTTPRequestQueue()
{
	return m_data->m_httpUtil.GetRequestQueue();
}
#endif

void WebRtcNetworkSource::SendConfigMessage(const std::string& str)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	messagesToSend.push_back(str);
}

bool WebRtcNetworkSource::getNextStreamingControlMessage(std::string& msg)
{
	std::lock_guard<std::mutex> lock(messagesMutex);
	if (!messagesToSend.size())
		return false;
	msg = messagesToSend[0];
	messagesToSend.erase(messagesToSend.begin());
	return true;
}

void WebRtcNetworkSource::receiveStreamingControlMessage(const std::string& str)
{
	json message = json::parse(str);
	auto it = message.find("type");
	if (it == message.end())
		return;
	try
	{
		auto type=it->get<std::string>();
		if (type == "offer")
		{
			auto o = message.find("sdp");
			string sdp= o->get<std::string>();
			receiveOffer(sdp);
		}
		else if (type == "candidate")
		{
			auto c = message.find("candidate");
			string candidate=c->get<std::string>();
			auto m= message.find("mid");
			std::string mid;
			if (m != message.end())
				mid = m->get<std::string>();
			auto l = message.find("mlineindex");
			int mlineindex;
			if (l != message.end())
				mlineindex = l->get<int>();
			receiveCandidate(candidate,mid,mlineindex);
		}
	}
	catch (std::invalid_argument inv)
	{
		if(inv.what())
			std::cerr << inv.what() << std::endl;
		else
			std::cerr << "std::invalid_argument." << std::endl;
	}
	catch (...)
	{
		std::cerr << "receiveStreamingControlMessage exception." << std::endl;
	}
}

void WebRtcNetworkSource::Private::onDataChannel(shared_ptr<rtc::DataChannel> dc)
{
	if (!dc->id().has_value())
		return;
	// make the id even.
	uint16_t id = EVEN_ID(dc->id().value());
	// find the dataChannel whose label matches this channel's label.
	int dcIndex = -1;
	for (int i = 0; i < dataChannels.size(); i++)
	{
		auto& stream = q_ptr()->m_streams[i];
		if (stream.label == dc->label())
		{
			dcIndex = i;
		}
	}
	if (dcIndex < 0)
	{
		// TODO: Inform the server that a channel has not been recognized - don't send data on it.
		return;
	}
	idToStreamIndex[id] = dcIndex;
	DataChannel& dataChannel = dataChannels[dcIndex];
	dataChannel.readyToSend = false;
	dataChannel.rtcDataChannel = dc;
	std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\"" << std::endl;

	dc->onOpen([this,dcIndex]()
		{
			DataChannel& dataChannel = dataChannels[dcIndex];
			dataChannel.readyToSend = true;
			std::cout << "DataChannel opened" << std::endl;
		});
	dc->onBufferedAmountLow([this, dcIndex]()
		{
			DataChannel& dataChannel = dataChannels[dcIndex];
			dataChannel.readyToSend = true;
			std::cout << "DataChannel onBufferedAmountLow" << std::endl;
		});
	dc->onClosed([id]()
		{
			std::cout << "DataChannel from " << id << " closed" << std::endl;
		});
	dc->onMessage([this,&dataChannel,id](rtc::binary b)
		{
		// data holds either std::string or rtc::binary
			uint8_t streamIndex = idToStreamIndex[id];
			auto o = streamIndexToOutput.find(streamIndex);
			if (o == streamIndexToOutput.end())
				return;
			uint8_t outputNodeIndex = o->second;
			dataChannel.bytesReceived += b.size();
			auto & stream=q_ptr()->m_streams[streamIndex];
			if (!stream.framed)
			{
				size_t numBytesWrittenToOutput;
				auto outputNode = dynamic_cast<Queue*>(q_ptr()->getOutput(outputNodeIndex));
				if (!outputNode)
				{
					AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: Invalid output node. Should be an avs::Queue.";
					return;
				}
				auto result = outputNode->write(q_ptr(), (const void*)b.data(), b.size(), numBytesWrittenToOutput);
				if (numBytesWrittenToOutput!=b.size())
				{
					AVSLOG(Warning) << "WebRtcNetworkSource EFP Callback: failed to write all to output Node.";
					return;
				}
			}
			else
			{
				auto val = m_EFPReceiver->receiveFragmentFromPtr((const uint8_t*)b.data(), b.size(), 0);
				if (val != ElasticFrameMessages::noError)
				{
					std::cerr << "EFP Error: Invalid data fragment received" << "\n";
				}
			}
			//std::cout << "Binary message from " << id
			//	<< " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
		},[this, &dataChannel,id](rtc::string s) {});
	//dataChannelMap.emplace(id, dc);
}

Result WebRtcNetworkSource::Private::sendData(uint8_t id, const uint8_t* packet, size_t sz)
{
	auto index = idToStreamIndex[id];
	auto& dataChannel = dataChannels[idToStreamIndex[id]];
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
					std::cerr << "WebRTC: channel " << (int)id << ", failed to send packet of size " << sz << " as it would overflow the webrtc buffer.\n";
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
				dataChannel.bytesSent += sz;
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
