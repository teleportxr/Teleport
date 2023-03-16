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
			std::cout << "State: " << state << std::endl;
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
		DataChannel(uint64_t stream_index = 0, WebRtcNetworkSource* webRtcNetworkSink = nullptr)
		{
		}
		~DataChannel()
		{}
		shared_ptr<rtc::DataChannel> rtcDataChannel;
	};
	struct WebRtcNetworkSource::Private final : public PipelineNode::Private
	{
		AVSTREAM_PRIVATEINTERFACE(WebRtcNetworkSource, PipelineNode)
		std::unordered_map<uint64_t, DataChannel> dataChannels;
		std::shared_ptr<rtc::PeerConnection> rtcPeerConnection;
		std::unique_ptr<ElasticFrameProtocolReceiver> m_EFPReceiver;
		NetworkSourceCounters m_counters;
		void onDataChannel(shared_ptr<rtc::DataChannel> dc);
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

Result WebRtcNetworkSource::configure(std::vector<NetworkSourceStream>&& streams, const NetworkSourceParams& params)
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

		int nodeIndex = m_streamNodeMap[rPacket->mStreamID];

		auto outputNode = dynamic_cast<Queue*>(getOutput(nodeIndex));
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

	setNumOutputSlots(numOutputs);

	m_streams = std::move(streams);

	for (size_t i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_streams[i];
		int id = EVEN_ID(stream.id);
		m_streamNodeMap[id] = i;

		m_data->dataChannels.try_emplace(id, id, this);
	}

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
	int nodeIndex = m_streamNodeMap[m_params.httpStreamID];

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
	m_data->rtcPeerConnection = nullptr;
	m_streams.clear();
	m_data->m_counters = {};
	m_streamNodeMap.clear();
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
	// update the stream stats.
	for (auto dc : m_data->dataChannels)
	{
		int i=m_streamNodeMap[dc.first];
		streamStatus[i].bandwidthKps=dc
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

	auto &dataChannel=dataChannels[id];
	dataChannel.rtcDataChannel = dc;
	std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\"" << std::endl;

	dc->onOpen([]()
		{
			std::cout << "DataChannel opened" << std::endl;
		});

	dc->onClosed([id]()
		{
			std::cout << "DataChannel from " << id << " closed" << std::endl;
		});

	dc->onMessage([this,id](auto data) {

		// data holds either std::string or rtc::binary
		if (std::holds_alternative<std::string>(data))
		std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
			<< std::endl;
		else
		{
			auto& b = std::get<rtc::binary>(data);
			int nodeIndex = q_ptr()->m_streamNodeMap[id];
			Queue* outputNode = dynamic_cast<Queue*>(q_ptr()->getOutput(nodeIndex));
			//size_t numBytesWrittenToOutput;
			//auto result = outputNode->write(q_ptr(), (const void*)b.data(), b.size(), numBytesWrittenToOutput);

			auto val = m_EFPReceiver->receiveFragmentFromPtr((const uint8_t * )b.data(), b.size(), 0);
			if (val!= ElasticFrameMessages::noError)
			{
				std::cerr<< "EFP Error: Invalid data fragment received" << "\n";
			}
			//std::cout << "Binary message from " << id
			//	<< " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
		}
		});

	//dataChannelMap.emplace(id, dc);
}
