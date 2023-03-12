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
#if TELEPORT_DATACHANNEL_WEBRTC
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include "TeleportCore/ErrorHandling.h"
#if TELEPORT_CLIENT
#include <libavstream/httputil.hpp>
#endif
#include <ios>

FILE _iob[] = { *stdin, *stdout, *stderr };

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}

#pragma optimize("",off)
using namespace std;
using nlohmann::json;

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration& config,
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
	rtc::InitLogger(rtc::LogLevel::Info);
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
	
	setNumOutputSlots(numOutputs);

	m_streams = std::move(streams);

	for (size_t i = 0; i < numOutputs; ++i)
	{
		const auto& stream = m_streams[i];
		m_streamNodeMap[stream.id] = i;

		m_data->dataChannels.try_emplace(stream.id, stream.id, this);
		DataChannel& dataChannel = m_data->dataChannels[stream.id];
		//rtc::DataChannelInit dataChannelInit;
		//dataChannelInit.id = stream.id;
		//std::string dcLabel = std::to_string(stream.id);
		//dataChannel.rtcDataChannel = m_data->rtcPeerConnection->createDataChannel(dcLabel, dataChannelInit);
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
	m_data->rtcPeerConnection = createPeerConnection(config, std::bind(&WebRtcNetworkSource::SendConfigMessage, this, std::placeholders::_1), std::bind(&WebRtcNetworkSource::Private::onDataChannel, m_data, std::placeholders::_1), "1");
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
		m_counters.httpFilesReceived++;
	}
}

Result WebRtcNetworkSource::deconfigure()
{
	if (getNumOutputSlots() <= 0)
	{
		return Result::Node_NotConfigured;
	}
	setNumOutputSlots(0);

	m_counters = {};
	m_streamNodeMap.clear();
	m_streams.clear();

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

#if TELEPORT_CLIENT
	return m_data->m_httpUtil.process();
#else
	return Result::OK;
#endif
}

NetworkSourceCounters WebRtcNetworkSource::getCounterValues() const
{
	return m_counters;
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
	messagesToSend.push_back(str);
}

bool WebRtcNetworkSource::getNextStreamingControlMessage(std::string& msg)
{
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
		auto &type=it->get<std::string>();
		if (type == "offer")
		{
			auto o = message.find("sdp");
			string sdp= o->get<std::string>();
			receiveOffer(sdp);
		}
		else if (type == "candidate")
		{
			auto c = message.find("candidate");
			string & candidate=c->get<std::string>();
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
	uint16_t id = dc->id().value();
	auto &dataChannel=dataChannels[id];
	dataChannel.rtcDataChannel = dc;
	std::cout << "DataChannel from " << id << " received with label \"" << dc->label() << "\"" << std::endl;

	dc->onOpen([]()
		{
			std::cout << "DataChannel opened" << std::endl;
		});

	dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << std::endl; });

	dc->onMessage([id](auto data) {
		// data holds either std::string or rtc::binary
		if (std::holds_alternative<std::string>(data))
		std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
			<< std::endl;
		else
			std::cout << "Binary message from " << id
			<< " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
		});

	//dataChannelMap.emplace(id, dc);
}
#endif